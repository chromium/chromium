// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/test_blob_consumer.h"

#include "base/test/test_future.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content::indexed_db {

TestBlobConsumer::TestBlobConsumer(
    base::OnceCallback<void(std::string)> on_complete,
    base::OnceClosure on_some_written)
    : on_complete_(std::move(on_complete)),
      on_some_written_(std::move(on_some_written)) {}

TestBlobConsumer::~TestBlobConsumer() = default;

void TestBlobConsumer::Start(mojo::Remote<blink::mojom::Blob>& blob) {
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  MojoResult result =
      CreateDataPipe(kPipeCapacity, producer_handle, consumer_handle);
  EXPECT_EQ(result, MOJO_RESULT_OK);

  drainer_ =
      std::make_unique<mojo::DataPipeDrainer>(this, std::move(consumer_handle));

  blob->ReadAll(std::move(producer_handle),
                receiver_.BindNewPipeAndPassRemote());
}

void TestBlobConsumer::OnDataAvailable(base::span<const uint8_t> data) {
  data_.append(reinterpret_cast<const char*>(data.data()), data.size());
  if (on_some_written_) {
    std::move(on_some_written_).Run();
  }
}

void TestBlobConsumer::OnDataComplete() {
  if (on_some_written_) {
    std::move(on_some_written_).Run();
  }
  std::move(on_complete_).Run(std::move(data_));
  delete this;
}

void TestBlobConsumer::OnCalculatedSize(uint64_t total_size,
                                        uint64_t expected_content_size) {}

void TestBlobConsumer::OnComplete(int32_t status, uint64_t data_length) {}

// static
void TestBlobConsumer::ReadWholeBlob(
    mojo::Remote<blink::mojom::Blob>& blob,
    base::OnceCallback<void(std::string)> on_complete,
    base::OnceClosure on_some_written) {
  (new TestBlobConsumer(std::move(on_complete), std::move(on_some_written)))
      ->Start(blob);
}

// Helper that drains a data pipe, collecting all bytes into a string.
class DataPipeDrainerClient : public mojo::DataPipeDrainer::Client {
 public:
  DataPipeDrainerClient(std::string* out, base::OnceClosure on_done)
      : out_(out), on_done_(std::move(on_done)) {}
  void OnDataAvailable(base::span<const uint8_t> data) override {
    out_->append(reinterpret_cast<const char*>(data.data()), data.size());
  }
  void OnDataComplete() override { std::move(on_done_).Run(); }

 private:
  raw_ptr<std::string> out_;
  base::OnceClosure on_done_;
};

// static
void TestBlobConsumer::ReadIntoDataPipe(
    mojo::Remote<blink::mojom::Blob>& blob,
    base::OnceCallback<void(uint64_t declared_size, std::string data)>
        on_complete) {
  mojo::Remote<network::mojom::DataPipeGetter> data_pipe_getter;
  blob->AsDataPipeGetter(data_pipe_getter.BindNewPipeAndPassReceiver());

  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  ASSERT_EQ(mojo::CreateDataPipe(nullptr, producer, consumer), MOJO_RESULT_OK);

  base::test::TestFuture<int32_t, uint64_t> read_future;
  data_pipe_getter->Read(std::move(producer), read_future.GetCallback());

  auto [status, declared_size] = read_future.Take();
  ASSERT_EQ(status, net::OK);

  std::string data;
  base::RunLoop drain_loop;
  DataPipeDrainerClient drainer_client(&data, drain_loop.QuitClosure());
  mojo::DataPipeDrainer drainer(&drainer_client, std::move(consumer));
  drain_loop.Run();

  std::move(on_complete).Run(declared_size, std::move(data));
}

}  // namespace content::indexed_db
