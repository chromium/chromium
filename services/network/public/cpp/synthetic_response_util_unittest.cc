// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/synthetic_response_util.h"

#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "mojo/public/c/system/data_pipe.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

scoped_refptr<net::HttpResponseHeaders> CreateHeaders(
    const std::string& raw_headers) {
  return base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(raw_headers));
}

}  // namespace

class SyntheticResponseTest : public testing::Test {
 public:
  SyntheticResponseTest() = default;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(SyntheticResponseTest, CheckHeaderConsistencyForSyntheticResponse) {
  std::vector<std::string> ignored_headers = {"x-ignored"};
  const struct {
    std::string headers1;
    std::string headers2;
    bool expected_result;
  } kTestCases[] = {
      // Same headers
      {"HTTP/1.1 200 OK\nContent-Type: text/html\n",
       "HTTP/1.1 200 OK\nContent-Type: text/html\n", true},
      // Different headers
      {"HTTP/1.1 200 OK\nContent-Type: text/html\n",
       "HTTP/1.1 200 OK\nContent-Type: text/plain\n", false},
      // Case insensitive
      {"HTTP/1.1 200 OK\nContent-Type: text/html\n",
       "HTTP/1.1 200 OK\ncontent-type: text/html\n", true},
      // Ignored headers
      {"HTTP/1.1 200 OK\nContent-Type: text/html\nx-ignored: foo\n",
       "HTTP/1.1 200 OK\nContent-Type: text/html\nx-ignored: bar\n", true},
      // Duplicate values
      {"HTTP/1.1 200 OK\nSet-Cookie: a=b\nSet-Cookie: c=d\n",
       "HTTP/1.1 200 OK\nSet-Cookie: c=d\nSet-Cookie: a=b\n", true},
      // Missing header
      {"HTTP/1.1 200 OK\nContent-Type: text/html\n", "HTTP/1.1 200 OK\n",
       false},
  };

  for (const auto& test_case : kTestCases) {
    auto headers1 = CreateHeaders(test_case.headers1);
    auto headers2 = CreateHeaders(test_case.headers2);
    EXPECT_EQ(test_case.expected_result,
              CheckHeaderConsistencyForSyntheticResponseForTesting(
                  *headers1, *headers2, ignored_headers))
        << "headers1: " << test_case.headers1
        << "\nheaders2: " << test_case.headers2;
  }
}

TEST_F(SyntheticResponseTest, WriteSyntheticResponseFallbackBody) {
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  ASSERT_EQ(mojo::CreateDataPipe(nullptr, producer_handle, consumer_handle),
            MOJO_RESULT_OK);

  auto [result, written_bytes] =
      WriteSyntheticResponseFallbackBody(producer_handle);
  ASSERT_EQ(result, MOJO_RESULT_OK);
  ASSERT_GE(written_bytes, 0u);

  std::string buffer(written_bytes, '\0');
  size_t bytes_read = written_bytes;
  ASSERT_EQ(consumer_handle->ReadData(MOJO_READ_DATA_FLAG_ALL_OR_NONE,
                                      base::as_writable_byte_span(buffer),
                                      bytes_read),
            MOJO_RESULT_OK);
  EXPECT_EQ(bytes_read, static_cast<size_t>(written_bytes));
  EXPECT_EQ(buffer, "<meta http-equiv=\"refresh\" content=\"0;\" />");
}

}  // namespace network
