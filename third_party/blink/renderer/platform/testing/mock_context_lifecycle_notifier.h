// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_MOCK_CONTEXT_LIFECYCLE_NOTIFIER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_MOCK_CONTEXT_LIFECYCLE_NOTIFIER_H_

#include "third_party/blink/renderer/platform/context_lifecycle_notifier.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

class MockContextLifecycleNotifier final
    : public GarbageCollected<MockContextLifecycleNotifier>,
      public ContextLifecycleNotifier {
 public:
  MockContextLifecycleNotifier() = default;

  // Make `ContextLifecycleNotifier::NotifyContextDestroyed()` public, so that
  // it can be called by `ScopedMockContextLifecycleNotifier` and tests.
  using ContextLifecycleNotifier::NotifyContextDestroyed;

  void Trace(Visitor* visitor) const override {
    ContextLifecycleNotifier::Trace(visitor);
  }
};

class ScopedMockContextLifecycleNotifier final {
 public:
  ScopedMockContextLifecycleNotifier()
      : notifier_(MakeGarbageCollected<MockContextLifecycleNotifier>()) {}
  ScopedMockContextLifecycleNotifier(
      const ScopedMockContextLifecycleNotifier&) = delete;
  ScopedMockContextLifecycleNotifier& operator=(
      const ScopedMockContextLifecycleNotifier&) = delete;

  ~ScopedMockContextLifecycleNotifier() { Reset(); }

  MockContextLifecycleNotifier* Notifier() { return notifier_.Get(); }

  MockContextLifecycleNotifier* operator->() { return notifier_.Get(); }

  void Reset() {
    if (!notifier_) {
      return;
    }
    if (!notifier_->IsContextDestroyed()) {
      notifier_->NotifyContextDestroyed();
    }
    notifier_.Clear();
  }

 private:
  Persistent<MockContextLifecycleNotifier> notifier_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_MOCK_CONTEXT_LIFECYCLE_NOTIFIER_H_
