// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/web_ui_loading_util.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/string_view_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/types/expected.h"
#include "content/common/features.h"
#include "mojo/public/c/system/data_pipe.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "net/base/net_errors.h"
#include "net/http/http_byte_range.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/loading_params.h"
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
// Parameterized over `kWebUISyncDataPipeWrite` so both the synchronous write
// path and the `mojo::DataPipeProducer` path are covered for every case.
class WebUILoadingUtilSendDataTest
    : public ::testing::TestWithParam<std::tuple<bool, SendDataTestCase>> {
 public:
  WebUILoadingUtilSendDataTest() {
    feature_list_.InitWithFeatureState(features::kWebUISyncDataPipeWrite,
                                       std::get<0>(GetParam()));
  }

 protected:
  const SendDataTestCase& test_case() const { return std::get<1>(GetParam()); }

 private:
  base::test::ScopedFeatureList feature_list_;
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
      base::MakeRefCounted<base::RefCountedString>(test_case().data);
  const bool success = SendData(std::move(url_response_head),
                                client.CreateRemote(), test_case().range, data);
  client.RunUntilComplete();
  // If an error is expected, verify the status error code is correct and
  // terminate the test early.
  if (!test_case().result.has_value()) {
    EXPECT_FALSE(success);
    EXPECT_EQ(test_case().result.error(),
              client.completion_status().error_code);
    return;
  }
  // No error expected. Verify response body is correct.
  EXPECT_TRUE(success);
  EXPECT_EQ(net::OK, client.completion_status().error_code);
  ASSERT_TRUE(client.response_body().is_valid());
  std::optional<std::string> response_body = ReadAllData(client);
  ASSERT_TRUE(response_body);
  EXPECT_EQ(test_case().result.value(), *response_body);
}

INSTANTIATE_TEST_SUITE_P(
    WebUILoadingUtilSendDataTest,
    WebUILoadingUtilSendDataTest,
    ::testing::Combine(
        ::testing::Bool(),
        ::testing::Values(
            SendDataTestCase(std::nullopt, "", ""),
            SendDataTestCase(std::nullopt,
                             "this is some data",
                             "this is some data"),
            SendDataTestCase(std::nullopt,
                             std::string(100 * 1024, 'x'),
                             std::string(100 * 1024, 'x')),
            SendDataTestCase(net::HttpByteRange::Bounded(3, 8),
                             "this is some data",
                             "s is s"),
            SendDataTestCase(
                std::make_optional(net::HttpByteRange::Bounded(3, 2)),
                "this is some data",
                base::unexpected(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE)))));

TEST(WebUILoadingUtilTest, GetPipeSyncWriteCompletesImmediately) {
  base::test::ScopedFeatureList feature_list(features::kWebUISyncDataPipeWrite);

  // Payloads larger than the 64 KiB default Mojo data pipe capacity (such as
  // ~82 KiB chrome://theme/colors.css) should be written synchronously into a
  // sized pipe and have their producer handle closed before `GetPipe()`
  // returns, without requiring any `TaskEnvironment` or `ThreadPool` tasks.
  const std::string expected(90 * 1024, 'a');
  const auto data = base::MakeRefCounted<base::RefCountedString>(expected);
  auto [pipe, output_size] = GetPipe(data, std::nullopt);
  ASSERT_TRUE(pipe.is_valid());
  EXPECT_EQ(expected.size(), output_size);

  std::vector<uint8_t> buffer(output_size);
  size_t bytes_read = 0;
  EXPECT_EQ(MOJO_RESULT_OK, pipe->ReadData(MOJO_READ_DATA_FLAG_ALL_OR_NONE,
                                           buffer, bytes_read));
  EXPECT_EQ(output_size, bytes_read);
  EXPECT_EQ(expected, base::as_string_view(buffer));

  // Producer handle was already closed synchronously, so the next read
  // observes EOF (`MOJO_RESULT_FAILED_PRECONDITION`) immediately.
  uint8_t extra_byte = 0;
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            pipe->ReadData(MOJO_READ_DATA_FLAG_NONE,
                           base::byte_span_from_ref(extra_byte), bytes_read));
}

TEST(WebUILoadingUtilTest, GetPipeEmptyPayloadSyncWrite) {
  base::test::ScopedFeatureList feature_list(features::kWebUISyncDataPipeWrite);

  const auto data = base::MakeRefCounted<base::RefCountedString>(std::string());
  auto [pipe, output_size] = GetPipe(data, std::nullopt);
  ASSERT_TRUE(pipe.is_valid());
  EXPECT_EQ(0u, output_size);

  uint8_t extra_byte = 0;
  size_t bytes_read = 0;
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            pipe->ReadData(MOJO_READ_DATA_FLAG_NONE,
                           base::byte_span_from_ref(extra_byte), bytes_read));
}

