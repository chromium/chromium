// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/compositor_mutator_client.h"

#include <memory>
#include "base/trace_event/trace_event.h"
#include "third_party/blink/renderer/platform/graphics/animation_worklet_mutator_dispatcher_impl.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

CompositorMutatorClient::CompositorMutatorClient(
    std::unique_ptr<AnimationWorkletMutatorDispatcherImpl> mutator)
    : mutator_(std::move(mutator)) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc"),
               "CompositorMutatorClient::CompositorMutatorClient");
  mutator_->SetClient(this);
}

CompositorMutatorClient::~CompositorMutatorClient() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc"),
               "CompositorMutatorClient::~CompositorMutatorClient");
}

bool CompositorMutatorClient::Mutate(
    std::unique_ptr<cc::MutatorInputState> input_state,
    MutateQueuingStrategy queueing_strategy,
    DoneCallback on_done) {
  TRACE_EVENT0("cc", "CompositorMutatorClient::Mutate");
  return mutator_->MutateAsynchronously(
      std::move(input_state), queueing_strategy,
      CrossThreadBindOnce(std::move(on_done)));
}

void CompositorMutatorClient::SetMutationUpdate(
    std::unique_ptr<cc::MutatorOutputState> output_state) {
  TRACE_EVENT0("cc", "CompositorMutatorClient::SetMutationUpdate");
  client_->SetMutationUpdate(std::move(output_state));
}

void CompositorMutatorClient::SetClient(cc::LayerTreeMutatorClient* client) {
  TRACE_EVENT0("cc", "CompositorMutatorClient::SetClient");
  client_ = client;
}

bool CompositorMutatorClient::HasMutators() {
  return mutator_->HasMutators();
}

}  // namespace blink
