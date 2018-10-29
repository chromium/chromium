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
    : Worklet(document), scope_id_(NextId()), last_animation_id_(0) {}

AnimationWorklet::~AnimationWorklet() = default;

bool AnimationWorklet::NeedsToCreateGlobalScope() {
  // For now, create only one global scope per document.
  // TODO(nhiroki): Revisit this later.
  return GetNumberOfGlobalScopes() == 0;
}

WorkletGlobalScopeProxy* AnimationWorklet::CreateGlobalScope() {
  DCHECK(NeedsToCreateGlobalScope());

  Document* document = To<Document>(GetExecutionContext());
  AnimationWorkletProxyClient* proxy_client =
      AnimationWorkletProxyClient::FromDocument(document, scope_id_);

  WorkerClients* worker_clients = WorkerClients::Create();
  ProvideAnimationWorkletProxyClientTo(worker_clients, proxy_client);

  AnimationWorkletMessagingProxy* proxy =
      new AnimationWorkletMessagingProxy(GetExecutionContext());
  proxy->Initialize(worker_clients, ModuleResponsesMap());
  return proxy;
}

WorkletAnimationId AnimationWorklet::NextWorkletAnimationId() {
  // Id starts from 1. This way it safe to use it as key in hashmap with default
  // key traits.
  return {.scope_id = scope_id_, .animation_id = ++last_animation_id_};
}

void AnimationWorklet::Trace(blink::Visitor* visitor) {
  Worklet::Trace(visitor);
}

}  // namespace blink
