// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_CONTEXT_LIFECYCLE_NOTIFIER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_CONTEXT_LIFECYCLE_NOTIFIER_H_

#include "base/dcheck_is_on.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap_observer_set.h"

namespace blink {

class ContextLifecycleObserver;

// Notifier interface for ContextLifecycleObserver.
class PLATFORM_EXPORT ContextLifecycleNotifier : public GarbageCollectedMixin {
 public:
  virtual ~ContextLifecycleNotifier();

  virtual void AddContextLifecycleObserver(ContextLifecycleObserver*);
  virtual void RemoveContextLifecycleObserver(ContextLifecycleObserver*);

  void Trace(Visitor* visitor) const override;

  bool IsContextDestroyed() const;

 protected:
  // Should be called by implementers to notify observers when the context is
  // destroyed.
  void NotifyContextDestroyed();

  const HeapObserverSet<ContextLifecycleObserver>& observers() const {
    return observers_;
  }
  HeapObserverSet<ContextLifecycleObserver>& observers() { return observers_; }

 private:
  HeapObserverSet<ContextLifecycleObserver> observers_;
  bool context_destroyed_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_CONTEXT_LIFECYCLE_NOTIFIER_H_
