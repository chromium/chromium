// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/bytes_uploader.h"

#include "base/containers/span.h"
#include "base/test/mock_callback.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"
#include "services/network/public/mojom/chunked_data_pipe_getter.mojom-blink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
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

  MOCK_METHOD1(BeginRead, Result(base::span<const char>&));
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
  ~BytesUploaderTest() override {
    // Avoids leaking mocked objects passed to `bytes_uploader_`.
    bytes_uploader_.Release();
  }
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
  test::TaskEnvironment task_environment_;
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
    EXPECT_CALL(*mock_bytes_consumer, BeginRead(_))
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
  std::string buffer(20, '\0');
  size_t actually_read_bytes = 0;
  MojoResult rv = Readable()->ReadData(MOJO_READ_DATA_FLAG_NONE,
                                       base::as_writable_byte_span(buffer),
                                       actually_read_bytes);
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
    EXPECT_CALL(*mock_bytes_consumer, BeginRead(_))
        .WillOnce(Invoke([](base::span<const char>& buffer) {
          buffer = base::span_from_cstring("foobar");
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
  std::string buffer(20, '\0');
  size_t actually_read_bytes = 0;
  EXPECT_EQ(MOJO_RESULT_OK,
            Readable()->ReadData(MOJO_READ_DATA_FLAG_NONE,
                                 base::as_writable_byte_span(buffer),
                                 actually_read_bytes));
  EXPECT_EQ(6u, actually_read_bytes);
  EXPECT_EQ("foobar", buffer.substr(0, 6));
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
    EXPECT_CALL(*mock_bytes_consumer, BeginRead(_))
        .WillOnce(Invoke([](base::span<const char>& buffer) {
          buffer = base::span_from_cstring("foobarFOOBAR");
          return BytesConsumer::Result::kOk;
        }));
    EXPECT_CALL(*mock_bytes_consumer, EndRead(10u))
        .WillOnce(Return(BytesConsumer::Result::kOk));

    EXPECT_CALL(*mock_bytes_consumer, BeginRead(_))
        .WillOnce(Invoke([](base::span<const char>& buffer) {
          buffer = base::span_from_cstring("AR");
          return BytesConsumer::Result::kOk;
        }));
    EXPECT_CALL(*mock_bytes_consumer, EndRead(0u))
        .WillOnce(Return(BytesConsumer::Result::kOk));

    EXPECT_CALL(checkpoint, Call(3));
    EXPECT_CALL(checkpoint, Call(4));
    EXPECT_CALL(*mock_bytes_consumer, BeginRead(_))
        .WillOnce(Invoke([](base::span<const char>& buffer) {
          buffer = base::span_from_cstring("AR");
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
  std::string buffer(20, '\0');
  size_t actually_read_bytes = 0;
  EXPECT_EQ(MOJO_RESULT_OK,
            Readable()->ReadData(MOJO_READ_DATA_FLAG_NONE,
                                 base::as_writable_byte_span(buffer),
                                 actually_read_bytes));
  EXPECT_EQ(10u, actually_read_bytes);
  EXPECT_EQ("foobarFOOB", buffer.substr(0, 10));

  checkpoint.Call(4);
  test::RunPendingTasks();
  std::string buffer2(20, '\0');
  EXPECT_EQ(MOJO_RESULT_OK,
            Readable()->ReadData(MOJO_READ_DATA_FLAG_NONE,
                                 base::as_writable_byte_span(buffer2),
                                 actually_read_bytes));
  EXPECT_EQ(2u, actually_read_bytes);
  EXPECT_EQ("AR", buffer2.substr(0, 2));
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
