// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_PRIVATE_MODEL_TRAINING_BINDINGS_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_PRIVATE_MODEL_TRAINING_BINDINGS_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/context_recycler.h"
#include "v8/include/v8-forward.h"

namespace auction_worklet {

// Class to manage bindings for Private Model Training.
// Specifically the `sendEncryptedTo()` javascript function,
// Expected to be used for a context managed by ContextRecycler. Allows only a
// single call. On any subsequent calls, clears the payload and throws an
// exception.
//
// More information on Private Model Training can be found here:
// https://github.com/WICG/turtledove/blob/main/PA_private_model_training.md
class PrivateModelTrainingBindings : public Bindings {
 public:
  explicit PrivateModelTrainingBindings(AuctionV8Helper* v8_helper);
  PrivateModelTrainingBindings(const PrivateModelTrainingBindings&) = delete;
  PrivateModelTrainingBindings& operator=(const PrivateModelTrainingBindings&) =
      delete;
  ~PrivateModelTrainingBindings() override;

  // The PrivateModelTrainingBindings must outlive the context.
  void AttachToContext(v8::Local<v8::Context> context) override;
  void Reset() override;

  const std::optional<mojo_base::BigBuffer>& payload() const {
    return payload_;
  }

  std::optional<mojo_base::BigBuffer> TakePayload() {
    return std::move(payload_);
  }

 private:
  // Sends encrypted modeling signals to the destination specified in
  // modelingSignalsConfig. The input 'args' is expected to be a single
  // argument: an ArrayBuffer containing the modeling signals which will be
  // encrypted.
  static void SendEncryptedTo(const v8::FunctionCallbackInfo<v8::Value>& args);

  const raw_ptr<AuctionV8Helper> v8_helper_;

  // cleared if an exception is thrown.
  std::optional<mojo_base::BigBuffer> payload_ = std::nullopt;

  bool already_called_ = false;
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_PRIVATE_MODEL_TRAINING_BINDINGS_H_
