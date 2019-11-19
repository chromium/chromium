// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/test/blob_test_utils.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"

namespace storage {

namespace {

// Helper class to copy a blob to a string.
class DataPipeDrainerClient : public mojo::DataPipeDrainer::Client {
 public:
  explicit DataPipeDrainerClient(std::string* output) : output_(output) {}
  void Run() { run_loop_.Run(); }

  void OnDataAvailable(const void* data, size_t num_bytes) override {
    output_->append(reinterpret_cast<const char*>(data), num_bytes);
  }
  void OnDataComplete() override { run_loop_.Quit(); }

 private:
  base::RunLoop run_loop_;
  std::string* output_;
};

}  // namespace

std::string BlobToString(blink::mojom::Blob* blob) {
  std::string output;
  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  CHECK_EQ(MOJO_RESULT_OK,
           mojo::CreateDataPipe(/*options=*/nullptr, &producer, &consumer));
  blob->ReadAll(std::move(producer), mojo::NullRemote());
  DataPipeDrainerClient client(&output);
  mojo::DataPipeDrainer drainer(&client, std::move(consumer));
  client.Run();
  return output;
}

}  // namespace storage