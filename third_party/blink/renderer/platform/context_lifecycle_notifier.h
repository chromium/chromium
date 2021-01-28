// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_CONTEXT_LIFECYCLE_NOTIFIER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_CONTEXT_LIFECYCLE_NOTIFIER_H_

#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class ContextLifecycleObserver;

// Notifier interface for ContextLifecycleObserver.
class PLATFORM_EXPORT ContextLifecycleNotifier : public GarbageCollectedMixin {
 public:
  virtual void AddContextLifecycleObserver(ContextLifecycleObserver*) = 0;
  virtual void RemoveContextLifecycleObserver(ContextLifecycleObserver*) = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_CONTEXT_LIFECYCLE_NOTIFIER_H_
