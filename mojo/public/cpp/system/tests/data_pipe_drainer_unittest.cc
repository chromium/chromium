// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/system/data_pipe_drainer.h"

#include <string_view>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {

template <typename Functor>
base::RepeatingClosure BindLambda(Functor callable) {
  return base::BindRepeating([](Functor callable) { callable(); }, callable);
}

class DataPipeDrainerTest : public testing::Test,
                            public DataPipeDrainer::Client {
 public:
  DataPipeDrainerTest(const DataPipeDrainerTest&) = delete;
  DataPipeDrainerTest& operator=(const DataPipeDrainerTest&) = delete;

 protected:
  DataPipeDrainerTest() {
    ScopedDataPipeProducerHandle producer_handle;
    ScopedDataPipeConsumerHandle consumer_handle;
    EXPECT_EQ(CreateDataPipe(nullptr, producer_handle, consumer_handle),
              MOJO_RESULT_OK);
    drainer_ =
        std::make_unique<DataPipeDrainer>(this, std::move(consumer_handle));
    producer_handle_ = std::move(producer_handle);
  }

  ScopedDataPipeProducerHandle producer_handle_;
  base::RepeatingClosure completion_callback_;

  void OnDataAvailable(base::span<const uint8_t> data) override {
    data_.append(base::as_string_view(data));
  }

  void OnDataComplete() override { completion_callback_.Run(); }

  base::test::SingleThreadTaskEnvironment task_environment_;
  std::string data_;
  std::unique_ptr<DataPipeDrainer> drainer_;
};

TEST_F(DataPipeDrainerTest, TestCompleteIsCalledOnce) {
  bool had_data_complete = false;

  completion_callback_ = BindLambda([&had_data_complete]() {
    EXPECT_FALSE(had_data_complete);
    had_data_complete = true;
  });
  size_t bytes_written = 0;
  EXPECT_EQ(MOJO_RESULT_OK, producer_handle_->WriteData(
                                base::byte_span_from_cstring("hello"),
                                MOJO_WRITE_DATA_FLAG_NONE, bytes_written));
  EXPECT_EQ(bytes_written, 5u);
  base::RunLoop().RunUntilIdle();
  producer_handle_.reset();
  base::RunLoop().RunUntilIdle();
}

}  // namespace test
}  // namespace mojo
