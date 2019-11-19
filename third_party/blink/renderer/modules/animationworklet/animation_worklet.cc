// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/animationworklet/animation_worklet.h"

#include "base/atomic_sequence_num.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/workers/worker_clients.h"
#include "third_party/blink/renderer/modules/animationworklet/animation_worklet_messaging_proxy.h"
#include "third_party/blink/renderer/modules/animationworklet/animation_worklet_proxy_client.h"

base::AtomicSequenceNumber g_next_worklet_id;

int NextId() {
  // Start id from 1. This way it safe to use it as key in hashmap with default
  // key traits.
  return g_next_worklet_id.GetNext() + 1;
}

namespace blink {

AnimationWorklet::AnimationWorklet(Document* document)
    : Worklet(document), worklet_id_(NextId()), last_animation_id_(0) {}

AnimationWorklet::~AnimationWorklet() = default;

bool AnimationWorklet::NeedsToCreateGlobalScope() {
  return GetNumberOfGlobalScopes() <
         static_cast<wtf_size_t>(
             AnimationWorkletProxyClient::kNumStatelessGlobalScopes);
}

WorkletGlobalScopeProxy* AnimationWorklet::CreateGlobalScope() {
  DCHECK(NeedsToCreateGlobalScope());

  if (!proxy_client_) {
    // TODO(kevers|majidvp): Consider refactoring so that proxy client
    // initialization can move to the constructor. Currently, initialization
    // in the constructor leads to test failures as the document frame has not
    // been initialized at the time of the constructor call.
    Document* document = To<Document>(GetExecutionContext());
    proxy_client_ =
        AnimationWorkletProxyClient::FromDocument(document, worklet_id_);
  }

  auto* worker_clients = MakeGarbageCollected<WorkerClients>();
  ProvideAnimationWorkletProxyClientTo(worker_clients, proxy_client_);

  AnimationWorkletMessagingProxy* proxy =
      MakeGarbageCollected<AnimationWorkletMessagingProxy>(
          GetExecutionContext());
  proxy->Initialize(worker_clients, ModuleResponsesMap());
  return proxy;
}

WorkletAnimationId AnimationWorklet::NextWorkletAnimationId() {
  // Id starts from 1. This way it safe to use it as key in hashmap with default
  // key traits.
  return WorkletAnimationId(worklet_id_, ++last_animation_id_);
}

void AnimationWorklet::Trace(blink::Visitor* visitor) {
  Worklet::Trace(visitor);
  visitor->Trace(proxy_client_);
}

}  // namespace blink
