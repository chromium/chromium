// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_MOCK_CONTEXT_LIFECYCLE_NOTIFIER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_MOCK_CONTEXT_LIFECYCLE_NOTIFIER_H_

#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/mojo_binding_context.h"
#include "third_party/blink/renderer/platform/context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/heap_observer_set.h"

namespace blink {

class MockContextLifecycleNotifier final
    : public GarbageCollected<MockContextLifecycleNotifier>,
      public MojoBindingContext {
 public:
  MockContextLifecycleNotifier() = default;

  void AddContextLifecycleObserver(
      ContextLifecycleObserver* observer) override {
    observers_.AddObserver(observer);
  }
  void RemoveContextLifecycleObserver(
      ContextLifecycleObserver* observer) override {
    observers_.RemoveObserver(observer);
  }

  void NotifyContextDestroyed() {
    observers_.ForEachObserver([](ContextLifecycleObserver* observer) {
      observer->NotifyContextDestroyed();
    });
  }

  const BrowserInterfaceBrokerProxy& GetBrowserInterfaceBroker()
      const override {
    return GetEmptyBrowserInterfaceBroker();
  }

  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(TaskType) override {
    return nullptr;
  }

  void Trace(Visitor* visitor) const { visitor->Trace(observers_); }

 private:
  HeapObserverSet<ContextLifecycleObserver> observers_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_MOCK_CONTEXT_LIFECYCLE_NOTIFIER_H_
