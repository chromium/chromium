// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/filter/gzip_source_stream.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "net/base/io_buffer.h"
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
    SourceStream::SourceType type) {
  DCHECK(type == TYPE_GZIP || type == TYPE_DEFLATE);
  auto source =
      base::WrapUnique(new GzipSourceStream(std::move(upstream), type));

  if (!source->Init())
    return nullptr;
  return source;
}

GzipSourceStream::GzipSourceStream(std::unique_ptr<SourceStream> upstream,
                                   SourceStream::SourceType type)
    : FilterSourceStream(type, std::move(upstream)) {}

bool GzipSourceStream::Init() {
  zlib_stream_ = std::make_unique<z_stream>();
  if (!zlib_stream_)
    return false;
  memset(zlib_stream_.get(), 0, sizeof(z_stream));

  int ret;
  if (type() == TYPE_GZIP) {
    ret = inflateInit2(zlib_stream_.get(), -MAX_WBITS);
  } else {
    ret = inflateInit(zlib_stream_.get());
  }
  DCHECK_NE(Z_VERSION_ERROR, ret);
  return ret == Z_OK;
}

std::string GzipSourceStream::GetTypeAsString() const {
  switch (type()) {
    case TYPE_GZIP:
      return kGzip;
    case TYPE_DEFLATE:
      return kDeflate;
    default:
      NOTREACHED_IN_MIGRATION();
      return "";
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
  char* input_data = input_buffer->data();
  size_t input_data_size = input_buffer_size;
  size_t bytes_out = 0;
  bool state_compressed_entered = false;
  while (input_data_size > 0 && bytes_out < output_buffer_size) {
    InputState state = input_state_;
    switch (state) {
      case STATE_START: {
        if (type() == TYPE_DEFLATE) {
          input_state_ = STATE_SNIFFING_DEFLATE_HEADER;
          break;
        }
        DCHECK_GT(input_data_size, 0u);
        input_state_ = STATE_GZIP_HEADER;
        break;
      }
      case STATE_GZIP_HEADER: {
        DCHECK_NE(TYPE_DEFLATE, type());

        const size_t kGzipFooterBytes = 8;
        const char* end = nullptr;
        GZipHeader::Status status =
            gzip_header_.ReadMore(input_data, input_data_size, &end);
        if (status == GZipHeader::INCOMPLETE_HEADER) {
          input_data += input_data_size;
          input_data_size = 0;
        } else if (status == GZipHeader::COMPLETE_HEADER) {
          // If there is a valid header, there should also be a valid footer.
          gzip_footer_bytes_left_ = kGzipFooterBytes;
          size_t bytes_consumed = static_cast<size_t>(end - input_data);
          input_data += bytes_consumed;
          input_data_size -= bytes_consumed;
          input_state_ = STATE_COMPRESSED_BODY;
        } else if (status == GZipHeader::INVALID_HEADER) {
          return base::unexpected(ERR_CONTENT_DECODING_FAILED);
        }
        break;
      }
      case STATE_SNIFFING_DEFLATE_HEADER: {
        DCHECK_EQ(TYPE_DEFLATE, type());

        zlib_stream_.get()->next_in = reinterpret_cast<Bytef*>(input_data);
        zlib_stream_.get()->avail_in = input_data_size;
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

        size_t bytes_used = input_data_size - zlib_stream_.get()->avail_in;
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
          replay_data_.append(input_data, bytes_used);
        }

        input_data_size -= bytes_used;
        input_data += bytes_used;
        break;
      }
      case STATE_REPLAY_DATA: {
        DCHECK_EQ(TYPE_DEFLATE, type());

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
        replay_data_.erase(0, bytes_used);
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
        zlib_stream_.get()->next_in = reinterpret_cast<Bytef*>(input_data);
        zlib_stream_.get()->avail_in = input_data_size;
        zlib_stream_.get()->next_out =
            reinterpret_cast<Bytef*>(output_buffer->data());
        zlib_stream_.get()->avail_out = output_buffer_size;

        int ret = inflate(zlib_stream_.get(), Z_NO_FLUSH);
        if (ret != Z_STREAM_END && ret != Z_OK)
          return base::unexpected(ERR_CONTENT_DECODING_FAILED);

        size_t bytes_used = input_data_size - zlib_stream_.get()->avail_in;
        bytes_out = output_buffer_size - zlib_stream_.get()->avail_out;
        input_data_size -= bytes_used;
        input_data += bytes_used;
        if (ret == Z_STREAM_END)
          input_state_ = STATE_GZIP_FOOTER;
        // zlib has written as much data to |output_buffer| as it could.
        // There might still be some unconsumed data in |input_buffer| if there
        // is no space in |output_buffer|.
        break;
      }
      case STATE_GZIP_FOOTER: {
        size_t to_read = std::min(gzip_footer_bytes_left_, input_data_size);
        gzip_footer_bytes_left_ -= to_read;
        input_data_size -= to_read;
        input_data += to_read;
        if (gzip_footer_bytes_left_ == 0)
          input_state_ = STATE_IGNORING_EXTRA_BYTES;
        break;
      }
      case STATE_IGNORING_EXTRA_BYTES: {
        input_data_size = 0;
        break;
      }
    }
  }
  *consumed_bytes = input_buffer_size - input_data_size;
  return bytes_out;
}

bool GzipSourceStream::InsertZlibHeader() {
  char dummy_header[] = {0x78, 0x01};
  char dummy_output[4];

  inflateReset(zlib_stream_.get());
  zlib_stream_.get()->next_in = reinterpret_cast<Bytef*>(&dummy_header[0]);
  zlib_stream_.get()->avail_in = sizeof(dummy_header);
  zlib_stream_.get()->next_out = reinterpret_cast<Bytef*>(&dummy_output[0]);
  zlib_stream_.get()->avail_out = sizeof(dummy_output);

  int ret = inflate(zlib_stream_.get(), Z_NO_FLUSH);
  return ret == Z_OK;
}

}  // namespace net
