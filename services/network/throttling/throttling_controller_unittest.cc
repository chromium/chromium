// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/throttling/throttling_controller.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "net/base/chunked_upload_data_stream.h"
#include "net/base/completion_repeating_callback.h"
#include "net/http/http_transaction_test_util.h"
#include "net/log/net_log.h"
#include "net/log/net_log_with_source.h"
#include "services/network/throttling/network_conditions.h"
#include "services/network/throttling/scoped_throttling_token.h"
#include "services/network/throttling/throttling_network_interceptor.h"
#include "services/network/throttling/throttling_network_transaction.h"
#include "services/network/throttling/throttling_upload_data_stream.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace network {

using net::kSimpleGET_Transaction;
using net::MockHttpRequest;
using net::MockNetworkLayer;
using net::MockTransaction;
using net::TEST_MODE_SYNC_NET_START;

const char kUploadData[] = "upload_data";
constexpr int64_t kUploadIdentifier = 17;

class TestCallback {
 public:
  TestCallback() : run_count_(0), value_(0) {}
  void Run(int value) {
    run_count_++;
    value_ = value;
  }
  int run_count() { return run_count_; }
  int value() { return value_; }

 private:
  int run_count_;
  int value_;
};

class ThrottlingControllerTestHelper {
 public:
  explicit ThrottlingControllerTestHelper(
      MockTransaction mock_transaction = kSimpleGET_Transaction)
      : completion_callback_(base::BindRepeating(&TestCallback::Run,
                                                 base::Unretained(&callback_))),
        mock_transaction_(mock_transaction, "http://dot.com"),
        buffer_(base::MakeRefCounted<net::IOBufferWithSize>(64)),
        net_log_with_source_(
            net::NetLogWithSource::Make(net::NetLog::Get(),
                                        net::NetLogSourceType::URL_REQUEST)),
        profile_id_(base::UnguessableToken::Create()) {
    mock_transaction_.test_mode = TEST_MODE_SYNC_NET_START;

    auto network_transaction =
        network_layer_.CreateTransaction(net::DEFAULT_PRIORITY);
    CHECK(network_transaction);
    transaction_ = std::make_unique<ThrottlingNetworkTransaction>(
        std::move(network_transaction));
  }
  void SetNetworkState(std::vector<MatchedNetworkConditions> conditions) {
    ThrottlingController::SetConditions(profile_id_, std::move(conditions));
  }

  void SetNetworkState(bool offline, double download, double upload) {
    ThrottlingController::SetConditions(
        profile_id_, {{{}, NetworkConditions{offline, 0, download, upload}}});
  }

  void SetNetworkState(const base::UnguessableToken& id, bool offline) {
    ThrottlingController::SetConditions(id, {{{}, NetworkConditions{offline}}});
  }

  ThrottlingController::ThrottlingProfile* GetThrottlingProfile() {
    auto interceptors =
        ThrottlingController::instance().interceptors_.find(profile_id_);
    if (interceptors == ThrottlingController::instance().interceptors_.end()) {
      return nullptr;
    }
    return &interceptors->second;
  }

  int Start(bool with_upload) {
    request_ = std::make_unique<MockHttpRequest>(mock_transaction_);
    throttling_token_ = ScopedThrottlingToken::MaybeCreate(
        net_log_with_source_.source().id, profile_id_);

    if (with_upload) {
      upload_data_stream_ =
          std::make_unique<net::ChunkedUploadDataStream>(kUploadIdentifier);
      upload_data_stream_->AppendData(
          base::byte_span_with_nul_from_cstring(kUploadData), true);
      request_->upload_data_stream = upload_data_stream_.get();
    }

    int rv = transaction_->Start(request_.get(), completion_callback_,
                                 net_log_with_source_);
    EXPECT_EQ(with_upload, !!transaction_->custom_upload_data_stream_);
    return rv;
  }

  int Read(net::IOBuffer* buffer, int buffer_size) {
    return transaction_->Read(buffer, buffer_size, completion_callback_);
  }

  int Read() { return Read(buffer_.get(), 64); }

  bool ShouldFail() {
    if (transaction_->interceptor_) {
      return transaction_->interceptor_->IsOffline();
    }
    ThrottlingNetworkInterceptor* interceptor =
        ThrottlingController::GetInterceptor(net_log_with_source_.source().id,
                                             GURL());
    EXPECT_TRUE(!!interceptor);
    return interceptor->IsOffline();
  }

  bool HasStarted() { return transaction_->started_; }

  bool HasFailed() { return transaction_->failed_; }

