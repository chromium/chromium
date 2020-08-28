// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_CONTEXT_LIFECYCLE_OBSERVER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_CONTEXT_LIFECYCLE_OBSERVER_H_

#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class ContextLifecycleNotifier;

// Observer that gets notified when the context is destroyed. Used to observe
// ExecutionContext from platform/.
class PLATFORM_EXPORT ContextLifecycleObserver : public GarbageCollectedMixin {
 public:
  virtual void ContextDestroyed() = 0;

  // Call before clearing an observer list.
  void ObserverListWillBeCleared();

  ContextLifecycleNotifier* GetContextLifecycleNotifier() const {
    return notifier_;
  }
  void SetContextLifecycleNotifier(ContextLifecycleNotifier*);

  virtual bool IsExecutionContextLifecycleObserver() const { return false; }

  void Trace(Visitor*) const override;

 protected:
  ContextLifecycleObserver() = default;

 private:
  WeakMember<ContextLifecycleNotifier> notifier_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_CONTEXT_LIFECYCLE_OBSERVER_H_
