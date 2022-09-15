// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_THROTTLING_THROTTLING_UPLOAD_DATA_STREAM_H_
#define SERVICES_NETWORK_THROTTLING_THROTTLING_UPLOAD_DATA_STREAM_H_

#include <stdint.h>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "net/base/upload_data_stream.h"
#include "services/network/throttling/throttling_network_interceptor.h"

namespace network {

class ThrottlingNetworkInterceptor;

// ThrottlingUploadData is a wrapper for upload data stream, which proxies
// methods and throttles after the original method succeeds.
class ThrottlingUploadDataStream : public net::UploadDataStream {
 public:
  // Supplied |upload_data_stream| must outlive this object.
  explicit ThrottlingUploadDataStream(
      net::UploadDataStream* upload_data_stream);

  ThrottlingUploadDataStream(const ThrottlingUploadDataStream&) = delete;
  ThrottlingUploadDataStream& operator=(const ThrottlingUploadDataStream&) =
      delete;

  ~ThrottlingUploadDataStream() override;

  void SetInterceptor(ThrottlingNetworkInterceptor* interceptor);

 private:
  // net::UploadDataStream implementation.
  bool IsInMemory() const override;
  int InitInternal(const net::NetLogWithSource& net_log) override;
  int ReadInternal(net::IOBuffer* buf, int buf_len) override;
  void ResetInternal() override;

  void StreamInitCallback(int result);
  void StreamReadCallback(int result);

  int ThrottleRead(int result);
  void ThrottleCallback(int result, int64_t bytes);

  ThrottlingNetworkInterceptor::ThrottleCallback throttle_callback_;
  int64_t throttled_byte_count_;

  raw_ptr<net::UploadDataStream> upload_data_stream_;
  base::WeakPtr<ThrottlingNetworkInterceptor> interceptor_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_THROTTLING_THROTTLING_UPLOAD_DATA_STREAM_H_
