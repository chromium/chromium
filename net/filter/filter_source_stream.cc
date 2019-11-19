// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/filter/filter_source_stream.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace net {

namespace {

const char kDeflate[] = "deflate";
const char kGZip[] = "gzip";
const char kXGZip[] = "x-gzip";
const char kBrotli[] = "br";

const size_t kBufferSize = 32 * 1024;

}  // namespace

FilterSourceStream::FilterSourceStream(SourceType type,
                                       std::unique_ptr<SourceStream> upstream)
    : SourceStream(type),
      upstream_(std::move(upstream)),
      next_state_(STATE_NONE),
      output_buffer_size_(0),
      upstream_end_reached_(false) {
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
  output_buffer_size_ = read_buffer_size;
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

FilterSourceStream::SourceType FilterSourceStream::ParseEncodingType(
    const std::string& encoding) {
  if (encoding.empty()) {
    return TYPE_NONE;
  } else if (base::LowerCaseEqualsASCII(encoding, kBrotli)) {
    return TYPE_BROTLI;
  } else if (base::LowerCaseEqualsASCII(encoding, kDeflate)) {
    return TYPE_DEFLATE;
  } else if (base::LowerCaseEqualsASCII(encoding, kGZip) ||
             base::LowerCaseEqualsASCII(encoding, kXGZip)) {
    return TYPE_GZIP;
  } else {
    return TYPE_UNKNOWN;
  }
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
        rv = ERR_UNEXPECTED;
        break;
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

  int consumed_bytes = 0;
  int bytes_output = FilterData(output_buffer_.get(), output_buffer_size_,
                                drainable_input_buffer_.get(),
                                drainable_input_buffer_->BytesRemaining(),
                                &consumed_bytes, upstream_end_reached_);
  DCHECK_LE(consumed_bytes, drainable_input_buffer_->BytesRemaining());
  DCHECK(bytes_output != 0 ||
         consumed_bytes == drainable_input_buffer_->BytesRemaining());

  // FilterData() is not allowed to return ERR_IO_PENDING.
  DCHECK_NE(ERR_IO_PENDING, bytes_output);

  if (consumed_bytes > 0)
    drainable_input_buffer_->DidConsume(consumed_bytes);

  // Received data or encountered an error.
  if (bytes_output != 0)
    return bytes_output;
  // If no data is returned, continue reading if |this| needs more input.
  if (NeedMoreData()) {
    DCHECK_EQ(0, drainable_input_buffer_->BytesRemaining());
    next_state_ = STATE_READ_DATA;
  }
  return bytes_output;
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
