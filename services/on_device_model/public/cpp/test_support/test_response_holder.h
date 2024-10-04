// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_TEST_SUPPORT_TEST_RESPONSE_HOLDER_H_
#define SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_TEST_SUPPORT_TEST_RESPONSE_HOLDER_H_

#include <string>
#include <vector>

#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"

namespace on_device_model {

// Helper to accumulate a streamed response from model execution. This is only
// used by downstream clients, but is defined upstream to avoid downstream mojom
// dependencies.
class TestResponseHolder : public mojom::StreamingResponder {
 public:
  TestResponseHolder();
  ~TestResponseHolder() override;

  // Returns a remote which can be used to stream a response to this object.
  mojo::PendingRemote<mojom::StreamingResponder> BindRemote();

  // Accumulated responses so far from whoever controls the remote
  // StreamingResponder endpoint.
  const std::vector<std::string>& responses() const { return responses_; }
  bool complete() const { return complete_; }
  bool disconnected() const { return disconnected_; }
  bool terminated() const { return disconnected_ || complete_; }
  uint32_t input_token_count() const { return input_token_count_; }
  uint32_t output_token_count() const { return output_token_count_; }

  // Spins a RunLoop until this object observes completion of its response.
  void WaitForCompletion();

  // mojom::StreamingResponder:
  void OnResponse(mojom::ResponseChunkPtr chunk) override;
  void OnComplete(mojom::ResponseSummaryPtr summary) override;
  void OnDisconnect();

 private:
  base::RunLoop run_loop_;
  std::vector<std::string> responses_;
  bool complete_ = false;
  bool disconnected_ = false;
  uint32_t input_token_count_ = 0;
  uint32_t output_token_count_ = 0;
  mojo::Receiver<mojom::StreamingResponder> receiver_{this};
};

}  // namespace on_device_model

#endif  // SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_TEST_SUPPORT_TEST_RESPONSE_HOLDER_H_
