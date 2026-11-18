// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/blob_reader.h"

#include <stdint.h>

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/strings/string_view_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"
#include "storage/browser/test/fake_blob.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ControllableBlob : public storage::FakeBlob {
 public:
  ControllableBlob() : storage::FakeBlob("uuid") {}

  mojo::Remote<blink::mojom::BlobReaderClient>& client() { return client_; }

  void ReadAll(
      mojo::ScopedDataPipeProducerHandle producer,
      mojo::PendingRemote<blink::mojom::BlobReaderClient> client) override {
    producer_ = std::move(producer);
    client_.Bind(std::move(client));
  }

  void WriteAndClose(std::string_view data) {
    ASSERT_EQ(MOJO_RESULT_OK,
              producer_->WriteAllData(base::as_byte_span(data)));
    producer_.reset();
    if (client_.is_bound()) {
      client_.FlushForTesting();
    }
  }

 private:
  mojo::Remote<blink::mojom::BlobReaderClient> client_;
  mojo::ScopedDataPipeProducerHandle producer_;
};

TEST(BlobReaderDisconnectedTest, ReceiverDroppedBeforeRead) {
  base::test::SingleThreadTaskEnvironment task_environment;
  mojo::PendingRemote<blink::mojom::Blob> blob;
  blob.InitWithNewPipeAndPassReceiver().reset();
  base::test::TestFuture<std::string, int64_t> result;

  BlobReader::Read(std::move(blob), result.GetCallback());

  EXPECT_EQ("", result.Get<0>());
  EXPECT_EQ(0, result.Get<1>());
}

}  // namespace

class BlobReaderTest : public testing::Test {
 private:
  class ObservedBlobReader : public BlobReader {
   public:
    ObservedBlobReader(mojo::PendingRemote<blink::mojom::Blob> blob,
                       BlobReadCallback callback,
                       base::OnceClosure data_complete)
        : BlobReader(std::move(blob), std::move(callback), std::nullopt),
          data_complete_(std::move(data_complete)) {}

   private:
    void OnDataComplete() override {
      // The base implementation can delete `this`.
      base::OnceClosure data_complete = std::move(data_complete_);
      BlobReader::OnDataComplete();
      std::move(data_complete).Run();
    }

    base::OnceClosure data_complete_;
  };

 protected:
  void SetUp() override {
    (new ObservedBlobReader(receiver_.BindNewPipeAndPassRemote(),
                            result_.GetCallback(),
                            data_complete_.QuitClosure()))
        ->Start();
    receiver_.FlushForTesting();
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  base::test::TestFuture<std::string, int64_t> result_;
  base::RunLoop data_complete_;
  ControllableBlob blob_;
  mojo::Receiver<blink::mojom::Blob> receiver_{&blob_};
};

TEST_F(BlobReaderTest, WaitsForOnCompleteAfterPipeCloses) {
  blob_.client()->OnCalculatedSize(4, 4);
  blob_.WriteAndClose("data");
  data_complete_.Run();
  EXPECT_FALSE(result_.IsReady());

  blob_.client()->OnComplete(net::OK, 4);
  EXPECT_EQ("data", result_.Get<0>());
  EXPECT_EQ(4, result_.Get<1>());
}

TEST_F(BlobReaderTest, WaitsForDataAfterOnComplete) {
  blob_.client()->OnCalculatedSize(4, 4);
  blob_.client()->OnComplete(net::OK, 4);
  blob_.client().FlushForTesting();
  EXPECT_FALSE(result_.IsReady());

  blob_.WriteAndClose("data");
  EXPECT_EQ("data", result_.Get<0>());
  EXPECT_EQ(4, result_.Get<1>());
}

TEST_F(BlobReaderTest, ReadErrorAfterPipeCloses) {
  blob_.client()->OnCalculatedSize(8, 8);
  blob_.WriteAndClose("data");
  data_complete_.Run();
  EXPECT_FALSE(result_.IsReady());

  blob_.client()->OnComplete(net::ERR_FAILED, 4);
  EXPECT_EQ(0, result_.Get<1>());
}

TEST_F(BlobReaderTest, ReadErrorBeforeSize) {
  blob_.client()->OnComplete(net::ERR_FAILED, 0);
  EXPECT_EQ(0, result_.Get<1>());
}

TEST_F(BlobReaderTest, DisconnectWhileWaitingForOnComplete) {
  blob_.client()->OnCalculatedSize(4, 4);
  blob_.WriteAndClose("data");
  data_complete_.Run();
  EXPECT_FALSE(result_.IsReady());

  blob_.client().reset();
  EXPECT_EQ(0, result_.Get<1>());
}

TEST_F(BlobReaderTest, DisconnectBeforeSize) {
  blob_.client().reset();
  EXPECT_EQ(0, result_.Get<1>());
}

TEST_F(BlobReaderTest, DisconnectAfterOnCompleteStillWaitsForData) {
  blob_.client()->OnCalculatedSize(4, 4);
  blob_.client()->OnComplete(net::OK, 4);
  blob_.client().FlushForTesting();
  blob_.client().reset();
  EXPECT_FALSE(result_.IsReady());

  blob_.WriteAndClose("data");
  EXPECT_EQ("data", result_.Get<0>());
  EXPECT_EQ(4, result_.Get<1>());
}
