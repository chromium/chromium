// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_TEST_UPLOAD_DATA_STREAM_NOT_ALLOW_HTTP1_H_
#define NET_HTTP_TEST_UPLOAD_DATA_STREAM_NOT_ALLOW_HTTP1_H_

#include "net/base/upload_data_stream.h"

namespace net {

// UploadDataStreamNotAllowHTTP1 simply disallows HTTP/1 and uploads content.
class UploadDataStreamNotAllowHTTP1 : public UploadDataStream {
 public:
  explicit UploadDataStreamNotAllowHTTP1(const std::string& content)
      : UploadDataStream(true, 0), content_(content) {}
  UploadDataStreamNotAllowHTTP1(const UploadDataStreamNotAllowHTTP1&) = delete;
  UploadDataStreamNotAllowHTTP1& operator=(
      const UploadDataStreamNotAllowHTTP1&) = delete;

  bool AllowHTTP1() const override;

 private:
  int InitInternal(const NetLogWithSource& net_log) override;
  int ReadInternal(IOBuffer* buf, int buf_len) override;
  void ResetInternal() override;

  std::string content_;
};

}  // namespace net

#endif  // NET_HTTP_TEST_UPLOAD_DATA_STREAM_NOT_ALLOW_HTTP1_H_