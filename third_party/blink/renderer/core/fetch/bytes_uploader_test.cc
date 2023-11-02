// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/bytes_uploader.h"

#include "base/test/mock_callback.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"
#include "services/network/public/mojom/chunked_data_pipe_getter.mojom-blink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

using network::mojom::blink::ChunkedDataPipeGetter;
using testing::_;
using testing::InSequence;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;
using testing::StrictMock;

namespace blink {

typedef testing::StrictMock<testing::MockFunction<void(int)>> Checkpoint;

class MockBytesConsumer : public BytesConsumer {
 public:
  MockBytesConsumer() = default;
  ~MockBytesConsumer() override = default;

  MOCK_METHOD2(BeginRead, Result(const char**, size_t*));
  MOCK_METHOD1(EndRead, Result(size_t));
  MOCK_METHOD1(SetClient, void(Client*));
  MOCK_METHOD0(ClearClient, void());
  MOCK_METHOD0(Cancel, void());
  PublicState GetPublicState() const override { return state_; }
  void SetPublicState(PublicState state) { state_ = state; }
  MOCK_CONST_METHOD0(GetError, Error());
  MOCK_CONST_METHOD0(DebugName, String());

 private:
  PublicState state_ = PublicState::kReadableOrWaiting;
};

class BytesUploaderTest : public ::testing::Test {
 public:
  void InitializeBytesUploader(MockBytesConsumer* mock_bytes_consumer,
                               uint32_t capacity = 100u) {
    bytes_uploader_ = MakeGarbageCollected<BytesUploader>(
        nullptr, mock_bytes_consumer, remote_.BindNewPipeAndPassReceiver(),
        blink::scheduler::GetSingleThreadTaskRunnerForTesting(),
        /*client=*/nullptr);

    const MojoCreateDataPipeOptions data_pipe_options{
        sizeof(MojoCreateDataPipeOptions), MOJO_CREATE_DATA_PIPE_FLAG_NONE, 1,
        capacity};
    ASSERT_EQ(MOJO_RESULT_OK,
              mojo::CreateDataPipe(&data_pipe_options, writable_, readable_));
  }

  mojo::ScopedDataPipeProducerHandle& Writable() { return writable_; }
  mojo::ScopedDataPipeConsumerHandle& Readable() { return readable_; }
  mojo::Remote<ChunkedDataPipeGetter>& Remote() { return remote_; }

 protected:
  Persistent<BytesUploader> bytes_uploader_;