  void CancelTransaction() { transaction_.reset(); }

  int ReadUploadData() {
    EXPECT_EQ(net::OK, transaction_->custom_upload_data_stream_->Init(
                           completion_callback_, net::NetLogWithSource()));
    return transaction_->custom_upload_data_stream_->Read(buffer_.get(), 64,
                                                          completion_callback_);
  }

  ~ThrottlingControllerTestHelper() = default;

  TestCallback* callback() { return &callback_; }
  ThrottlingNetworkTransaction* transaction() { return transaction_.get(); }

  void FastForwardUntilNoTasksRemain() {
    task_environment_.FastForwardUntilNoTasksRemain();
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME};
  MockNetworkLayer network_layer_;
  TestCallback callback_;
  net::CompletionRepeatingCallback completion_callback_;
  net::ScopedMockTransaction mock_transaction_;
  std::unique_ptr<net::ChunkedUploadDataStream> upload_data_stream_;
  std::unique_ptr<ThrottlingNetworkTransaction> transaction_;
  scoped_refptr<net::IOBuffer> buffer_;
  std::unique_ptr<MockHttpRequest> request_;
  std::unique_ptr<network::ScopedThrottlingToken> throttling_token_;
  const net::NetLogWithSource net_log_with_source_;
  const base::UnguessableToken profile_id_;
};

TEST(ThrottlingControllerTest, SingleDisableEnable) {
  ThrottlingControllerTestHelper helper;
  helper.SetNetworkState(false, 0, 0);
  helper.Start(false);

  EXPECT_FALSE(helper.ShouldFail());
  helper.SetNetworkState(true, 0, 0);
  EXPECT_TRUE(helper.ShouldFail());
  helper.SetNetworkState(false, 0, 0);
  EXPECT_FALSE(helper.ShouldFail());

  base::RunLoop().RunUntilIdle();
}

TEST(ThrottlingControllerTest, InterceptorIsolation) {
  const base::UnguessableToken another_profile_id =
      base::UnguessableToken::Create();
  ThrottlingControllerTestHelper helper;
  helper.SetNetworkState(false, 0, 0);
  helper.Start(false);

  EXPECT_FALSE(helper.ShouldFail());
  helper.SetNetworkState(another_profile_id, true);
  EXPECT_FALSE(helper.ShouldFail());
  helper.SetNetworkState(true, 0, 0);
  EXPECT_TRUE(helper.ShouldFail());

  helper.SetNetworkState(another_profile_id, false);
  helper.SetNetworkState(false, 0, 0);
  base::RunLoop().RunUntilIdle();
}

TEST(ThrottlingControllerTest, FailOnStart) {
  ThrottlingControllerTestHelper helper;
  helper.SetNetworkState(true, 0, 0);

  int rv = helper.Start(false);
  EXPECT_EQ(rv, net::ERR_INTERNET_DISCONNECTED);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(helper.callback()->run_count(), 0);
}

TEST(ThrottlingControllerTest, CancelTransaction) {
  ThrottlingControllerTestHelper helper;
  helper.SetNetworkState(false, 0, 0);

  int rv = helper.Start(false);
  EXPECT_EQ(rv, net::OK);
  EXPECT_TRUE(helper.HasStarted());
  helper.CancelTransaction();

  // Should not crash.
  helper.SetNetworkState(true, 0, 0);
  helper.SetNetworkState(false, 0, 0);
  base::RunLoop().RunUntilIdle();
}

TEST(ThrottlingControllerTest, CancelFailedTransaction) {
  ThrottlingControllerTestHelper helper;
  helper.SetNetworkState(true, 0, 0);

  int rv = helper.Start(false);
  EXPECT_EQ(rv, net::ERR_INTERNET_DISCONNECTED);
  EXPECT_TRUE(helper.HasStarted());
  helper.CancelTransaction();

  // Should not crash.
  helper.SetNetworkState(true, 0, 0);
  helper.SetNetworkState(false, 0, 0);
  base::RunLoop().RunUntilIdle();
}

TEST(ThrottlingControllerTest, UploadDoesNotFail) {
  ThrottlingControllerTestHelper helper;
  helper.SetNetworkState(true, 0, 0);
  int rv = helper.Start(true);
  EXPECT_EQ(rv, net::ERR_INTERNET_DISCONNECTED);
  rv = helper.ReadUploadData();
  EXPECT_EQ(rv, static_cast<int>(std::size(kUploadData)));
}

