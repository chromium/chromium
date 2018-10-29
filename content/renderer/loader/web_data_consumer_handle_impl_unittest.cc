// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/loader/web_data_consumer_handle_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"

namespace content {

namespace {

using blink::WebDataConsumerHandle;

class ReadDataOperationBase {
 public:
  virtual ~ReadDataOperationBase() {}
  virtual void ReadMore() = 0;

  static const WebDataConsumerHandle::Flags kNone =
      WebDataConsumerHandle::kFlagNone;
  static const WebDataConsumerHandle::Result kOk = WebDataConsumerHandle::kOk;
  static const WebDataConsumerHandle::Result kDone =
      WebDataConsumerHandle::kDone;
  static const WebDataConsumerHandle::Result kShouldWait =
      WebDataConsumerHandle::kShouldWait;
};

class ClientImpl final : public WebDataConsumerHandle::Client {
 public:
  explicit ClientImpl(ReadDataOperationBase* operation)
      : operation_(operation) {}

  void DidGetReadable() override {
    blink::scheduler::GetSingleThreadTaskRunnerForTesting()->PostTask(
        FROM_HERE, base::BindOnce(&ReadDataOperationBase::ReadMore,
                                  base::Unretained(operation_)));
  }

 private:
  ReadDataOperationBase* operation_;
};

class ReadDataOperation : public ReadDataOperationBase {
 public:
  typedef WebDataConsumerHandle::Result Result;
  ReadDataOperation(
      mojo::ScopedDataPipeConsumerHandle handle,
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
      const base::Closure& on_done)
      : handle_(new WebDataConsumerHandleImpl(std::move(handle))),
        main_thread_task_runner_(std::move(main_thread_task_runner)),
        on_done_(on_done) {}

  const std::string& result() const { return result_; }

  void ReadMore() override {
    // We may have drained the pipe while this task was waiting to run.
    if (reader_)
      ReadData();
  }

  void ReadData() {
    if (!client_) {
      client_.reset(new ClientImpl(this));
      reader_ = handle_->ObtainReader(
          client_.get(),
          blink::scheduler::GetSingleThreadTaskRunnerForTesting());
    }

    Result rv = kOk;
    size_t readSize = 0;
    while (true) {
      char buffer[16];
      rv = reader_->Read(&buffer, sizeof(buffer), kNone, &readSize);
      if (rv != kOk)
        break;
      result_.insert(result_.size(), &buffer[0], readSize);
    }

    if (rv == kShouldWait) {
      // Wait a while...
      return;
    }

    if (rv != kDone) {
      // Something is wrong.
      result_ = "error";
    }

    // The operation is done.
    reader_.reset();
    main_thread_task_runner_->PostTask(FROM_HERE, on_done_);
  }

 private:
  std::unique_ptr<WebDataConsumerHandleImpl> handle_;
  std::unique_ptr<WebDataConsumerHandle::Reader> reader_;
  std::unique_ptr<WebDataConsumerHandle::Client> client_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  base::Closure on_done_;
  std::string result_;
};

class TwoPhaseReadDataOperation : public ReadDataOperationBase {
 public:
  typedef WebDataConsumerHandle::Result Result;
  TwoPhaseReadDataOperation(
      mojo::ScopedDataPipeConsumerHandle handle,
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
      const base::Closure& on_done)
      : handle_(new WebDataConsumerHandleImpl(std::move(handle))),
        main_thread_task_runner_(std::move(main_thread_task_runner)),
        on_done_(on_done) {}

  const std::string& result() const { return result_; }

  void ReadMore() override {
    // We may have drained the pipe while this task was waiting to run.
    if (reader_)
      ReadData();
  }

  void ReadData() {
    if (!client_) {
      client_.reset(new ClientImpl(this));
      reader_ = handle_->ObtainReader(
          client_.get(),
          blink::scheduler::GetSingleThreadTaskRunnerForTesting());
    }

    Result rv;
    while (true) {
      const void* buffer = nullptr;
      size_t size;
      rv = reader_->BeginRead(&buffer, kNone, &size);
      if (rv != kOk)
        break;
      // In order to verify endRead, we read at most one byte for each time.
      size_t read_size = std::max(static_cast<size_t>(1), size);
      result_.insert(result_.size(), static_cast<const char*>(buffer),
                     read_size);
      rv = reader_->EndRead(read_size);
      if (rv != kOk) {
        // Something is wrong.
        result_ = "error";
        main_thread_task_runner_->PostTask(FROM_HERE, on_done_);
        return;
      }
    }

    if (rv == kShouldWait) {
      // Wait a while...
      return;
    }

    if (rv != kDone) {
      // Something is wrong.
      result_ = "error";
    }

    // The operation is done.
    reader_.reset();
    main_thread_task_runner_->PostTask(FROM_HERE, on_done_);
  }

