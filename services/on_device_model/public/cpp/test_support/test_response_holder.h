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

#ifndef ML_INTERNAL_TEXT_SAFETY_SESSION_MIGRATION
#define ML_INTERNAL_TEXT_SAFETY_SESSION_MIGRATION 1
#endif

namespace on_device_model {

template <typename Responder>
class BaseTestResponseHolder : public Responder {
 public:
  BaseTestResponseHolder() = default;
  ~BaseTestResponseHolder() override = default;

  // Returns a remote which can be used to stream a response to this object.
  virtual mojo::PendingRemote<Responder> BindRemote() {
    auto remote = receiver_.BindNewPipeAndPassRemote();
    receiver_.set_disconnect_handler(
        base::BindOnce(&BaseTestResponseHolder<Responder>::OnDisconnect,
                       base::Unretained(this)));
    disconnected_ = false;
    return remote;
  }

  bool disconnected() const { return disconnected_; }

  // Spins a RunLoop until this object observes completion of its response.
  void WaitForCompletion() { run_loop_.Run(); }

  void OnDisconnect() {
    disconnected_ = true;
    run_loop_.Quit();
  }

 protected:
  void OnCompleted() { run_loop_.Quit(); }
  bool disconnected_ = false;

 private:
  base::RunLoop run_loop_;
  mojo::Receiver<Responder> receiver_{this};
};

// Helper to accumulate a streamed response from model execution. This is only
// used by downstream clients, but is defined upstream to avoid downstream mojom
// dependencies.
class TestResponseHolder
    : public BaseTestResponseHolder<mojom::StreamingResponder> {
 public:
  TestResponseHolder();
  ~TestResponseHolder() override;

  // Accumulated responses so far from whoever controls the remote
  // StreamingResponder endpoint.
  const std::vector<std::string>& responses() const { return responses_; }
  uint32_t output_token_count() const { return output_token_count_; }
  bool terminated() const { return disconnected_ || complete_; }
  bool complete() const { return complete_; }

  // mojom::StreamingResponder:
  void OnResponse(mojom::ResponseChunkPtr chunk) override;
  void OnComplete(mojom::ResponseSummaryPtr summary) override;

 private:
  std::vector<std::string> responses_;
  uint32_t output_token_count_ = 0;
  bool complete_ = false;
};

// Helper to accumulate a streamed response from model execution. This is only
// used by downstream clients, but is defined upstream to avoid downstream mojom
// dependencies.
class TestAsrResponseHolder
    : public BaseTestResponseHolder<mojom::AsrStreamResponder> {
 public:
  TestAsrResponseHolder();
  ~TestAsrResponseHolder() override;

  // mojom::AsrStreamResponder:
  void OnResponse(
      std::vector<mojom::SpeechRecognitionResultPtr> results) override;

  void WaitForFirstResponse() {
    wait_for_response_ = true;
    WaitForCompletion();
  }

  std::vector<std::string> AllResponses() { return responses_; }

 private:
  std::vector<std::string> responses_;
  bool wait_for_response_;
};

}  // namespace on_device_model

#endif  // SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_TEST_SUPPORT_TEST_RESPONSE_HOLDER_H_
