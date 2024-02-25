// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/weak_cell.h"

#include "base/functional/function_ref.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/heap_test_utilities.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

class TestClass : public GarbageCollected<TestClass> {
 public:
  WeakCell<TestClass>* GetWeakCell() { return weak_factory_.GetWeakCell(); }

  void Method(base::FunctionRef<void()> fn) const { fn(); }

  void InvalidateCell() { weak_factory_.Invalidate(); }

  void Trace(Visitor* v) const { v->Trace(weak_factory_); }

 private:
  WeakCellFactory<TestClass> weak_factory_{this};
};

}  // namespace

class WeakCellTest : public TestSupportingGC {};

TEST_F(WeakCellTest, Finalization) {
  TestClass* tester = MakeGarbageCollected<TestClass>();

  Persistent<WeakCell<TestClass>> weak_cell = tester->GetWeakCell();
  tester = nullptr;
  PreciselyCollectGarbage();

  // WeakCell should:
  // - not keep its referenced object alive and
  // - become null after its referenced object is no longer reachable.
  EXPECT_EQ(nullptr, weak_cell->Get());
}

TEST_F(WeakCellTest, Invalidation) {
  TestClass* tester = MakeGarbageCollected<TestClass>();

  WeakCell<TestClass>* original_weak_cell = tester->GetWeakCell();
  tester->InvalidateCell();
  // Even though `tester` is still alive, an invalidated WeakCell should return
  // nullptr.
  EXPECT_EQ(nullptr, original_weak_cell->Get());

  // However, getting a new WeakCell should return `tester.`
  WeakCell<TestClass>* new_weak_cell = tester->GetWeakCell();
  EXPECT_EQ(tester, new_weak_cell->Get());
  // While the original weak cell should remain null.
  EXPECT_EQ(nullptr, original_weak_cell->Get());
}

TEST_F(WeakCellTest, Callback) {
  // Verify that `WeakCell<T>` can be used as a callback receiver.
  TestClass* tester = MakeGarbageCollected<TestClass>();

  auto callback =
      WTF::BindOnce(&TestClass::Method, WrapPersistent(tester->GetWeakCell()));
  bool did_run = false;
  std::move(callback).Run([&] { did_run = true; });
  EXPECT_TRUE(did_run);
}

TEST_F(WeakCellTest, FinalizationCancelsCallback) {
  TestClass* tester = MakeGarbageCollected<TestClass>();

  auto callback =
      WTF::BindOnce(&TestClass::Method, WrapPersistent(tester->GetWeakCell()));
  tester = nullptr;
  PreciselyCollectGarbage();

  bool did_run = false;
  std::move(callback).Run([&] { did_run = true; });
  EXPECT_FALSE(did_run);
}

TEST_F(WeakCellTest, InvalidationCancelsCallback) {
  TestClass* tester = MakeGarbageCollected<TestClass>();

  auto callback =
      WTF::BindOnce(&TestClass::Method, WrapPersistent(tester->GetWeakCell()));
  tester->InvalidateCell();

  bool did_run = false;
  std::move(callback).Run([&] { did_run = true; });
  EXPECT_FALSE(did_run);
}

}  // namespace blink
