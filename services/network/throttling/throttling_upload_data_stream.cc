// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/throttling/throttling_upload_data_stream.h"

#include "base/functional/bind.h"
#include "net/base/net_errors.h"

namespace network {

ThrottlingUploadDataStream::ThrottlingUploadDataStream(
    net::UploadDataStream* upload_data_stream)
    : net::UploadDataStream(upload_data_stream->is_chunked(),
                            upload_data_stream->identifier()),
      throttle_callback_(
          base::BindRepeating(&ThrottlingUploadDataStream::ThrottleCallback,
                              base::Unretained(this))),
      throttled_byte_count_(0),
      upload_data_stream_(upload_data_stream) {}

ThrottlingUploadDataStream::~ThrottlingUploadDataStream() {
  if (interceptor_)
    interceptor_->StopThrottle(throttle_callback_);
}

void ThrottlingUploadDataStream::SetInterceptor(
    ThrottlingNetworkInterceptor* interceptor) {
  DCHECK(!interceptor_);
  if (interceptor)
    interceptor_ = interceptor->GetWeakPtr();
}

bool ThrottlingUploadDataStream::IsInMemory() const {
  return false;
}

int ThrottlingUploadDataStream::InitInternal(
    const net::NetLogWithSource& net_log) {
  throttled_byte_count_ = 0;
  int result = upload_data_stream_->Init(
      base::BindOnce(&ThrottlingUploadDataStream::StreamInitCallback,
                     base::Unretained(this)),
      net_log);
  if (result == net::OK && !is_chunked())
    SetSize(upload_data_stream_->size());
  return result;
}

void ThrottlingUploadDataStream::StreamInitCallback(int result) {
  if (!is_chunked())
    SetSize(upload_data_stream_->size());
  OnInitCompleted(result);
}

int ThrottlingUploadDataStream::ReadInternal(net::IOBuffer* buf, int buf_len) {
  int result = upload_data_stream_->Read(
      buf, buf_len,
      base::BindOnce(&ThrottlingUploadDataStream::StreamReadCallback,
                     base::Unretained(this)));
  return ThrottleRead(result);
}

void ThrottlingUploadDataStream::StreamReadCallback(int result) {
  result = ThrottleRead(result);
  if (result != net::ERR_IO_PENDING) {
    if (result < net::ERR_IO_PENDING) {
      LOG(ERROR) << "StreamReadCallback failed with Error: " << result;
    }
    OnReadCompleted(result);
  }
}

int ThrottlingUploadDataStream::ThrottleRead(int result) {
  if (is_chunked() && upload_data_stream_->IsEOF())
    SetIsFinalChunk();

  if (!interceptor_ || result < 0)
    return result;

  if (result > 0)
    throttled_byte_count_ += result;
  return interceptor_->StartThrottle(result, throttled_byte_count_,
                                     base::TimeTicks(), false, true,
                                     throttle_callback_);
}

void ThrottlingUploadDataStream::ThrottleCallback(int result, int64_t bytes) {
  throttled_byte_count_ = bytes;
  if (result < net::ERR_IO_PENDING) {
    LOG(ERROR) << "ThrottleCallback failed with Error: " << result;
  }
  OnReadCompleted(result);
}

void ThrottlingUploadDataStream::ResetInternal() {
  upload_data_stream_->Reset();
  throttled_byte_count_ = 0;
  if (interceptor_)
    interceptor_->StopThrottle(throttle_callback_);
}

}  // namespace network
