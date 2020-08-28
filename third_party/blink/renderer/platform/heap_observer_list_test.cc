/*
 * Copyright (C) 2013 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "third_party/blink/renderer/platform/heap_observer_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"

namespace blink {

class TestingObserver;

class TestingNotifier final : public GarbageCollected<TestingNotifier> {
 public:
  TestingNotifier() = default;

  HeapObserverList<TestingObserver>& ObserverList() { return observer_list_; }

  void Trace(Visitor* visitor) const { visitor->Trace(observer_list_); }

 private:
  HeapObserverList<TestingObserver> observer_list_;
};

class TestingObserver final : public GarbageCollected<TestingObserver> {
 public:
  TestingObserver() = default;
  void OnNotification() { count_++; }
  int Count() { return count_; }
  void Trace(Visitor* visitor) const {}

 private:
  int count_ = 0;
};

void Notify(HeapObserverList<TestingObserver>& observer_list) {
  observer_list.ForEachObserver(
      [](TestingObserver* observer) { observer->OnNotification(); });
}

TEST(HeapObserverListTest, AddRemove) {
  Persistent<TestingNotifier> notifier =
      MakeGarbageCollected<TestingNotifier>();
  Persistent<TestingObserver> observer =
      MakeGarbageCollected<TestingObserver>();

  notifier->ObserverList().AddObserver(observer);

  EXPECT_EQ(observer->Count(), 0);
  Notify(notifier->ObserverList());
  EXPECT_EQ(observer->Count(), 1);

  notifier->ObserverList().RemoveObserver(observer);

  Notify(notifier->ObserverList());
  EXPECT_EQ(observer->Count(), 1);
}

TEST(HeapObserverListTest, HasObserver) {
  Persistent<TestingNotifier> notifier =
      MakeGarbageCollected<TestingNotifier>();
  Persistent<TestingObserver> observer =
      MakeGarbageCollected<TestingObserver>();

  EXPECT_FALSE(notifier->ObserverList().HasObserver(observer));

  notifier->ObserverList().AddObserver(observer);
  EXPECT_TRUE(notifier->ObserverList().HasObserver(observer.Get()));

  notifier->ObserverList().RemoveObserver(observer);
  EXPECT_FALSE(notifier->ObserverList().HasObserver(observer.Get()));
}

TEST(HeapObserverListTest, GarbageCollect) {
  Persistent<TestingNotifier> notifier =
      MakeGarbageCollected<TestingNotifier>();
  Persistent<TestingObserver> observer =
      MakeGarbageCollected<TestingObserver>();
  notifier->ObserverList().AddObserver(observer);

  ThreadState::Current()->CollectAllGarbageForTesting();
  Notify(notifier->ObserverList());
  EXPECT_EQ(observer->Count(), 1);

  WeakPersistent<TestingObserver> weak_ref = observer.Get();
  observer = nullptr;
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(weak_ref.Get(), nullptr);
}

TEST(HeapObserverListTest, IsIteratingOverObservers) {
  Persistent<TestingNotifier> notifier =
      MakeGarbageCollected<TestingNotifier>();
  Persistent<TestingObserver> observer =
      MakeGarbageCollected<TestingObserver>();
  notifier->ObserverList().AddObserver(observer);

  EXPECT_FALSE(notifier->ObserverList().IsIteratingOverObservers());
  notifier->ObserverList().ForEachObserver([&](TestingObserver* observer) {
    EXPECT_TRUE(notifier->ObserverList().IsIteratingOverObservers());
  });
}

TEST(HeapObserverListTest, ForEachObserverOrder) {
  Persistent<TestingNotifier> notifier =
      MakeGarbageCollected<TestingNotifier>();
  Persistent<TestingObserver> observer1 =
      MakeGarbageCollected<TestingObserver>();
  Persistent<TestingObserver> observer2 =
      MakeGarbageCollected<TestingObserver>();

  HeapVector<Member<TestingObserver>> seen_observers;

  notifier->ObserverList().AddObserver(observer1);
  notifier->ObserverList().AddObserver(observer2);
  notifier->ObserverList().ForEachObserver(
      [&](TestingObserver* observer) { seen_observers.push_back(observer); });

  ASSERT_EQ(2u, seen_observers.size());
  EXPECT_EQ(observer1.Get(), seen_observers[0].Get());
  EXPECT_EQ(observer2.Get(), seen_observers[1].Get());

  seen_observers.clear();

  notifier->ObserverList().RemoveObserver(observer1);
  notifier->ObserverList().AddObserver(observer1);
  notifier->ObserverList().ForEachObserver(
      [&](TestingObserver* observer) { seen_observers.push_back(observer); });

  ASSERT_EQ(2u, seen_observers.size());
  EXPECT_EQ(observer2.Get(), seen_observers[0].Get());
  EXPECT_EQ(observer1.Get(), seen_observers[1].Get());

  seen_observers.clear();

  notifier->ObserverList().Clear();
  notifier->ObserverList().ForEachObserver(
      [&](TestingObserver* observer) { seen_observers.push_back(observer); });
  ASSERT_EQ(0u, seen_observers.size());
}

}  // namespace blink
