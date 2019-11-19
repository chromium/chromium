// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/system/data_pipe_drainer.h"

#include "base/bind.h"
#include "base/callback.h"
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
 protected:
  DataPipeDrainerTest() {
    DataPipe pipe;
    drainer_ = std::make_unique<DataPipeDrainer>(
        this, std::move(pipe.consumer_handle));
    producer_handle_ = std::move(pipe.producer_handle);
  }

  ScopedDataPipeProducerHandle producer_handle_;
  base::RepeatingClosure completion_callback_;

  void OnDataAvailable(const void* data, size_t num_bytes) override {
    data_.append(static_cast<const char*>(data), num_bytes);
  }

  void OnDataComplete() override { completion_callback_.Run(); }

  base::test::SingleThreadTaskEnvironment task_environment_;
  std::string data_;
  std::unique_ptr<DataPipeDrainer> drainer_;

  DISALLOW_COPY_AND_ASSIGN(DataPipeDrainerTest);
};

TEST_F(DataPipeDrainerTest, TestCompleteIsCalledOnce) {
  bool had_data_complete = false;

  completion_callback_ = BindLambda([&had_data_complete]() {
    EXPECT_FALSE(had_data_complete);
    had_data_complete = true;
  });
  uint32_t size = 5;
  EXPECT_EQ(MOJO_RESULT_OK, producer_handle_->WriteData(
                                "hello", &size, MOJO_WRITE_DATA_FLAG_NONE));
  base::RunLoop().RunUntilIdle();
  producer_handle_.reset();
  base::RunLoop().RunUntilIdle();
}

}  // namespace test
}  // namespace mojo
