// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/sqlite/active_blob_streamer.h"

#include <array>
#include <memory>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "sql/database.h"
#include "sql/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content::indexed_db::sqlite {

namespace {

constexpr int kBlobSize = 4096;
constexpr uint32_t kPipeCapacity = kBlobSize / 4;

class ActiveBlobStreamerTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(db_.OpenInMemory());
    ASSERT_TRUE(db_.Execute("CREATE TABLE blobs (bytes BLOB)"));
    ASSERT_TRUE(db_.Execute("INSERT INTO blobs VALUES (zeroblob(4096))"));
  }

 protected:
  std::unique_ptr<ActiveBlobStreamer> CreateStreamer(
      base::RepeatingCallback<void(net::Error)> on_read_complete) {
    return std::make_unique<ActiveBlobStreamer>(
        IndexedDBExternalObject(u"text/plain", kBlobSize, /*blob_number=*/1),
        base::BindLambdaForTesting([this](size_t chunk_index) {
          EXPECT_EQ(0U, chunk_index);
          return db_.GetStreamingBlob("blobs", "bytes", /*row_id=*/1,
                                      /*readonly=*/true);
        }),
        kBlobSize, base::DoNothing(), std::move(on_read_complete));
  }

  mojo::ScopedDataPipeConsumerHandle StartRead(
      uint64_t read_length,
      net::Error expected_result,
      storage::mojom::BlobDataItemReader::ReadCallback callback) {
    std::unique_ptr<ActiveBlobStreamer> streamer = CreateStreamer(
        base::BindLambdaForTesting([expected_result](net::Error result) {
          EXPECT_EQ(expected_result, result);
        }));
    mojo::ScopedDataPipeProducerHandle producer;
    mojo::ScopedDataPipeConsumerHandle consumer;
    CHECK_EQ(MOJO_RESULT_OK,
             mojo::CreateDataPipe(kPipeCapacity, producer, consumer));
    ActiveBlobStreamer* streamer_ptr = streamer.get();
    streamer_ptr->Read(0, read_length, std::move(producer),
                       std::move(callback).Then(base::OnceClosure(
                           base::DoNothingWithBoundArgs(std::move(streamer)))));
    return consumer;
  }

  base::test::TaskEnvironment task_environment_;
  sql::Database db_{sql::test::kTestTag};
};

TEST_F(ActiveBlobStreamerTest, DestructionCancelsActiveReads) {
  std::unique_ptr<ActiveBlobStreamer> streamer =
      CreateStreamer(base::BindRepeating([](net::Error) {
        ADD_FAILURE() << "Cancelled reads should not report completion";
      }));
  base::test::TestFuture<int32_t> read_callback;
  std::array<mojo::ScopedDataPipeConsumerHandle, 2> consumers;
  for (size_t i = 0; i < consumers.size(); ++i) {
    mojo::ScopedDataPipeProducerHandle producer;
    ASSERT_EQ(MOJO_RESULT_OK,
              mojo::CreateDataPipe(kPipeCapacity, producer, consumers[i]));
    if (i == 0) {
      streamer->ReadAll(std::move(producer), {});
    } else {
      streamer->Read(0, kBlobSize, std::move(producer),
                     read_callback.GetCallback());
    }
    ASSERT_FALSE(consumers[i]->QuerySignalsState().peer_closed());
  }

  // Queue a writable notification before destroying the streamer.
  base::span<const uint8_t> buffer;
  ASSERT_EQ(MOJO_RESULT_OK,
            consumers[0]->BeginReadData(MOJO_READ_DATA_FLAG_NONE, buffer));
  ASSERT_EQ(MOJO_RESULT_OK, consumers[0]->EndReadData(buffer.size()));

  streamer.reset();

  EXPECT_EQ(net::ERR_ABORTED, read_callback.Get());

  for (const mojo::ScopedDataPipeConsumerHandle& consumer : consumers) {
    base::test::TestFuture<MojoResult, mojo::HandleSignalsState> peer_closed;
    mojo::SimpleWatcher watcher(FROM_HERE,
                                mojo::SimpleWatcher::ArmingPolicy::MANUAL);
    ASSERT_EQ(
        MOJO_RESULT_OK,
        watcher.Watch(consumer.get(), MOJO_HANDLE_SIGNAL_PEER_CLOSED,
                      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                      peer_closed.GetRepeatingCallback<
                          MojoResult, const mojo::HandleSignalsState&>()));
    watcher.ArmOrNotify();
    ASSERT_TRUE(peer_closed.Wait());
    EXPECT_EQ(MOJO_RESULT_OK, peer_closed.Get<MojoResult>());
    EXPECT_TRUE(peer_closed.Get<mojo::HandleSignalsState>().peer_closed());
  }
}

TEST_F(ActiveBlobStreamerTest, SynchronousCompletion) {
  base::test::TestFuture<int32_t> completion;
  // The first kPipeCapacity bytes are read from the blob and stuffed in the
  // pipe synchronously. Since that's the entire read, completion is
  // synchronous.
  mojo::ScopedDataPipeConsumerHandle consumer =
      StartRead(kPipeCapacity, net::OK, completion.GetCallback());

  EXPECT_TRUE(completion.IsReady());
  EXPECT_EQ(net::OK, completion.Get());
  EXPECT_TRUE(consumer->QuerySignalsState().peer_closed());
}

TEST_F(ActiveBlobStreamerTest, RemotePipeClosure) {
  base::test::TestFuture<int32_t> completion;
  mojo::ScopedDataPipeConsumerHandle consumer =
      StartRead(kBlobSize, net::ERR_ABORTED, completion.GetCallback());
  ASSERT_FALSE(completion.IsReady());
  consumer.reset();

  EXPECT_EQ(net::ERR_ABORTED, completion.Get());
}

}  // namespace

}  // namespace content::indexed_db::sqlite
