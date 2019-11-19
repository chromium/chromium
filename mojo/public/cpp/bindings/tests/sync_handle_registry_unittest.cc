// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "mojo/public/cpp/bindings/sync_handle_registry.h"
#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/waitable_event.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {

class SyncHandleRegistryTest : public testing::Test {
 public:
  SyncHandleRegistryTest() : registry_(SyncHandleRegistry::current()) {}

  const scoped_refptr<SyncHandleRegistry>& registry() { return registry_; }

 private:
  scoped_refptr<SyncHandleRegistry> registry_;

  DISALLOW_COPY_AND_ASSIGN(SyncHandleRegistryTest);
};

TEST_F(SyncHandleRegistryTest, DuplicateEventRegistration) {
  bool called1 = false;
  bool called2 = false;
  auto callback = [](bool* called) { *called = true; };
  auto callback1 = base::BindRepeating(callback, &called1);
  auto callback2 = base::BindRepeating(callback, &called2);

  base::WaitableEvent e(base::WaitableEvent::ResetPolicy::MANUAL,
                        base::WaitableEvent::InitialState::SIGNALED);
  registry()->RegisterEvent(&e, callback1);
  registry()->RegisterEvent(&e, callback2);

  const bool* stop_flags[] = {&called1, &called2};
  registry()->Wait(stop_flags, 2);

  EXPECT_TRUE(called1);
  EXPECT_TRUE(called2);
  registry()->UnregisterEvent(&e, callback1);

  called1 = false;
  called2 = false;

  registry()->Wait(stop_flags, 2);

  EXPECT_FALSE(called1);
  EXPECT_TRUE(called2);

  registry()->UnregisterEvent(&e, callback2);
}

TEST_F(SyncHandleRegistryTest, UnregisterDuplicateEventInNestedWait) {
  base::WaitableEvent e(base::WaitableEvent::ResetPolicy::MANUAL,
                        base::WaitableEvent::InitialState::SIGNALED);
  bool called1 = false;
  bool called2 = false;
  bool called3 = false;
  auto callback1 =
      base::BindRepeating([](bool* called) { *called = true; }, &called1);
  auto callback2 = base::BindRepeating(
      [](base::WaitableEvent* e, base::RepeatingClosure other_callback,
         scoped_refptr<SyncHandleRegistry> registry, bool* called) {
        registry->UnregisterEvent(e, other_callback);
        *called = true;
      },
      &e, callback1, registry(), &called2);
  auto callback3 =
      base::BindRepeating([](bool* called) { *called = true; }, &called3);

  registry()->RegisterEvent(&e, callback1);
  registry()->RegisterEvent(&e, callback2);
  registry()->RegisterEvent(&e, callback3);

  const bool* stop_flags[] = {&called1, &called2, &called3};
  registry()->Wait(stop_flags, 3);

  // We don't make any assumptions about the order in which callbacks run, so
  // we can't check |called1| - it may or may not get set depending on internal
  // details. All we know is |called2| should be set, and a subsequent wait
  // should definitely NOT set |called1|.
  EXPECT_TRUE(called2);
  EXPECT_TRUE(called3);

  called1 = false;
  called2 = false;
  called3 = false;

  registry()->UnregisterEvent(&e, callback2);
  registry()->Wait(stop_flags, 3);

  EXPECT_FALSE(called1);
  EXPECT_FALSE(called2);
  EXPECT_TRUE(called3);
}

TEST_F(SyncHandleRegistryTest, UnregisterAndRegisterForNewEventInCallback) {
  auto e = std::make_unique<base::WaitableEvent>(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::SIGNALED);
  bool called = false;
  base::RepeatingClosure callback_holder;
  auto callback = base::BindRepeating(
      [](std::unique_ptr<base::WaitableEvent>* e,
         base::RepeatingClosure* callback_holder,
         scoped_refptr<SyncHandleRegistry> registry, bool* called) {
        EXPECT_FALSE(*called);

        registry->UnregisterEvent(e->get(), *callback_holder);
        e->reset();
        *called = true;

        base::WaitableEvent nested_event(
            base::WaitableEvent::ResetPolicy::MANUAL,
            base::WaitableEvent::InitialState::SIGNALED);
        bool nested_called = false;
        auto nested_callback = base::BindRepeating(
            [](bool* called) { *called = true; }, &nested_called);
        registry->RegisterEvent(&nested_event, nested_callback);
        const bool* stop_flag = &nested_called;
        registry->Wait(&stop_flag, 1);
        registry->UnregisterEvent(&nested_event, nested_callback);
      },
      &e, &callback_holder, registry(), &called);
  callback_holder = callback;

  registry()->RegisterEvent(e.get(), callback);

  const bool* stop_flag = &called;
  registry()->Wait(&stop_flag, 1);
  EXPECT_TRUE(called);
}

