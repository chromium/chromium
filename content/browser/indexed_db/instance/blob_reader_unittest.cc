// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/blob_reader.h"

#include <array>
#include <memory>
#include <string>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content::indexed_db {

namespace {

constexpr uint64_t kFileSize = 4096;
constexpr uint32_t kPipeCapacity = kFileSize / 4;

class BlobReaderTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::FilePath file_path = temp_dir_.GetPath().AppendASCII("blob");
    ASSERT_TRUE(base::WriteFile(file_path, std::string(kFileSize, 'a')));
    blob_info_.set_indexed_db_file_path(file_path);
  }

 protected:
  mojo::ScopedDataPipeConsumerHandle StartRead(
      uint64_t read_length,
      net::Error expected_result,
      storage::mojom::BlobDataItemReader::ReadCallback callback) {
    std::unique_ptr<BlobReader> reader = std::make_unique<BlobReader>(
        blob_info_, base::DoNothing(),
        base::BindLambdaForTesting([expected_result](net::Error result) {
          EXPECT_EQ(expected_result, result);
        }));
    mojo::ScopedDataPipeProducerHandle producer;
    mojo::ScopedDataPipeConsumerHandle consumer;
    CHECK_EQ(MOJO_RESULT_OK,
             mojo::CreateDataPipe(kPipeCapacity, producer, consumer));
    BlobReader* reader_ptr = reader.get();
    reader_ptr->Read(0, read_length, std::move(producer),
                     std::move(callback).Then(base::OnceClosure(
                         base::DoNothingWithBoundArgs(std::move(reader)))));
    return consumer;
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  IndexedDBExternalObject blob_info_{u"text/plain", kFileSize,
                                     /*blob_number=*/1};
};

TEST_F(BlobReaderTest, DestructionCancelsActiveReads) {
  std::unique_ptr<BlobReader> reader = std::make_unique<BlobReader>(
      blob_info_, base::DoNothing(), base::BindRepeating([](net::Error) {
        ADD_FAILURE() << "Cancelled reads should not report completion";
      }));
  base::test::TestFuture<int32_t> read_callback;
  std::array<mojo::ScopedDataPipeConsumerHandle, 2> consumers;
  for (size_t i = 0; i < consumers.size(); ++i) {
    mojo::ScopedDataPipeProducerHandle producer;
    ASSERT_EQ(MOJO_RESULT_OK,
              mojo::CreateDataPipe(kPipeCapacity, producer, consumers[i]));
    if (i == 0) {
      reader->ReadAll(std::move(producer), {});
    } else {
      reader->Read(0, kFileSize, std::move(producer),
                   read_callback.GetCallback());
    }
    ASSERT_FALSE(consumers[i]->QuerySignalsState().peer_closed());
  }

  // Queue a writable notification before destroying the reader.
  base::span<const uint8_t> buffer;
  ASSERT_EQ(MOJO_RESULT_OK,
            consumers[0]->BeginReadData(MOJO_READ_DATA_FLAG_NONE, buffer));
  ASSERT_EQ(MOJO_RESULT_OK, consumers[0]->EndReadData(buffer.size()));

  reader.reset();

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

TEST_F(BlobReaderTest, SynchronousCompletion) {
  base::test::TestFuture<int32_t> completion;
  // The first kPipeCapacity bytes are read from the file and stuffed in the
  // pipe synchronously. Since that's the entire read, completion is
  // synchronous.
  mojo::ScopedDataPipeConsumerHandle consumer =
      StartRead(kPipeCapacity, net::OK, completion.GetCallback());

  EXPECT_TRUE(completion.IsReady());
  EXPECT_EQ(net::OK, completion.Get());
  EXPECT_TRUE(consumer->QuerySignalsState().peer_closed());
}

TEST_F(BlobReaderTest, RemotePipeClosure) {
  base::test::TestFuture<int32_t> completion;
  mojo::ScopedDataPipeConsumerHandle consumer =
      StartRead(kFileSize, net::ERR_ABORTED, completion.GetCallback());
  ASSERT_FALSE(completion.IsReady());
  consumer.reset();

  EXPECT_EQ(net::ERR_ABORTED, completion.Get());
}

}  // namespace

}  // namespace content::indexed_db
