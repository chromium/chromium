// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/context_lifecycle_notifier.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/heap_test_utilities.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/testing/mock_context_lifecycle_notifier.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {
namespace {

class CallbackContextLifecycleObserver final
    : public GarbageCollected<CallbackContextLifecycleObserver>,
      public ContextLifecycleObserver {
 public:
  CallbackContextLifecycleObserver(ContextLifecycleNotifier* notifier,
                                   base::OnceClosure on_destroyed)
      : on_destroyed_(std::move(on_destroyed)) {
    SetContextLifecycleNotifier(notifier);
  }

  bool notified() const { return notified_; }

  void Trace(Visitor* visitor) const override {
    ContextLifecycleObserver::Trace(visitor);
  }

 private:
  void ContextDestroyed() override {
    notified_ = true;
    if (on_destroyed_) {
      std::move(on_destroyed_).Run();
    }
  }

  bool notified_ = false;
  base::OnceClosure on_destroyed_;
};

class ContextLifecycleNotifierTest : public TestSupportingGC {};

TEST_F(ContextLifecycleNotifierTest,
       NotifyContextDestroyedKeepsObserversAliveAcrossGC) {
  Persistent<MockContextLifecycleNotifier> notifier =
      MakeGarbageCollected<MockContextLifecycleNotifier>();

  Persistent<CallbackContextLifecycleObserver> second_observer;

  // Register `first_observer` first so its `ContextDestroyed()` runs before
  // `second_observer` is notified. Simulate an observer whose
  // `ContextDestroyed()` triggers an Oilpan GC after `IsContextDestroyed()` has
  // become true (at which point mechanisms like `ActiveScriptWrappableManager`
  // drop their strong references to remaining observers).
  Persistent<CallbackContextLifecycleObserver> first_observer =
      MakeGarbageCollected<CallbackContextLifecycleObserver>(
          notifier, blink::BindOnce(
                        [](MockContextLifecycleNotifier* notifier,
                           Persistent<CallbackContextLifecycleObserver>*
                               second_keep_alive) {
                          EXPECT_TRUE(notifier->IsContextDestroyed());
                          second_keep_alive->Clear();
                          TestSupportingGC::ConservativelyCollectGarbage();
                        },
                        WrapWeakPersistent(notifier.Get()),
                        blink::Unretained(&second_observer)));

  bool second_notified = false;
  second_observer = MakeGarbageCollected<CallbackContextLifecycleObserver>(
      notifier, blink::BindOnce([](bool* flag) { *flag = true; },
                                blink::Unretained(&second_notified)));
  WeakPersistent<CallbackContextLifecycleObserver> weak_second =
      second_observer.Get();

  notifier->NotifyContextDestroyed();

  EXPECT_TRUE(first_observer->notified());
  EXPECT_TRUE(second_notified);
  EXPECT_EQ(second_observer.Get(), nullptr);

  PreciselyCollectGarbage();
  EXPECT_EQ(weak_second.Get(), nullptr);
}

}  // namespace
}  // namespace blink
