// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_headers_test_util.h"
#include "net/http/http_util.h"
#include "services/network/public/mojom/http_response_headers.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {

TEST(NetworkParamTraitsTest, HttpResponseHeaders) {
  auto in = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders("HTTP/1.1 200 OK\n"
                                        "Content-Type: text/plain\n"
                                        "Content-Length: 10\n"
                                        "Set-Cookie: foo=bar; httponly\n"
                                        "Set-Cookie: bar=foo\n"
                                        "X-Test-Header: test\n"
                                        "Set-Cookie2: bar2=foo2\n"));
  scoped_refptr<net::HttpResponseHeaders> out;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<network::mojom::HttpResponseHeaders>(
          in, out));

  // Cookie headers should be dropped and not serialized over IPC.
  EXPECT_EQ(
      "HTTP/1.1 200 OK\n"
      "Content-Type: text/plain\n"
      "Content-Length: 10\n"
      "X-Test-Header: test\n",
      net::test::HttpResponseHeadersToSimpleString(out));
}

}  // namespace mojo