TEST(ThrottlingControllerTest, DownloadOnly) {
  ThrottlingControllerTestHelper helper;
  TestCallback* callback = helper.callback();

  helper.SetNetworkState(false, 10000000, -1);
  int rv = helper.Start(false);
  EXPECT_EQ(rv, net::ERR_IO_PENDING);
  helper.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(callback->run_count(), 1);
  EXPECT_GE(callback->value(), net::OK);

  rv = helper.Read();
  EXPECT_EQ(rv, net::ERR_IO_PENDING);
  EXPECT_EQ(callback->run_count(), 1);
  helper.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(callback->run_count(), 2);
  EXPECT_GE(callback->value(), net::OK);
}

TEST(ThrottlingControllerTest, UploadOnly) {
  ThrottlingControllerTestHelper helper;
  TestCallback* callback = helper.callback();

  helper.SetNetworkState(false, -2, 1000000);
  int rv = helper.Start(true);
  EXPECT_EQ(rv, net::OK);
  helper.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(callback->run_count(), 0);

  rv = helper.Read();
  EXPECT_EQ(rv, net::ERR_IO_PENDING);
  EXPECT_EQ(callback->run_count(), 0);
  helper.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(callback->run_count(), 1);
  EXPECT_GE(callback->value(), net::OK);

  rv = helper.ReadUploadData();
  EXPECT_EQ(rv, net::ERR_IO_PENDING);
  EXPECT_EQ(callback->run_count(), 1);
  helper.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(callback->run_count(), 2);
  EXPECT_EQ(callback->value(), static_cast<int>(std::size(kUploadData)));
}

TEST(ThrottlingControllerTest, DownloadBufferSizeIsNotModifiedIfNotThrottled) {
  MockTransaction mock_transaction = kSimpleGET_Transaction;
  const int kLargeDataSize = 1024 * 1024;
  std::string large_data(kLargeDataSize, 'x');
  mock_transaction.data = large_data.c_str();
  ThrottlingControllerTestHelper helper(mock_transaction);
  TestCallback* callback = helper.callback();

  helper.SetNetworkState(false, 0, 0);
  int rv = helper.Start(false);
  EXPECT_EQ(rv, net::OK);

  auto large_data_buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(kLargeDataSize);
  rv = helper.Read(large_data_buffer.get(), kLargeDataSize);
  EXPECT_EQ(rv, net::ERR_IO_PENDING);
  helper.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(callback->run_count(), 1);
  EXPECT_EQ(callback->value(), kLargeDataSize);
}

TEST(ThrottlingControllerTest, DownloadIsStreamed) {
  MockTransaction mock_transaction = kSimpleGET_Transaction;
  const int kLargeDataSize = 1024 * 1024;
  std::string large_data(kLargeDataSize, 'x');
  mock_transaction.data = large_data.c_str();
  ThrottlingControllerTestHelper helper(mock_transaction);
  TestCallback* callback = helper.callback();

  helper.SetNetworkState(false, 1, 0);
  int rv = helper.Start(false);
  EXPECT_EQ(rv, net::ERR_IO_PENDING);
  helper.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(callback->run_count(), 1);
  EXPECT_GE(callback->value(), net::OK);

  auto large_data_buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(kLargeDataSize);
  helper.Read(large_data_buffer.get(), kLargeDataSize);
  EXPECT_EQ(rv, net::ERR_IO_PENDING);
  EXPECT_EQ(callback->run_count(), 1);
  helper.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(callback->run_count(), 2);
  EXPECT_GT(callback->value(), net::OK);
  EXPECT_LT(callback->value(), kLargeDataSize);
}

TEST(ThrottlingControllerTest, SetConditions) {
  ThrottlingControllerTestHelper helper;
  // Set global conditions
  helper.SetNetworkState({{std::string{}, NetworkConditions{true}}});

  // Test that only global conditions are set
  EXPECT_EQ(helper.GetThrottlingProfile()->matcher_count(), 1u);

  // Set matched conditions
  helper.SetNetworkState({{"http://*/*", NetworkConditions{true}}});

  // Test that only one matched condition is set
  EXPECT_EQ(helper.GetThrottlingProfile()->matcher_count(), 1u);

  // Set both global and local conditions
  helper.SetNetworkState({
      {"http://*/*", NetworkConditions{true}},
      {std::string{}, NetworkConditions{false}},
  });
  EXPECT_EQ(helper.GetThrottlingProfile()->matcher_count(), 2u);

  // Set them the other way around
  helper.SetNetworkState({
      {std::string{}, NetworkConditions{false}},
      {"http://*/*", NetworkConditions{true}},
  });
  EXPECT_EQ(helper.GetThrottlingProfile()->matcher_count(), 2u);

  // Try to set an invalid pattern. The parser accepts a lot of weird inputs,
  // but in some cases fails to parse:
  helper.SetNetworkState({
      {"ht tp://", NetworkConditions{false}},
      {"*.css", NetworkConditions{false}},
  });
  EXPECT_EQ(helper.GetThrottlingProfile()->matcher_count(), 0u);
}

