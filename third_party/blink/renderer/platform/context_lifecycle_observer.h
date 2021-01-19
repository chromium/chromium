// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_CONTEXT_LIFECYCLE_OBSERVER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_CONTEXT_LIFECYCLE_OBSERVER_H_

#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class MojoBindingContext;

// Observer that gets notified when the context is destroyed. Used to observe
// ExecutionContext from platform/.
class PLATFORM_EXPORT ContextLifecycleObserver : public GarbageCollectedMixin {
 public:
  virtual ~ContextLifecycleObserver();

  void NotifyContextDestroyed();

  MojoBindingContext* GetContext() const { return context_; }
  void SetContext(MojoBindingContext*);

  virtual bool IsExecutionContextLifecycleObserver() const { return false; }

  void Trace(Visitor*) const override;

 protected:
  ContextLifecycleObserver() = default;

  virtual void ContextDestroyed() = 0;

 private:
  // `context_` is reset in `NotifyContextDestroyed()`, which is guaranteed to
  // be called, so this will never cause a use-after-free.
  MojoBindingContext* context_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_CONTEXT_LIFECYCLE_OBSERVER_H_