 private:
  mojo::ScopedDataPipeProducerHandle writable_;
  mojo::ScopedDataPipeConsumerHandle readable_;
  mojo::Remote<ChunkedDataPipeGetter> remote_;
};

TEST_F(BytesUploaderTest, Create) {
  auto* mock_bytes_consumer =
      MakeGarbageCollected<StrictMock<MockBytesConsumer>>();

  Checkpoint checkpoint;
  {
    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
  }

  checkpoint.Call(1);
  mojo::PendingRemote<ChunkedDataPipeGetter> pending_remote;
  BytesUploader* bytes_uploader_ = MakeGarbageCollected<BytesUploader>(
      nullptr, mock_bytes_consumer,
      pending_remote.InitWithNewPipeAndPassReceiver(),
      blink::scheduler::GetSingleThreadTaskRunnerForTesting(),
      /*client=*/nullptr);
  ASSERT_TRUE(bytes_uploader_);
}

// TODO(yoichio): Needs BytesConsumer state tests.

TEST_F(BytesUploaderTest, ReadEmpty) {
  auto* mock_bytes_consumer =
      MakeGarbageCollected<StrictMock<MockBytesConsumer>>();
  base::MockCallback<ChunkedDataPipeGetter::GetSizeCallback> get_size_callback;
  Checkpoint checkpoint;
  {
    InSequence s;

    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(*mock_bytes_consumer, SetClient(_));
    EXPECT_CALL(*mock_bytes_consumer, BeginRead(_, _))
        .WillOnce(Return(BytesConsumer::Result::kDone));
    EXPECT_CALL(*mock_bytes_consumer, Cancel());
    EXPECT_CALL(get_size_callback, Run(net::OK, 0u));

    EXPECT_CALL(checkpoint, Call(3));
  }

  checkpoint.Call(1);
  InitializeBytesUploader(mock_bytes_consumer);
  Remote()->GetSize(get_size_callback.Get());
  Remote()->StartReading(std::move(Writable()));

  checkpoint.Call(2);
  test::RunPendingTasks();

  checkpoint.Call(3);
  char buffer[20] = {};
  uint32_t num_bytes = sizeof(buffer);
  MojoResult rv =
      Readable()->ReadData(buffer, &num_bytes, MOJO_READ_DATA_FLAG_NONE);
  EXPECT_EQ(MOJO_RESULT_SHOULD_WAIT, rv);
}

TEST_F(BytesUploaderTest, ReadSmall) {
  auto* mock_bytes_consumer =
      MakeGarbageCollected<StrictMock<MockBytesConsumer>>();
  base::MockCallback<ChunkedDataPipeGetter::GetSizeCallback> get_size_callback;
  Checkpoint checkpoint;
  {
    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(*mock_bytes_consumer, SetClient(_));
    EXPECT_CALL(*mock_bytes_consumer, BeginRead(_, _))
        .WillOnce(Invoke([](const char** buffer, size_t* size) {
          *size = 6;
          *buffer = "foobar";
          return BytesConsumer::Result::kOk;
        }));
    EXPECT_CALL(*mock_bytes_consumer, EndRead(6u))
        .WillOnce(Return(BytesConsumer::Result::kDone));
    EXPECT_CALL(*mock_bytes_consumer, Cancel());
    EXPECT_CALL(get_size_callback, Run(net::OK, 6u));

    EXPECT_CALL(checkpoint, Call(3));
  }

  checkpoint.Call(1);
  InitializeBytesUploader(mock_bytes_consumer);
  Remote()->GetSize(get_size_callback.Get());
  Remote()->StartReading(std::move(Writable()));

  checkpoint.Call(2);
  test::RunPendingTasks();

  checkpoint.Call(3);
  char buffer[20] = {};
  uint32_t num_bytes = sizeof(buffer);
  EXPECT_EQ(MOJO_RESULT_OK,
            Readable()->ReadData(buffer, &num_bytes, MOJO_READ_DATA_FLAG_NONE));
  EXPECT_EQ(6u, num_bytes);
  EXPECT_STREQ("foobar", buffer);
}

TEST_F(BytesUploaderTest, ReadOverPipeCapacity) {
  auto* mock_bytes_consumer =
      MakeGarbageCollected<StrictMock<MockBytesConsumer>>();
  base::MockCallback<ChunkedDataPipeGetter::GetSizeCallback> get_size_callback;
  Checkpoint checkpoint;
  {
    InSequence s;

    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(*mock_bytes_consumer, SetClient(_));
    EXPECT_CALL(*mock_bytes_consumer, BeginRead(_, _))
        .WillOnce(Invoke([](const char** buffer, size_t* size) {
          *size = 12;
          *buffer = "foobarFOOBAR";
          return BytesConsumer::Result::kOk;
        }));
    EXPECT_CALL(*mock_bytes_consumer, EndRead(10u))
        .WillOnce(Return(BytesConsumer::Result::kOk));

    EXPECT_CALL(*mock_bytes_consumer, BeginRead(_, _))
        .WillOnce(Invoke([](const char** buffer, size_t* size) {
          *size = 2;
          *buffer = "AR";
          return BytesConsumer::Result::kOk;
        }));
    EXPECT_CALL(*mock_bytes_consumer, EndRead(0u))
        .WillOnce(Return(BytesConsumer::Result::kOk));

    EXPECT_CALL(checkpoint, Call(3));
    EXPECT_CALL(checkpoint, Call(4));
    EXPECT_CALL(*mock_bytes_consumer, BeginRead(_, _))
        .WillOnce(Invoke([](const char** buffer, size_t* size) {
          *size = 2;
          *buffer = "AR";
          return BytesConsumer::Result::kOk;
        }));
    EXPECT_CALL(*mock_bytes_consumer, EndRead(2u))
        .WillOnce(Return(BytesConsumer::Result::kDone));
    EXPECT_CALL(*mock_bytes_consumer, Cancel());
    EXPECT_CALL(get_size_callback, Run(net::OK, 12u));
  }

  checkpoint.Call(1);
  InitializeBytesUploader(mock_bytes_consumer, 10u);
  Remote()->GetSize(get_size_callback.Get());
  Remote()->StartReading(std::move(Writable()));

  checkpoint.Call(2);
  test::RunPendingTasks();

  checkpoint.Call(3);
  char buffer[20] = {};
  uint32_t num_bytes = sizeof(buffer);
  EXPECT_EQ(MOJO_RESULT_OK,
            Readable()->ReadData(buffer, &num_bytes, MOJO_READ_DATA_FLAG_NONE));
  EXPECT_EQ(10u, num_bytes);
  EXPECT_STREQ("foobarFOOB", buffer);

  checkpoint.Call(4);
  test::RunPendingTasks();
  char buffer2[20] = {};
  num_bytes = sizeof(buffer2);
  EXPECT_EQ(MOJO_RESULT_OK, Readable()->ReadData(buffer2, &num_bytes,
                                                 MOJO_READ_DATA_FLAG_NONE));
  EXPECT_EQ(2u, num_bytes);
  EXPECT_STREQ("AR", buffer2);
}

TEST_F(BytesUploaderTest, StartReadingWithoutGetSize) {
  auto* mock_bytes_consumer =
      MakeGarbageCollected<NiceMock<MockBytesConsumer>>();
  InitializeBytesUploader(mock_bytes_consumer);

  Remote()->StartReading(std::move(Writable()));
  test::RunPendingTasks();
  // The operation is rejected, and the connection is shut down.
  EXPECT_FALSE(Remote().is_connected());
}

}  // namespace blink