TEST(ThrottlingControllerTest, MultipleGlobalConditions) {
  ThrottlingControllerTestHelper helper;

  // Set multiple global conditions. The first one wins.
  helper.SetNetworkState({
      {std::string{}, NetworkConditions{false, 0.0, 1.0, 0.5}},
      {std::string{}, NetworkConditions{true}},
  });

  auto* interceptor = helper.GetThrottlingProfile()->FindInterceptor(
      GURL("http://example.com"));
  EXPECT_TRUE(interceptor);
  EXPECT_EQ(interceptor->conditions().upload_throughput(), 0.5);
}

TEST(ThrottlingControllerTest, UpdateConditions) {
  ThrottlingControllerTestHelper helper;

  helper.SetNetworkState({
      {"http://*", NetworkConditions{false, 0.0, 1.0, 0.5}},
      {std::string{}, NetworkConditions{false, 0.0, 0.5, 1.0}},
  });

  EXPECT_EQ(helper.GetThrottlingProfile()
                ->FindInterceptor(GURL("http://example.com"))
                ->conditions()
                .upload_throughput(),
            0.5);
  EXPECT_EQ(helper.GetThrottlingProfile()
                ->FindInterceptor(GURL("https://example.com"))
                ->conditions()
                .upload_throughput(),
            1.0);

  // Update conditions for the same patterns.
  helper.SetNetworkState({
      {"http://*", NetworkConditions{false, 0.0, 0.5, 1.0}},
      {std::string{}, NetworkConditions{false, 0.0, 1.0, 0.5}},
  });

  EXPECT_EQ(helper.GetThrottlingProfile()
                ->FindInterceptor(GURL("http://example.com"))
                ->conditions()
                .upload_throughput(),
            1.0);
  EXPECT_EQ(helper.GetThrottlingProfile()
                ->FindInterceptor(GURL("https://example.com"))
                ->conditions()
                .upload_throughput(),
            0.5);
}

TEST(ThrottlingControllerTest, GroupingMatchedConditions) {
  ThrottlingControllerTestHelper helper;

  // Multiple patterns with the same conditions get grouped into a single pipe.
  helper.SetNetworkState({
      {"http://*", NetworkConditions{true}},
      {"https://*", NetworkConditions{true}},
  });

  EXPECT_EQ(helper.GetThrottlingProfile()->matcher_count(), 1u);
  EXPECT_TRUE(helper.GetThrottlingProfile()->FindInterceptor(
      GURL("http://example.com")));
  EXPECT_TRUE(helper.GetThrottlingProfile()->FindInterceptor(
      GURL("https://example.com")));
}

TEST(ThrottlingControllerTest, MultipleMatchedConditions) {
  ThrottlingControllerTestHelper helper;

  // If multiple conditions match a URL, the first one wins.
  helper.SetNetworkState({
      {"http://*", NetworkConditions{false, 0.0, 0.5, 1.0}},
      {"http://example.com", NetworkConditions{false, 0.0, 1.0, 0.5}},
  });
  EXPECT_EQ(helper.GetThrottlingProfile()
                ->FindInterceptor(GURL("http://example.com"))
                ->conditions()
                .upload_throughput(),
            1.0);

  // Flip the order.
  helper.SetNetworkState({
      {"http://example.com", NetworkConditions{false, 0.0, 1.0, 0.5}},
      {"http://*", NetworkConditions{false, 0.0, 0.5, 1.0}},
  });
  EXPECT_EQ(helper.GetThrottlingProfile()
                ->FindInterceptor(GURL("http://example.com"))
                ->conditions()
                .upload_throughput(),
            0.5);

  // Global conditions respect the ordering as well
  helper.SetNetworkState({
      {std::string{}, NetworkConditions{false, 0.0, 0.5, 1.0}},
      {"http://example.com", NetworkConditions{false, 0.0, 1.0, 0.5}},
  });
  EXPECT_EQ(helper.GetThrottlingProfile()
                ->FindInterceptor(GURL("http://example.com"))
                ->conditions()
                .upload_throughput(),
            1.0);
}

}  // namespace network
