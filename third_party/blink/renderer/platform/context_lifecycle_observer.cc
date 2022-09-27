// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/context_lifecycle_observer.h"

#include "third_party/blink/renderer/platform/context_lifecycle_notifier.h"

namespace blink {

ContextLifecycleObserver::~ContextLifecycleObserver() {
#if DCHECK_IS_ON()
  // We want to make sure that if we are still waiting for a notification,
  // then the context hasn't been GC'ed (or, in other words, if the WeakPtr is
  // reset then `ContextDestroyed()` has been called).
  // waiting_for_context_destroyed_ -> notifier_
  // !waiting_for_context_destroyed_ || notifier_
  DCHECK(!waiting_for_context_destroyed_ || notifier_);
#endif
}

void ContextLifecycleObserver::SetContextLifecycleNotifier(
    ContextLifecycleNotifier* notifier) {
  if (notifier == notifier_)
    return;

  if (notifier_)
    notifier_->RemoveContextLifecycleObserver(this);

  notifier_ = notifier;

#if DCHECK_IS_ON()
  // If the notifier is not null we expect it to notify us when it is destroyed.
  waiting_for_context_destroyed_ = !!notifier_;
#endif

  if (notifier_)
    notifier_->AddContextLifecycleObserver(this);
}

void ContextLifecycleObserver::NotifyContextDestroyed() {
#if DCHECK_IS_ON()
  DCHECK(waiting_for_context_destroyed_);
  waiting_for_context_destroyed_ = false;
#endif
  ContextDestroyed();
  notifier_ = nullptr;
}

void ContextLifecycleObserver::Trace(Visitor* visitor) const {
  visitor->Trace(notifier_);
}

}  // namespace blink
