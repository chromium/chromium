// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/web_ui_loading_util.h"

#include <cstdint>
#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_view_util.h"
#include "base/test/task_environment.h"
#include "base/types/expected.h"
#include "mojo/public/c/system/data_pipe.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "net/base/net_errors.h"
#include "net/http/http_byte_range.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace webui {

namespace {

struct RangeTestCase {
  std::optional<std::string> range;
  std::optional<GetRequestedRangeError> error;
};

struct SendDataTestCase {
  std::optional<net::HttpByteRange> range;
  std::string data;
  base::expected<std::string, int> result;
};

class WebUILoadingUtilGetRequestedRangeTest
    : public ::testing::TestWithParam<RangeTestCase> {};
class WebUILoadingUtilSendDataTest
    : public ::testing::TestWithParam<SendDataTestCase> {
 private:
  // For TestURLLoaderClient.
  base::test::TaskEnvironment task_environment_;
};

class BodyReader : public mojo::DataPipeDrainer::Client {
 public:
  explicit BodyReader(mojo::ScopedDataPipeConsumerHandle source)
      : drainer_(this, std::move(source)) {
    run_loop_.Run();
  }

  std::string TakeContent() { return std::move(content_); }

  // mojo::DataPipeDrainer::Client:
  void OnDataAvailable(base::span<const uint8_t> data) override {
    content_.append(base::as_string_view(data));
  }
  void OnDataComplete() override { run_loop_.Quit(); }

 private:
  mojo::DataPipeDrainer drainer_;
  base::RunLoop run_loop_;
  std::string content_;
};

std::optional<std::string> ReadAllData(network::TestURLLoaderClient& client) {
  BodyReader reader(client.response_body_release());
  return reader.TakeContent();
}

}  // namespace

TEST_P(WebUILoadingUtilGetRequestedRangeTest, GetRequestedRange) {
  net::HttpRequestHeaders headers;
  if (GetParam().range) {
    headers.SetHeader(net::HttpRequestHeaders::kRange, *GetParam().range);
  }
  base::expected<net::HttpByteRange, GetRequestedRangeError> result =
      GetRequestedRange(headers);
  if (GetParam().error) {
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), *GetParam().error);
  } else {
    EXPECT_TRUE(result.has_value());
  }
}

INSTANTIATE_TEST_SUITE_P(
    WebUILoadingUtilGetRequestedRangeTest,
    WebUILoadingUtilGetRequestedRangeTest,
    ::testing::Values(
        RangeTestCase(std::nullopt,
                      std::make_optional(GetRequestedRangeError::kNoRanges)),
        RangeTestCase(std::make_optional("bytes=1-9"), std::nullopt),
        RangeTestCase(
            std::make_optional("bytes=1-9,11-19"),
            std::make_optional(GetRequestedRangeError::kMultipleRanges)),
        RangeTestCase(std::make_optional("bytes=3-2"),
                      std::make_optional(GetRequestedRangeError::kParseFailed)),
        RangeTestCase(
            std::make_optional("byt"),
            std::make_optional(GetRequestedRangeError::kParseFailed))));

TEST_P(WebUILoadingUtilSendDataTest, SendData) {
  network::TestURLLoaderClient client;
  auto url_response_head = network::mojom::URLResponseHead::New();
  const scoped_refptr<base::RefCountedString> data =
      base::MakeRefCounted<base::RefCountedString>(GetParam().data);
  const bool success = SendData(std::move(url_response_head),
                                client.CreateRemote(), GetParam().range, data);
  client.RunUntilComplete();
  // If an error is expected, verify the status error code is correct and
  // terminate the test early.
  if (!GetParam().result.has_value()) {
    EXPECT_FALSE(success);
    EXPECT_EQ(GetParam().result.error(), client.completion_status().error_code);
    return;
  }
  // No error expected. Verify response body is correct.
  EXPECT_TRUE(success);
  EXPECT_EQ(net::OK, client.completion_status().error_code);
  ASSERT_TRUE(client.response_body().is_valid());
  std::optional<std::string> response_body = ReadAllData(client);
  ASSERT_TRUE(response_body);
  EXPECT_EQ(GetParam().result.value(), *response_body);
}

INSTANTIATE_TEST_SUITE_P(
    WebUILoadingUtilSendDataTest,
    WebUILoadingUtilSendDataTest,
    ::testing::Values(
        SendDataTestCase(std::nullopt,
                         "this is some data",
                         "this is some data"),
        SendDataTestCase(std::make_optional(net::HttpByteRange::Bounded(3, 8)),
                         "this is some data",
                         "s is s"),
        SendDataTestCase(
            std::make_optional(net::HttpByteRange::Bounded(3, 2)),
            "this is some data",
            base::unexpected(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE))));

}  // namespace webui

}  // namespace content
