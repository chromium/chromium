// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/http_raw_request_response_info.h"

#include "base/memory/scoped_refptr.h"

namespace network {

HttpRawRequestResponseInfo::HttpRawRequestResponseInfo()
    : http_status_code(0) {}

HttpRawRequestResponseInfo::~HttpRawRequestResponseInfo() {}

scoped_refptr<HttpRawRequestResponseInfo> HttpRawRequestResponseInfo::DeepCopy()
    const {
  auto new_info = base::MakeRefCounted<HttpRawRequestResponseInfo>();
  new_info->http_status_code = http_status_code;
  new_info->http_status_text = http_status_text;
  new_info->request_headers = request_headers;
  new_info->response_headers = response_headers;
  new_info->request_headers_text = request_headers_text;
  new_info->response_headers_text = response_headers_text;
  return new_info;
}

}  // namespace network