TEST(WebUILoadingUtilTest, GetPipeSyncWriteAtExactMaxPipeCapacity) {
  base::test::ScopedFeatureList feature_list(features::kWebUISyncDataPipeWrite);

  // A payload exactly equal to the maximum pipe capacity must still be written
  // synchronously (inclusive boundary), without any `TaskEnvironment`.
  const size_t max_pipe_size = network::GetDataPipeDefaultAllocationSize(
      network::DataPipeAllocationSize::kLargerSizeIfPossible);
  const std::string expected(max_pipe_size, 'b');
  const auto data = base::MakeRefCounted<base::RefCountedString>(expected);
  auto [pipe, output_size] = GetPipe(data, std::nullopt);
  ASSERT_TRUE(pipe.is_valid());
  EXPECT_EQ(expected.size(), output_size);

  std::vector<uint8_t> buffer(output_size);
  size_t bytes_read = 0;
  EXPECT_EQ(MOJO_RESULT_OK, pipe->ReadData(MOJO_READ_DATA_FLAG_ALL_OR_NONE,
                                           buffer, bytes_read));
  EXPECT_EQ(output_size, bytes_read);
  EXPECT_EQ(expected, base::as_string_view(buffer));

  uint8_t extra_byte = 0;
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            pipe->ReadData(MOJO_READ_DATA_FLAG_NONE,
                           base::byte_span_from_ref(extra_byte), bytes_read));
}

TEST(WebUILoadingUtilTest, GetPipeSyncWriteRangeSliceOfOversizedResource) {
  base::test::ScopedFeatureList feature_list(features::kWebUISyncDataPipeWrite);

  // Even when `bytes` exceeds the maximum pipe capacity, a bounded range slice
  // that fits in the pipe must clamp `capacity_num_bytes` from `output_size`
  // (not `bytes->size()`) and write the requested subspan synchronously without
  // any `TaskEnvironment`.
  const size_t max_pipe_size = network::GetDataPipeDefaultAllocationSize(
      network::DataPipeAllocationSize::kLargerSizeIfPossible);
  std::string payload(max_pipe_size + 4096, 'x');
  const std::string expected_slice(100, 'r');
  payload.replace(250, expected_slice.size(), expected_slice);
  const auto data =
      base::MakeRefCounted<base::RefCountedString>(std::move(payload));

  net::HttpByteRange range = net::HttpByteRange::Bounded(250, 349);
  ASSERT_TRUE(range.ComputeBounds(data->size()));

  auto [pipe, output_size] = GetPipe(data, range);
  ASSERT_TRUE(pipe.is_valid());
  EXPECT_EQ(expected_slice.size(), output_size);

  std::vector<uint8_t> buffer(output_size);
  size_t bytes_read = 0;
  EXPECT_EQ(MOJO_RESULT_OK, pipe->ReadData(MOJO_READ_DATA_FLAG_ALL_OR_NONE,
                                           buffer, bytes_read));
  EXPECT_EQ(output_size, bytes_read);
  EXPECT_EQ(expected_slice, base::as_string_view(buffer));

  uint8_t extra_byte = 0;
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            pipe->ReadData(MOJO_READ_DATA_FLAG_NONE,
                           base::byte_span_from_ref(extra_byte), bytes_read));
}

TEST(WebUILoadingUtilTest, GetPipeFallsBackWhenExceedingMaxPipeCapacity) {
  base::test::ScopedFeatureList feature_list(features::kWebUISyncDataPipeWrite);
  base::test::TaskEnvironment task_environment;

  const size_t max_pipe_size = network::GetDataPipeDefaultAllocationSize(
      network::DataPipeAllocationSize::kLargerSizeIfPossible);
  const std::string expected(max_pipe_size + 4096, 'z');
  const auto data = base::MakeRefCounted<base::RefCountedString>(expected);
  auto [pipe, output_size] = GetPipe(data, std::nullopt);
  ASSERT_TRUE(pipe.is_valid());
  EXPECT_EQ(expected.size(), output_size);

  BodyReader reader(std::move(pipe));
  EXPECT_EQ(expected, reader.TakeContent());
}

}  // namespace webui

}  // namespace content
