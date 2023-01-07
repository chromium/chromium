// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_HTTP_RAW_REQUEST_RESPONSE_INFO_H_
#define SERVICES_NETWORK_PUBLIC_CPP_HTTP_RAW_REQUEST_RESPONSE_INFO_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_split.h"

namespace network {

// Note: when modifying this structure, also update DeepCopy in
// http_raw_request_response_info.cc.
struct COMPONENT_EXPORT(NETWORK_CPP_BASE) HttpRawRequestResponseInfo
    : base::RefCountedThreadSafe<HttpRawRequestResponseInfo> {
  using HeadersVector = base::StringPairs;

  HttpRawRequestResponseInfo();

  scoped_refptr<HttpRawRequestResponseInfo> DeepCopy() const;

  int32_t http_status_code;
  std::string http_status_text;  // Not present in HTTP/2
  HeadersVector request_headers;
  HeadersVector response_headers;
  std::string request_headers_text;
  std::string response_headers_text;

 private:
  friend class base::RefCountedThreadSafe<HttpRawRequestResponseInfo>;
  ~HttpRawRequestResponseInfo();
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_HTTP_RAW_REQUEST_RESPONSE_INFO_H_
