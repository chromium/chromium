// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/context_lifecycle_observer.h"

#include "third_party/blink/public/platform/mojo_binding_context.h"

namespace blink {

ContextLifecycleObserver::~ContextLifecycleObserver() = default;

void ContextLifecycleObserver::SetContext(MojoBindingContext* context) {
  if (context == context_)
    return;

  if (context_)
    context_->RemoveContextLifecycleObserver(this);

  context_ = context;

  if (context_)
    context_->AddContextLifecycleObserver(this);
}

void ContextLifecycleObserver::NotifyContextDestroyed() {
  ContextDestroyed();
  context_ = nullptr;
}

void ContextLifecycleObserver::Trace(Visitor* visitor) const {}

}  // namespace blink