TEST_F(SyncHandleRegistryTest, UnregisterAndRegisterForSameEventInCallback) {
  base::WaitableEvent e(base::WaitableEvent::ResetPolicy::MANUAL,
                        base::WaitableEvent::InitialState::SIGNALED);
  bool called = false;
  base::RepeatingClosure callback_holder;
  auto callback = base::BindRepeating(
      [](base::WaitableEvent* e, base::RepeatingClosure* callback_holder,
         scoped_refptr<SyncHandleRegistry> registry, bool* called) {
        EXPECT_FALSE(*called);

        registry->UnregisterEvent(e, *callback_holder);
        *called = true;

        bool nested_called = false;
        auto nested_callback = base::BindRepeating(
            [](bool* called) { *called = true; }, &nested_called);
        registry->RegisterEvent(e, nested_callback);
        const bool* stop_flag = &nested_called;
        registry->Wait(&stop_flag, 1);
        registry->UnregisterEvent(e, nested_callback);

        EXPECT_TRUE(nested_called);
      },
      &e, &callback_holder, registry(), &called);
  callback_holder = callback;

  registry()->RegisterEvent(&e, callback);

  const bool* stop_flag = &called;
  registry()->Wait(&stop_flag, 1);
  EXPECT_TRUE(called);
}

TEST_F(SyncHandleRegistryTest, RegisterDuplicateEventFromWithinCallback) {
  base::WaitableEvent e(base::WaitableEvent::ResetPolicy::MANUAL,
                        base::WaitableEvent::InitialState::SIGNALED);
  bool called = false;
  int call_count = 0;
  auto callback = base::BindRepeating(
      [](base::WaitableEvent* e, scoped_refptr<SyncHandleRegistry> registry,
         bool* called, int* call_count) {
        // Don't re-enter.
        ++(*call_count);
        if (*called)
          return;

        *called = true;

        bool called2 = false;
        auto callback2 =
            base::BindRepeating([](bool* called) { *called = true; }, &called2);
        registry->RegisterEvent(e, callback2);

        const bool* stop_flag = &called2;
        registry->Wait(&stop_flag, 1);

        registry->UnregisterEvent(e, callback2);
      },
      &e, registry(), &called, &call_count);

  registry()->RegisterEvent(&e, callback);

  const bool* stop_flag = &called;
  registry()->Wait(&stop_flag, 1);

  EXPECT_TRUE(called);
  EXPECT_EQ(2, call_count);

  registry()->UnregisterEvent(&e, callback);
}

TEST_F(SyncHandleRegistryTest, UnregisterUniqueEventInNestedWait) {
  auto e1 = std::make_unique<base::WaitableEvent>(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  base::WaitableEvent e2(base::WaitableEvent::ResetPolicy::MANUAL,
                         base::WaitableEvent::InitialState::SIGNALED);
  bool called1 = false;
  bool called2 = false;
  auto callback1 =
      base::BindRepeating([](bool* called) { *called = true; }, &called1);
  auto callback2 = base::BindRepeating(
      [](std::unique_ptr<base::WaitableEvent>* e1,
         base::RepeatingClosure other_callback,
         scoped_refptr<SyncHandleRegistry> registry, bool* called) {
        // Prevent re-entrancy.
        if (*called)
          return;

        registry->UnregisterEvent(e1->get(), other_callback);
        *called = true;
        e1->reset();

        // Nest another wait.
        bool called3 = false;
        auto callback3 =
            base::BindRepeating([](bool* called) { *called = true; }, &called3);
        base::WaitableEvent e3(base::WaitableEvent::ResetPolicy::MANUAL,
                               base::WaitableEvent::InitialState::SIGNALED);
        registry->RegisterEvent(&e3, callback3);

        // This nested Wait() must not attempt to wait on |e1| since it has
        // been unregistered. This would crash otherwise, since |e1| has been
        // deleted. See http://crbug.com/761097.
        const bool* stop_flags[] = {&called3};
        registry->Wait(stop_flags, 1);

        EXPECT_TRUE(called3);
        registry->UnregisterEvent(&e3, callback3);
      },
      &e1, callback1, registry(), &called2);

  registry()->RegisterEvent(e1.get(), callback1);
  registry()->RegisterEvent(&e2, callback2);

  const bool* stop_flags[] = {&called1, &called2};
  registry()->Wait(stop_flags, 2);

  EXPECT_TRUE(called2);

  registry()->UnregisterEvent(&e2, callback2);
}

}  // namespace mojo
