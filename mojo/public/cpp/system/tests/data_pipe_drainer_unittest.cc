// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/system/data_pipe_drainer.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/strings/string_view_util.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace {

class TestClient : public DataPipeDrainer::Client {
 public:
  explicit TestClient(ScopedDataPipeConsumerHandle consumer) {
    drainer_.emplace(this, std::move(consumer));
  }

  TestClient(const TestClient&) = delete;
  TestClient& operator=(const TestClient&) = delete;

  bool WaitForData(std::string_view expected_data) {
    while (data_ != expected_data && !is_complete_) {
      data_run_loop_.emplace();
      data_run_loop_->Run();
    }
    return data_ == expected_data;
  }

  void WaitForComplete() { completion_run_loop_.Run(); }

  void OnDataAvailable(base::span<const uint8_t> data) override {
    data_.append(base::as_string_view(data));
    if (data_run_loop_) {
      data_run_loop_->Quit();
    }
    if (data_available_callback_) {
      std::move(data_available_callback_).Run();
    }
  }

  void OnDataComplete() override {
    EXPECT_FALSE(is_complete_);
    is_complete_ = true;
    if (data_run_loop_) {
      data_run_loop_->Quit();
    }
    completion_run_loop_.Quit();
    if (completion_callback_) {
      std::move(completion_callback_).Run();
    }
  }

  void ResetDrainer() { drainer_.reset(); }
  bool has_drainer() const { return drainer_.has_value(); }
  const std::string& data() const { return data_; }

  void set_data_available_callback(base::RepeatingClosure callback) {
    data_available_callback_ = std::move(callback);
  }
  void set_completion_callback(base::RepeatingClosure callback) {
    completion_callback_ = std::move(callback);
  }

 private:
  std::string data_;
  bool is_complete_ = false;
  std::optional<base::RunLoop> data_run_loop_;
  base::RunLoop completion_run_loop_;
  base::RepeatingClosure data_available_callback_;
  base::RepeatingClosure completion_callback_;
  std::optional<DataPipeDrainer> drainer_;
};

class DataPipeDrainerTest : public testing::Test {
 public:
  DataPipeDrainerTest() {
    ScopedDataPipeConsumerHandle consumer_handle;
    EXPECT_EQ(CreateDataPipe(nullptr, producer_handle_, consumer_handle),
              MOJO_RESULT_OK);
    client_ = std::make_unique<TestClient>(std::move(consumer_handle));
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  ScopedDataPipeProducerHandle producer_handle_;
  std::unique_ptr<TestClient> client_;
};

TEST_F(DataPipeDrainerTest, TestCompleteIsCalledOnce) {
  EXPECT_EQ(MOJO_RESULT_OK, producer_handle_->WriteAllData(
                                base::byte_span_from_cstring("hello")));
  EXPECT_TRUE(client_->WaitForData("hello"));

  producer_handle_.reset();
  client_->WaitForComplete();
}

TEST_F(DataPipeDrainerTest, DestroyDrainerOnDataAvailable) {
  client_->set_data_available_callback(
      base::BindLambdaForTesting([&]() { client_->ResetDrainer(); }));
  client_->set_completion_callback(base::BindLambdaForTesting(
      []() { ADD_FAILURE() << "OnDataComplete should not be called."; }));

  EXPECT_EQ(MOJO_RESULT_OK, producer_handle_->WriteAllData(
                                base::byte_span_from_cstring("hello")));

  EXPECT_TRUE(client_->WaitForData("hello"));
  EXPECT_FALSE(client_->has_drainer());
  EXPECT_EQ(
      MOJO_RESULT_FAILED_PRECONDITION,
      producer_handle_->WriteAllData(base::byte_span_from_cstring("world")));
}

TEST_F(DataPipeDrainerTest, DestroyDrainerOnDataAvailableWithQueuedEOF) {
  client_->set_data_available_callback(
      base::BindLambdaForTesting([&]() { client_->ResetDrainer(); }));
  client_->set_completion_callback(base::BindLambdaForTesting(
      []() { ADD_FAILURE() << "OnDataComplete should not be called."; }));

  EXPECT_EQ(MOJO_RESULT_OK, producer_handle_->WriteAllData(
                                base::byte_span_from_cstring("hello")));
  producer_handle_.reset();

  EXPECT_TRUE(client_->WaitForData("hello"));
  EXPECT_FALSE(client_->has_drainer());
}

TEST_F(DataPipeDrainerTest, DestroyDrainerOnDataComplete) {
  client_->set_completion_callback(
      base::BindLambdaForTesting([&]() { client_->ResetDrainer(); }));

  EXPECT_EQ(MOJO_RESULT_OK, producer_handle_->WriteAllData(
                                base::byte_span_from_cstring("hello")));
  producer_handle_.reset();
  client_->WaitForComplete();

  EXPECT_EQ(client_->data(), "hello");
  EXPECT_FALSE(client_->has_drainer());
}

TEST_F(DataPipeDrainerTest, DestroyClientOnDataAvailable) {
  base::RunLoop run_loop;
  std::string captured_data;
  client_->set_data_available_callback(base::BindLambdaForTesting([&]() {
    captured_data = client_->data();
    client_.reset();
    run_loop.Quit();
  }));
  client_->set_completion_callback(base::BindLambdaForTesting(
      []() { ADD_FAILURE() << "OnDataComplete should not be called."; }));

  EXPECT_EQ(MOJO_RESULT_OK, producer_handle_->WriteAllData(
                                base::byte_span_from_cstring("hello")));
  producer_handle_.reset();
  run_loop.Run();

  EXPECT_EQ(captured_data, "hello");
  EXPECT_EQ(client_, nullptr);
}

TEST_F(DataPipeDrainerTest, DestroyClientOnDataComplete) {
  base::RunLoop run_loop;
  client_->set_completion_callback(base::BindLambdaForTesting([&]() {
    EXPECT_EQ(client_->data(), "hello");
    client_.reset();
    run_loop.Quit();
  }));

  EXPECT_EQ(MOJO_RESULT_OK, producer_handle_->WriteAllData(
                                base::byte_span_from_cstring("hello")));
  producer_handle_.reset();
  run_loop.Run();

  EXPECT_EQ(client_, nullptr);
}

}  // namespace
}  // namespace mojo
