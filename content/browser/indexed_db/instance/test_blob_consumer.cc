// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/test_blob_consumer.h"

#include "mojo/public/cpp/system/data_pipe.h"
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

}  // namespace content::indexed_db
