// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/filter/gzip_source_stream.h"

#include <stdint.h>

#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "net/base/io_buffer.h"
#include "net/filter/source_stream_type.h"
#include "third_party/zlib/zlib.h"

namespace net {

namespace {

const char kDeflate[] = "DEFLATE";
const char kGzip[] = "GZIP";

// For deflate streams, if more than this many bytes have been received without
// an error and without adding a Zlib header, assume the original stream had a
// Zlib header. In practice, don't need nearly this much data, but since the
// detection logic is a heuristic, best to be safe. Data is freed once it's been
// determined whether the stream has a zlib header or not, so larger values
// shouldn't affect memory usage, in practice.
const int kMaxZlibHeaderSniffBytes = 1000;

}  // namespace

GzipSourceStream::~GzipSourceStream() {
  if (zlib_stream_)
    inflateEnd(zlib_stream_.get());
}

std::unique_ptr<GzipSourceStream> GzipSourceStream::Create(
    std::unique_ptr<SourceStream> upstream,
    SourceStreamType type) {
  DCHECK(type == SourceStreamType::kGzip || type == SourceStreamType::kDeflate);
  auto source =
      base::WrapUnique(new GzipSourceStream(std::move(upstream), type));

  if (!source->Init())
    return nullptr;
  return source;
}

GzipSourceStream::GzipSourceStream(std::unique_ptr<SourceStream> upstream,
                                   SourceStreamType type)
    : FilterSourceStream(type, std::move(upstream)) {}

bool GzipSourceStream::Init() {
  zlib_stream_ = std::make_unique<z_stream>();
  *zlib_stream_ = {};

  int ret;
  if (type() == SourceStreamType::kGzip) {
    ret = inflateInit2(zlib_stream_.get(), -MAX_WBITS);
  } else {
    ret = inflateInit(zlib_stream_.get());
  }
  DCHECK_NE(Z_VERSION_ERROR, ret);
  return ret == Z_OK;
}

std::string GzipSourceStream::GetTypeAsString() const {
  switch (type()) {
    case SourceStreamType::kGzip:
      return kGzip;
    case SourceStreamType::kDeflate:
      return kDeflate;
    default:
      NOTREACHED();
  }
}

base::expected<size_t, Error> GzipSourceStream::FilterData(
    IOBuffer* output_buffer,
    size_t output_buffer_size,
    IOBuffer* input_buffer,
    size_t input_buffer_size,
    size_t* consumed_bytes,
    bool upstream_end_reached) {
  *consumed_bytes = 0;

  // Span that tracks the portion of `input_buffer` that has not yet been
  // process. The data isn't const because the zlib API doesn't use consts for
  // input, but it should not be modified in practice.
  base::span<uint8_t> input_data = input_buffer->first(input_buffer_size);

  size_t bytes_out = 0;
  bool state_compressed_entered = false;
  while (!input_data.empty() && bytes_out < output_buffer_size) {
    InputState state = input_state_;
    switch (state) {
      case STATE_START: {
        if (type() == SourceStreamType::kDeflate) {
          input_state_ = STATE_SNIFFING_DEFLATE_HEADER;
          break;
        }
        DCHECK(!input_data.empty());
        input_state_ = STATE_GZIP_HEADER;
        break;
      }
      case STATE_GZIP_HEADER: {
        DCHECK_NE(SourceStreamType::kDeflate, type());

        const size_t kGzipFooterBytes = 8;
        size_t header_end = 0u;
        GZipHeader::Status status =
            gzip_header_.ReadMore(input_data, header_end);
        if (status == GZipHeader::INCOMPLETE_HEADER) {
          input_data = input_data.subspan(input_data.size());
        } else if (status == GZipHeader::COMPLETE_HEADER) {
          // If there is a valid header, there should also be a valid footer.
          gzip_footer_bytes_left_ = kGzipFooterBytes;
          input_data = input_data.subspan(header_end);
          input_state_ = STATE_COMPRESSED_BODY;
        } else if (status == GZipHeader::INVALID_HEADER) {
          return base::unexpected(ERR_CONTENT_DECODING_FAILED);
        }
        break;
      }
      case STATE_SNIFFING_DEFLATE_HEADER: {
        DCHECK_EQ(SourceStreamType::kDeflate, type());

        zlib_stream_.get()->next_in =
            reinterpret_cast<Bytef*>(input_data.data());
        zlib_stream_.get()->avail_in = input_data.size();
        zlib_stream_.get()->next_out =
            reinterpret_cast<Bytef*>(output_buffer->data());
        zlib_stream_.get()->avail_out = output_buffer_size;

        int ret = inflate(zlib_stream_.get(), Z_NO_FLUSH);

        // On error, try adding a zlib header and replaying the response. Note
        // that data just received doesn't have to be replayed, since it hasn't
        // been removed from input_data yet, only data from previous FilterData
        // calls needs to be replayed.
        if (ret != Z_STREAM_END && ret != Z_OK) {
          if (!InsertZlibHeader())
            return base::unexpected(ERR_CONTENT_DECODING_FAILED);

          input_state_ = STATE_REPLAY_DATA;
          // |replay_state_| should still have its initial value.
          DCHECK_EQ(STATE_COMPRESSED_BODY, replay_state_);
          break;
        }

        size_t bytes_used = input_data.size() - zlib_stream_.get()->avail_in;
        bytes_out = output_buffer_size - zlib_stream_.get()->avail_out;
        // If any bytes are output, enough total bytes have been received, or at
        // the end of the stream, assume the response had a valid Zlib header.
        if (bytes_out > 0 ||
            bytes_used + replay_data_.size() >= kMaxZlibHeaderSniffBytes ||
            ret == Z_STREAM_END) {
          replay_data_.clear();
          if (ret == Z_STREAM_END) {
            input_state_ = STATE_GZIP_FOOTER;
          } else {
            input_state_ = STATE_COMPRESSED_BODY;
          }
        } else {
          replay_data_.insert(replay_data_.end(), input_data.begin(),
                              input_data.begin() + bytes_used);
        }

        input_data = input_data.subspan(bytes_used);
        break;
      }
      case STATE_REPLAY_DATA: {
        DCHECK_EQ(SourceStreamType::kDeflate, type());

        if (replay_data_.empty()) {
          input_state_ = replay_state_;
          break;
        }

        // Call FilterData recursively, after updating |input_state_|, with
        // |replay_data_|. This recursive call makes handling data from
        // |replay_data_| and |input_buffer| much simpler than the alternative
        // operations, though it's not pretty.
        input_state_ = replay_state_;
        size_t bytes_used;
        scoped_refptr<IOBuffer> replay_buffer =
            base::MakeRefCounted<WrappedIOBuffer>(replay_data_);
        base::expected<size_t, Error> result =
            FilterData(output_buffer, output_buffer_size, replay_buffer.get(),
                       replay_data_.size(), &bytes_used, upstream_end_reached);
        replay_data_.erase(replay_data_.begin(),
                           replay_data_.begin() + bytes_used);
        // Back up resulting state, and return state to STATE_REPLAY_DATA.
        replay_state_ = input_state_;
        input_state_ = STATE_REPLAY_DATA;

        // Could continue consuming data in the success case, but simplest not
        // to.
        if (!result.has_value() || result.value() != 0)
          return result;
        break;
      }
      case STATE_COMPRESSED_BODY: {
        DCHECK(!state_compressed_entered);

        state_compressed_entered = true;
        zlib_stream_.get()->next_in =
            reinterpret_cast<Bytef*>(input_data.data());
        zlib_stream_.get()->avail_in = input_data.size();
        zlib_stream_.get()->next_out =
            reinterpret_cast<Bytef*>(output_buffer->data());
        zlib_stream_.get()->avail_out = output_buffer_size;

        int ret = inflate(zlib_stream_.get(), Z_NO_FLUSH);
        if (ret != Z_STREAM_END && ret != Z_OK)
          return base::unexpected(ERR_CONTENT_DECODING_FAILED);

        size_t bytes_used = input_data.size() - zlib_stream_.get()->avail_in;
        bytes_out = output_buffer_size - zlib_stream_.get()->avail_out;
        input_data = input_data.subspan(bytes_used);
        if (ret == Z_STREAM_END)
          input_state_ = STATE_GZIP_FOOTER;
        // zlib has written as much data to |output_buffer| as it could.
        // There might still be some unconsumed data in |input_buffer| if there
        // is no space in |output_buffer|.
        break;
      }
      case STATE_GZIP_FOOTER: {
        size_t to_read = std::min(gzip_footer_bytes_left_, input_data.size());
        gzip_footer_bytes_left_ -= to_read;
        input_data = input_data.subspan(to_read);
        if (gzip_footer_bytes_left_ == 0)
          input_state_ = STATE_IGNORING_EXTRA_BYTES;
        break;
      }
      case STATE_IGNORING_EXTRA_BYTES: {
        input_data = input_data.subspan(input_data.size());
        break;
      }
    }
  }
  *consumed_bytes = input_buffer_size - input_data.size();
  return bytes_out;
}

bool GzipSourceStream::InsertZlibHeader() {
  auto dummy_header = std::to_array<char>({0x78, 0x01});
  std::array<char, 4> dummy_output;

  inflateReset(zlib_stream_.get());
  zlib_stream_.get()->next_in = reinterpret_cast<Bytef*>(&dummy_header[0]);
  zlib_stream_.get()->avail_in = sizeof(dummy_header);
  zlib_stream_.get()->next_out = reinterpret_cast<Bytef*>(&dummy_output[0]);
  zlib_stream_.get()->avail_out = sizeof(dummy_output);

  int ret = inflate(zlib_stream_.get(), Z_NO_FLUSH);
  return ret == Z_OK;
}

}  // namespace net
