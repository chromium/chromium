// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/filter/filter_source_stream.h"

#include <string_view>
#include <utility>

#include "base/check_op.h"
#include "base/containers/adapters.h"
#include "base/containers/fixed_flat_map.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/trace_event/trace_event.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/trace_constants.h"
#include "net/filter/brotli_source_stream.h"
#include "net/filter/filter_source_stream.h"
#include "net/filter/gzip_source_stream.h"
#include "net/filter/source_stream.h"
#include "net/filter/source_stream_type.h"
#include "net/filter/zstd_source_stream.h"
#include "net/http/http_response_headers.h"

namespace net {

namespace {

constexpr char kDeflate[] = "deflate";
constexpr char kGZip[] = "gzip";
constexpr char kXGZip[] = "x-gzip";
constexpr char kBrotli[] = "br";
constexpr char kZstd[] = "zstd";

const size_t kBufferSize = 32 * 1024;

}  // namespace

FilterSourceStream::FilterSourceStream(SourceStreamType type,
                                       std::unique_ptr<SourceStream> upstream)
    : SourceStream(type), upstream_(std::move(upstream)) {
  DCHECK(upstream_);
}

FilterSourceStream::~FilterSourceStream() = default;

int FilterSourceStream::Read(IOBuffer* read_buffer,
                             int read_buffer_size,
                             CompletionOnceCallback callback) {
  DCHECK_EQ(STATE_NONE, next_state_);
  DCHECK(read_buffer);
  DCHECK_LT(0, read_buffer_size);

  // Allocate a BlockBuffer during first Read().
  if (!input_buffer_) {
    input_buffer_ = base::MakeRefCounted<IOBufferWithSize>(kBufferSize);
    // This is first Read(), start with reading data from |upstream_|.
    next_state_ = STATE_READ_DATA;
  } else {
    // Otherwise start with filtering data, which will tell us whether this
    // stream needs input data.
    next_state_ = STATE_FILTER_DATA;
  }

  output_buffer_ = read_buffer;
  output_buffer_size_ = base::checked_cast<size_t>(read_buffer_size);
  int rv = DoLoop(OK);

  if (rv == ERR_IO_PENDING)
    callback_ = std::move(callback);
  return rv;
}

std::string FilterSourceStream::Description() const {
  std::string next_type_string = upstream_->Description();
  if (next_type_string.empty())
    return GetTypeAsString();
  return next_type_string + "," + GetTypeAsString();
}

bool FilterSourceStream::MayHaveMoreBytes() const {
  return !upstream_end_reached_;
}

SourceStreamType FilterSourceStream::ParseEncodingType(
    std::string_view encoding) {
  std::string lower_encoding = base::ToLowerASCII(encoding);
  static constexpr auto kEncodingMap =
      base::MakeFixedFlatMap<std::string_view, SourceStreamType>({
          {"", SourceStreamType::kNone},
          {kBrotli, SourceStreamType::kBrotli},
          {kDeflate, SourceStreamType::kDeflate},
          {kGZip, SourceStreamType::kGzip},
          {kXGZip, SourceStreamType::kGzip},
          {kZstd, SourceStreamType::kZstd},
      });
  auto encoding_type = kEncodingMap.find(lower_encoding);
  if (encoding_type == kEncodingMap.end()) {
    return SourceStreamType::kUnknown;
  }
  return encoding_type->second;
}

// static
std::vector<SourceStreamType> FilterSourceStream::GetContentEncodingTypes(
    const std::optional<base::flat_set<SourceStreamType>>&
        accepted_stream_types,
    const HttpResponseHeaders& headers) {
  std::vector<SourceStreamType> types;
  size_t iter = 0;
  while (std::optional<std::string_view> type =
             headers.EnumerateHeader(&iter, "Content-Encoding")) {
    SourceStreamType source_type = FilterSourceStream::ParseEncodingType(*type);
    switch (source_type) {
      case SourceStreamType::kBrotli:
      case SourceStreamType::kDeflate:
      case SourceStreamType::kGzip:
      case SourceStreamType::kZstd:
        if (accepted_stream_types &&
            !accepted_stream_types->contains(source_type)) {
          // If the source type is disabled, we treat it
          // in the same way as SourceStreamType::kUnknown.
          return std::vector<SourceStreamType>();
        }
        types.push_back(source_type);
        break;
      case SourceStreamType::kNone:
        // Identity encoding type. Returns an empty vector to pass through raw
        // response body.
        return std::vector<SourceStreamType>();
      case SourceStreamType::kUnknown:
        // Unknown encoding type. Returns an empty vector to pass through raw
        // response body.
        // Request will not be canceled; though
        // it is expected that user will see malformed / garbage response.
        return std::vector<SourceStreamType>();
    }
  }
  return types;
}

// static
std::unique_ptr<SourceStream> FilterSourceStream::CreateDecodingSourceStream(
    std::unique_ptr<SourceStream> upstream,
    const std::vector<SourceStreamType>& types) {
  for (const auto& type : base::Reversed(types)) {
    std::unique_ptr<FilterSourceStream> downstream;
    switch (type) {
      case SourceStreamType::kBrotli:
        downstream = CreateBrotliSourceStream(std::move(upstream));
        break;
      case SourceStreamType::kGzip:
      case SourceStreamType::kDeflate:
        downstream = GzipSourceStream::Create(std::move(upstream), type);
        break;
      case SourceStreamType::kZstd:
        downstream = CreateZstdSourceStream(std::move(upstream));
        break;
      case SourceStreamType::kNone:
      case SourceStreamType::kUnknown:
        NOTREACHED();
    }
    // https://crbug.com/410771958: this can happen when zstd is disabled via
    // disable_zstd_filter (GN arg), but we somehow still received a zstd
    // encoded response.
    if (downstream == nullptr) {
      return nullptr;
    }
    upstream = std::move(downstream);
  }
  return upstream;
}

int FilterSourceStream::DoLoop(int result) {
  DCHECK_NE(STATE_NONE, next_state_);

  int rv = result;
  do {
    State state = next_state_;
    next_state_ = STATE_NONE;
    switch (state) {
      case STATE_READ_DATA:
        rv = DoReadData();
        break;
      case STATE_READ_DATA_COMPLETE:
        rv = DoReadDataComplete(rv);
        break;
      case STATE_FILTER_DATA:
        DCHECK_LE(0, rv);
        rv = DoFilterData();
        break;
      default:
        NOTREACHED() << "bad state: " << state;
    }
  } while (rv != ERR_IO_PENDING && next_state_ != STATE_NONE);
  return rv;
}

int FilterSourceStream::DoReadData() {
  // Read more data means subclasses have consumed all input or this is the
  // first read in which case the |drainable_input_buffer_| is not initialized.
  DCHECK(drainable_input_buffer_ == nullptr ||
         0 == drainable_input_buffer_->BytesRemaining());

  next_state_ = STATE_READ_DATA_COMPLETE;
  // Use base::Unretained here is safe because |this| owns |upstream_|.
  int rv = upstream_->Read(input_buffer_.get(), kBufferSize,
                           base::BindOnce(&FilterSourceStream::OnIOComplete,
                                          base::Unretained(this)));

  return rv;
}

int FilterSourceStream::DoReadDataComplete(int result) {
  DCHECK_NE(ERR_IO_PENDING, result);

  if (result >= OK) {
    drainable_input_buffer_ =
        base::MakeRefCounted<DrainableIOBuffer>(input_buffer_, result);
    next_state_ = STATE_FILTER_DATA;
  }
  if (result <= OK)
    upstream_end_reached_ = true;
  return result;
}

int FilterSourceStream::DoFilterData() {
  DCHECK(output_buffer_);
  DCHECK(drainable_input_buffer_);

  size_t consumed_bytes = 0;
  const int bytes_remaining = drainable_input_buffer_->BytesRemaining();
  TRACE_EVENT_BEGIN2(NetTracingCategory(), "FilterSourceStream::FilterData",
                     "remaining", bytes_remaining, "upstream_end_reached",
                     upstream_end_reached_);
  base::expected<size_t, Error> bytes_output = FilterData(
      output_buffer_.get(), output_buffer_size_, drainable_input_buffer_.get(),
      bytes_remaining, &consumed_bytes, upstream_end_reached_);
  TRACE_EVENT_END2(NetTracingCategory(), "FilterSourceStream::FilterData",
                   "consumed_bytes", consumed_bytes, "output_or_error",
                   bytes_output.has_value()
                       ? base::checked_cast<int>(bytes_output.value())
                       : bytes_output.error());

  if (bytes_output.has_value() && bytes_output.value() == 0) {
    DCHECK_EQ(consumed_bytes, base::checked_cast<size_t>(bytes_remaining));
  } else {
    DCHECK_LE(consumed_bytes, base::checked_cast<size_t>(bytes_remaining));
  }
  // FilterData() is not allowed to return ERR_IO_PENDING.
  if (!bytes_output.has_value())
    DCHECK_NE(ERR_IO_PENDING, bytes_output.error());

  if (consumed_bytes > 0)
    drainable_input_buffer_->DidConsume(consumed_bytes);

  // Received data or encountered an error.
  if (!bytes_output.has_value()) {
    CHECK_LT(bytes_output.error(), 0);
    return bytes_output.error();
  }
  if (bytes_output.value() != 0)
    return base::checked_cast<int>(bytes_output.value());

  // If no data is returned, continue reading if |this| needs more input.
  if (NeedMoreData()) {
    DCHECK_EQ(0, drainable_input_buffer_->BytesRemaining());
    next_state_ = STATE_READ_DATA;
  }
  return 0;
}

void FilterSourceStream::OnIOComplete(int result) {
  DCHECK_EQ(STATE_READ_DATA_COMPLETE, next_state_);

  int rv = DoLoop(result);
  if (rv == ERR_IO_PENDING)
    return;

  output_buffer_ = nullptr;
  output_buffer_size_ = 0;

  std::move(callback_).Run(rv);
}

bool FilterSourceStream::NeedMoreData() const {
  return !upstream_end_reached_;
}

}  // namespace net
