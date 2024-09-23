// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/test/blob_test_utils.h"

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"

namespace storage {

namespace {

// Helper class to copy a blob to a string.
class DataPipeDrainerClient : public mojo::DataPipeDrainer::Client {
 public:
  explicit DataPipeDrainerClient(std::string* output) : output_(output) {}
  void Run() { run_loop_.Run(); }

  void OnDataAvailable(base::span<const uint8_t> data) override {
    output_->append(base::as_string_view(data));
  }
  void OnDataComplete() override { run_loop_.Quit(); }

 private:
  base::RunLoop run_loop_;
  raw_ptr<std::string> output_;
};

}  // namespace

std::string BlobToString(blink::mojom::Blob* blob) {
  std::string output;
  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  CHECK_EQ(MOJO_RESULT_OK,
           mojo::CreateDataPipe(/*options=*/nullptr, producer, consumer));
  blob->ReadAll(std::move(producer), mojo::NullRemote());
  DataPipeDrainerClient client(&output);
  mojo::DataPipeDrainer drainer(&client, std::move(consumer));
  client.Run();
  return output;
}

}  // namespace storage
