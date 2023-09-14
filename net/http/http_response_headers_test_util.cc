// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_response_headers_test_util.h"

#include "base/strings/strcat.h"
#include "net/http/http_response_headers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net::test {

std::string HttpResponseHeadersToSimpleString(
    const scoped_refptr<HttpResponseHeaders>& parsed) {
  std::string result = parsed->GetStatusLine() + "\n";

  size_t iter = 0;
  std::string name;
  std::string value;
  while (parsed->EnumerateHeaderLines(&iter, &name, &value)) {
    EXPECT_TRUE(name.find('\n') == std::string::npos)
        << "Newline in name is confusing";
    EXPECT_TRUE(name.find(':') == std::string::npos)
        << "Colon in name is ambiguous";
    EXPECT_TRUE(value.find('\n') == std::string::npos)
        << "Newline in value is ambiguous";

    base::StrAppend(&result, {name, ": ", value, "\n"});
  }

  return result;
}

}  // namespace net::test
