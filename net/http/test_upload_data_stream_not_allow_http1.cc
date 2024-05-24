// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/test_upload_data_stream_not_allow_http1.h"

#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace net {

bool UploadDataStreamNotAllowHTTP1::AllowHTTP1() const {
  return false;
}

int UploadDataStreamNotAllowHTTP1::InitInternal(const NetLogWithSource&) {
  return OK;
}

int UploadDataStreamNotAllowHTTP1::ReadInternal(IOBuffer* buf, int buf_len) {
  const size_t bytes_to_read =
      std::min(content_.length(), static_cast<size_t>(buf_len));
  memcpy(buf->data(), content_.c_str(), bytes_to_read);
  content_ = content_.substr(bytes_to_read);
  if (!content_.length())
    SetIsFinalChunk();
  return bytes_to_read;
}

void UploadDataStreamNotAllowHTTP1::ResetInternal() {}

}  // namespace net