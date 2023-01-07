// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/data_url.h"

#include <stddef.h>
#include <stdint.h>

#include <fuzzer/FuzzedDataProvider.h>

#include <string>

#include "base/check_op.h"
#include "base/memory/ref_counted.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "url/gurl.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider provider(data, size);
  std::string method = provider.ConsumeRandomLengthString(256);
  // Don't restrict to data URLs.
  GURL url(provider.ConsumeRemainingBytesAsString());

  std::string mime_type;
  std::string charset;
  std::string body;

  std::string mime_type2;
  std::string charset2;
  std::string body2;
  scoped_refptr<net::HttpResponseHeaders> headers;

  // Run the URL through DataURL::Parse() and DataURL::BuildResponse(). They
  // should succeed and fail in exactly the same cases.
  CHECK_EQ(net::DataURL::Parse(url, &mime_type, &charset, &body),
           net::OK == net::DataURL::BuildResponse(url, method, &mime_type2,
                                                  &charset2, &body2, &headers));
  return 0;
}