 private:
  std::unique_ptr<WebDataConsumerHandleImpl> handle_;
  std::unique_ptr<WebDataConsumerHandle::Reader> reader_;
  std::unique_ptr<WebDataConsumerHandle::Client> client_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  base::Closure on_done_;
  std::string result_;
};

class WebDataConsumerHandleImplTest : public ::testing::Test {
 public:
  typedef WebDataConsumerHandle::Result Result;

  void SetUp() override {
    MojoCreateDataPipeOptions options;
    options.struct_size = sizeof(MojoCreateDataPipeOptions);
    options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
    options.element_num_bytes = 1;
    options.capacity_num_bytes = kDataPipeCapacity;

    MojoResult result = mojo::CreateDataPipe(&options, &producer_, &consumer_);
    ASSERT_EQ(MOJO_RESULT_OK, result);
  }

 protected:
  static constexpr int kDataPipeCapacity = 4;

  // This function can be blocked if the associated consumer doesn't consume
  // the data.
  std::string ProduceData(size_t total_size) {
    int index = 0;
    std::string expected;
    for (size_t i = 0; i < total_size; ++i) {
      expected += static_cast<char>(index + 'a');
      index = (37 * index + 11) % 26;
    }

    const char* p = expected.data();
    size_t remaining = total_size;
    const MojoWriteDataFlags kNone = MOJO_WRITE_DATA_FLAG_NONE;
    MojoResult rv;
    while (remaining > 0) {
      uint32_t size = remaining;
      rv = producer_->WriteData(p, &size, kNone);
      if (rv == MOJO_RESULT_OK) {
        remaining -= size;
        p += size;
      } else if (rv != MOJO_RESULT_SHOULD_WAIT) {
        // Something is wrong.
        EXPECT_TRUE(false) << "WriteData() returns an invalid value.";
        return "error on writing";
      }
    }
    return expected;
  }

  base::test::ScopedTaskEnvironment task_environment_;

  mojo::ScopedDataPipeProducerHandle producer_;
  mojo::ScopedDataPipeConsumerHandle consumer_;
};

TEST_F(WebDataConsumerHandleImplTest, ReadData) {
  base::RunLoop run_loop;
  auto operation = std::make_unique<ReadDataOperation>(
      std::move(consumer_),
      blink::scheduler::GetSingleThreadTaskRunnerForTesting(),
      run_loop.QuitClosure());

  base::Thread t("DataConsumerHandle test thread");
  ASSERT_TRUE(t.Start());

  t.task_runner()->PostTask(FROM_HERE,
                            base::BindOnce(&ReadDataOperation::ReadData,
                                           base::Unretained(operation.get())));

  std::string expected = ProduceData(24 * 1024);
  producer_.reset();

  run_loop.Run();
  t.Stop();

  EXPECT_EQ(expected, operation->result());
}

TEST_F(WebDataConsumerHandleImplTest, TwoPhaseReadData) {
  base::RunLoop run_loop;
  auto operation = std::make_unique<TwoPhaseReadDataOperation>(
      std::move(consumer_),
      blink::scheduler::GetSingleThreadTaskRunnerForTesting(),
      run_loop.QuitClosure());

  base::Thread t("DataConsumerHandle test thread");
  ASSERT_TRUE(t.Start());

  t.task_runner()->PostTask(FROM_HERE,
                            base::BindOnce(&TwoPhaseReadDataOperation::ReadData,
                                           base::Unretained(operation.get())));

  std::string expected = ProduceData(24 * 1024);
  producer_.reset();

  run_loop.Run();
  t.Stop();

  EXPECT_EQ(expected, operation->result());
}

TEST_F(WebDataConsumerHandleImplTest, ZeroSizeRead) {
  ASSERT_GT(kDataPipeCapacity - 1, 0);
  constexpr size_t data_size = kDataPipeCapacity - 1;
  std::unique_ptr<WebDataConsumerHandleImpl> handle(
      new WebDataConsumerHandleImpl(std::move(consumer_)));
  std::unique_ptr<WebDataConsumerHandle::Reader> reader(handle->ObtainReader(
      nullptr, blink::scheduler::GetSingleThreadTaskRunnerForTesting()));

  size_t read_size;
  WebDataConsumerHandle::Result rv =
      reader->Read(nullptr, 0, WebDataConsumerHandle::kFlagNone, &read_size);
  EXPECT_EQ(WebDataConsumerHandle::Result::kShouldWait, rv);

  std::string expected = ProduceData(data_size);
  producer_.reset();

  rv = reader->Read(nullptr, 0, WebDataConsumerHandle::kFlagNone, &read_size);
  EXPECT_EQ(WebDataConsumerHandle::Result::kOk, rv);

  char buffer[16];
  rv = reader->Read(&buffer, sizeof(buffer), WebDataConsumerHandle::kFlagNone,
                    &read_size);
  EXPECT_EQ(WebDataConsumerHandle::Result::kOk, rv);
  EXPECT_EQ(data_size, read_size);
  EXPECT_EQ(expected, std::string(buffer, read_size));

  rv = reader->Read(nullptr, 0, WebDataConsumerHandle::kFlagNone, &read_size);
  EXPECT_EQ(WebDataConsumerHandle::Result::kDone, rv);
}

class CountDidGetReadableClient : public blink::WebDataConsumerHandle::Client {
 public:
  ~CountDidGetReadableClient() override {}
  void DidGetReadable() override { num_did_get_readable_called_++; }
  int num_did_get_readable_called() { return num_did_get_readable_called_; }

