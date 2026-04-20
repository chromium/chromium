// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INSTANCE_TEST_BLOB_CONSUMER_H_
#define CONTENT_BROWSER_INDEXED_DB_INSTANCE_TEST_BLOB_CONSUMER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "services/network/public/mojom/data_pipe_getter.mojom.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"

namespace content::indexed_db {

// Helper for tests that reads an entire blob from a mojom::Blob. This is
// roughly akin to `await blob.text()` from script.
class TestBlobConsumer : public mojo::DataPipeDrainer::Client,
                         public blink::mojom::BlobReaderClient {
 public:
  // We artificially limit the capacity of the pipe to verify behavior when a
  // blob exceeds this capacity.
  static constexpr int kPipeCapacity = 1000;

  static void ReadWholeBlob(mojo::Remote<blink::mojom::Blob>& blob,
                            base::OnceCallback<void(std::string)> on_complete,
                            base::OnceClosure on_some_written = {});

  // Reads `blob` through the `DataPipeGetter` path.
  static void ReadIntoDataPipe(
      mojo::Remote<blink::mojom::Blob>& blob,
      base::OnceCallback<void(uint64_t declared_size, std::string data)>
          on_complete);

 private:
  TestBlobConsumer(base::OnceCallback<void(std::string)> on_complete,
                   base::OnceClosure on_some_written);

  ~TestBlobConsumer() override;

  void Start(mojo::Remote<blink::mojom::Blob>& blob);

  // mojo::DataPipeDrainer::Client
  void OnDataAvailable(base::span<const uint8_t> data) override;
  void OnDataComplete() override;

  // blink::mojom::BlobReaderClient
  void OnCalculatedSize(uint64_t total_size,
                        uint64_t expected_content_size) override;
  void OnComplete(int32_t status, uint64_t data_length) override;

  mojo::Receiver<blink::mojom::BlobReaderClient> receiver_{this};
  // Called when `OnDataComplete()` is called.
  base::OnceCallback<void(std::string)> on_complete_;
  // Called when `OnDataAvailable()` is called for the first time, or when
  // `OnDataComplete()` is called if `OnDataAvailable()` was never called.
  base::OnceClosure on_some_written_;
  std::unique_ptr<mojo::DataPipeDrainer> drainer_;
  std::string data_;
};

}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_INSTANCE_TEST_BLOB_CONSUMER_H_
