// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_util.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/http_request_headers_mojom_traits.h"
#include "services/network/public/cpp/network_traits_test_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

class NetworkStructTraitsTest : public testing::Test,
                                public mojom::TraitsTestService {
 protected:
  NetworkStructTraitsTest() = default;

  mojo::PendingRemote<mojom::TraitsTestService> GetTraitsTestRemote() {
    mojo::PendingRemote<mojom::TraitsTestService> remote;
    traits_test_receivers_.Add(this, remote.InitWithNewPipeAndPassReceiver());
    return remote;
  }

 private:
  // TraitsTestService:
  void EchoHttpRequestHeaders(
      const net::HttpRequestHeaders& header,
      EchoHttpRequestHeadersCallback callback) override {
    std::move(callback).Run(header);
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  mojo::ReceiverSet<TraitsTestService> traits_test_receivers_;
  DISALLOW_COPY_AND_ASSIGN(NetworkStructTraitsTest);
};

}  // namespace

TEST_F(NetworkStructTraitsTest, HttpRequestHeaders_Basic) {
  net::HttpRequestHeaders headers;
  net::HttpRequestHeaders output;
  headers.SetHeader("Foo", "bar");
  mojo::Remote<mojom::TraitsTestService> remote(GetTraitsTestRemote());
  remote->EchoHttpRequestHeaders(headers, &output);
  std::string value;
  EXPECT_TRUE(output.GetHeader("Foo", &value));
  EXPECT_EQ("bar", value);
  EXPECT_FALSE(output.HasHeader("Fo"));
}

TEST_F(NetworkStructTraitsTest, HttpRequestHeaders_InvalidHeaderName) {
  // Check that non-token chars are disallowed.
  const unsigned char invalid_name_chars[] = {
      0x80, 0x19, '(', ')', '<', '>', '@', ',', ';', ':',
      '\\', '"',  '/', '[', ']', '?', '=', '{', '}'};
  for (char c : invalid_name_chars) {
    std::string invalid_name("foo");
    invalid_name.push_back(c);
    net::HttpRequestHeaders header;
    net::HttpRequestHeaders output;
    header.SetHeaderWithoutCheckForTesting(invalid_name, "foo");
    mojo::Remote<mojom::TraitsTestService> remote(GetTraitsTestRemote());
    remote->EchoHttpRequestHeaders(header, &output);
    std::string value;
    EXPECT_TRUE(output.IsEmpty());
  }
}

// Test cases are copied from HttpUtilTest.IsValidHeaderValue.
TEST_F(NetworkStructTraitsTest, HttpRequestHeaders_InvalidHeaderValue) {
  const char* const invalid_values[] = {
      "X-Requested-With: chrome${NUL}Sec-Unsafe: injected",
      "X-Requested-With: chrome\r\nSec-Unsafe: injected",
      "X-Requested-With: chrome\nSec-Unsafe: injected",
      "X-Requested-With: chrome\rSec-Unsafe: injected",
  };

  for (const std::string& value : invalid_values) {
    std::string replaced = value;
    base::ReplaceSubstringsAfterOffset(&replaced, 0, "${NUL}",
                                       std::string(1, '\0'));
    net::HttpRequestHeaders header;
    net::HttpRequestHeaders output;
    header.SetHeaderWithoutCheckForTesting("Foo", replaced);
    mojo::Remote<mojom::TraitsTestService> remote(GetTraitsTestRemote());
    remote->EchoHttpRequestHeaders(header, &output);
    EXPECT_FALSE(output.HasHeader("Foo"));
  }

  // Check that all characters permitted by RFC7230 3.2.6 are allowed.
  std::string allowed = "\t";
  for (char c = '\x20'; c < '\x7F'; ++c) {
    allowed.append(1, c);
  }
  for (int c = 0x80; c <= 0xFF; ++c) {
    allowed.append(1, static_cast<char>(c));
  }

  net::HttpRequestHeaders header;
  net::HttpRequestHeaders output;
  header.SetHeaderWithoutCheckForTesting("Foo", allowed);
  mojo::Remote<mojom::TraitsTestService> remote(GetTraitsTestRemote());
  remote->EchoHttpRequestHeaders(header, &output);
  EXPECT_TRUE(output.HasHeader("Foo"));
}

}  // namespace network
