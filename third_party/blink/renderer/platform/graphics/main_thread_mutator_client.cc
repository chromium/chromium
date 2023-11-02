// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/main_thread_mutator_client.h"

#include <memory>
#include "third_party/blink/renderer/platform/graphics/animation_worklet_mutator_dispatcher_impl.h"

namespace blink {

MainThreadMutatorClient::MainThreadMutatorClient(
    std::unique_ptr<AnimationWorkletMutatorDispatcherImpl> mutator)
    : mutator_(std::move(mutator)) {
  mutator_->SetClient(this);
}

void MainThreadMutatorClient::SynchronizeAnimatorName(
    const String& animator_name) {
  delegate_->SynchronizeAnimatorName(animator_name);
}

void MainThreadMutatorClient::SetMutationUpdate(
    std::unique_ptr<AnimationWorkletOutput> output_state) {
  delegate_->SetMutationUpdate(std::move(output_state));
}

void MainThreadMutatorClient::SetDelegate(MutatorClient* delegate) {
  delegate_ = delegate;
}

}  // namespace blink