 private:
  int num_did_get_readable_called_ = 0;
};

TEST_F(WebDataConsumerHandleImplTest, DidGetReadable) {
  static constexpr size_t kBlockSize = kDataPipeCapacity / 3;
  static constexpr size_t kTotalSize = kBlockSize * 2;

  std::unique_ptr<CountDidGetReadableClient> client =
      std::make_unique<CountDidGetReadableClient>();
  std::unique_ptr<WebDataConsumerHandleImpl> handle(
      new WebDataConsumerHandleImpl(std::move(consumer_)));
  std::unique_ptr<WebDataConsumerHandle::Reader> reader(handle->ObtainReader(
      client.get(), blink::scheduler::GetSingleThreadTaskRunnerForTesting()));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, client->num_did_get_readable_called());

  // Push two blocks.
  {
    std::string expected;
    int index = 0;
    for (size_t i = 0; i < kTotalSize; ++i) {
      expected += static_cast<char>(index + 'a');
      index = (37 * index + 11) % 26;
    }
    uint32_t size = expected.size();
    MojoResult rv =
        producer_->WriteData(expected.data(), &size, MOJO_WRITE_DATA_FLAG_NONE);
    EXPECT_EQ(MOJO_RESULT_OK, rv);
    EXPECT_EQ(kTotalSize, size);
  }
  base::RunLoop().RunUntilIdle();
  // |client| is notified the pipe gets ready.
  EXPECT_EQ(1, client->num_did_get_readable_called());

  // Read a block.
  {
    char buffer[kBlockSize];
    size_t size = 0;
    Result rv = reader->Read(&buffer, sizeof(buffer),
                             WebDataConsumerHandle::kFlagNone, &size);
    EXPECT_EQ(Result::kOk, rv);
    EXPECT_EQ(sizeof(buffer), size);
  }
  base::RunLoop().RunUntilIdle();
  // |client| is NOT notified since the data is still available.
  EXPECT_EQ(1, client->num_did_get_readable_called());

  // Read the other block.
  {
    const void* buffer = nullptr;
    size_t size = sizeof(buffer);
    Result rv =
        reader->BeginRead(&buffer, WebDataConsumerHandle::kFlagNone, &size);
    EXPECT_EQ(Result::kOk, rv);
    EXPECT_TRUE(buffer);
    EXPECT_EQ(kTotalSize - kBlockSize, size);
    base::RunLoop().RunUntilIdle();

    rv = reader->EndRead(kBlockSize);
    EXPECT_EQ(Result::kOk, rv);
  }
  base::RunLoop().RunUntilIdle();
  // |client| is NOT notified the pipe is still waiting for more data.
  EXPECT_EQ(1, client->num_did_get_readable_called());

  // Read one more.
  {
    char buffer[kBlockSize];
    size_t size = 0;
    Result rv = reader->Read(&buffer, sizeof(buffer),
                             WebDataConsumerHandle::kFlagNone, &size);
    EXPECT_EQ(Result::kShouldWait, rv);
  }
  base::RunLoop().RunUntilIdle();
  // |client| is NOT notified because the pipe is still waiting for more data.
  EXPECT_EQ(1, client->num_did_get_readable_called());

  // Push one more block.
  {
    std::string expected(kBlockSize, 'x');
    uint32_t size = expected.size();
    MojoResult rv =
        producer_->WriteData(expected.data(), &size, MOJO_WRITE_DATA_FLAG_NONE);
    EXPECT_EQ(MOJO_RESULT_OK, rv);
    EXPECT_EQ(expected.size(), size);
  }
  base::RunLoop().RunUntilIdle();
  // |client| is notified the pipe gets ready.
  EXPECT_EQ(2, client->num_did_get_readable_called());
}

}  // namespace

}  // namespace content
