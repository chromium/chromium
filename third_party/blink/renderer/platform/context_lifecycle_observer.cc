// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/context_lifecycle_observer.h"

#include "third_party/blink/renderer/platform/context_lifecycle_notifier.h"

namespace blink {

void ContextLifecycleObserver::ObserverListWillBeCleared() {
  notifier_ = nullptr;
}

void ContextLifecycleObserver::SetContextLifecycleNotifier(
    ContextLifecycleNotifier* notifier) {
  if (notifier == notifier_)
    return;

  if (notifier_)
    notifier_->RemoveContextLifecycleObserver(this);

  notifier_ = notifier;

  if (notifier_)
    notifier_->AddContextLifecycleObserver(this);
}

void ContextLifecycleObserver::Trace(Visitor* visitor) const {
  visitor->Trace(notifier_);
}

}  // namespace blink
