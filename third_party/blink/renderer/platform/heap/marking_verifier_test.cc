// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/marking_verifier.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/heap_test_utilities.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

#if DCHECK_IS_ON()
class MarkingVerifierDeathTest : public TestSupportingGC {};

namespace {

class ResurrectingPreFinalizer
    : public GarbageCollected<ResurrectingPreFinalizer> {
  USING_PRE_FINALIZER(ResurrectingPreFinalizer, Dispose);

 public:
  enum TestType {
    kMember,
    kWeakMember,
  };

  class GlobalStorage : public GarbageCollected<GlobalStorage> {
   public:
    void Trace(Visitor* visitor) {
      visitor->Trace(strong);
      visitor->Trace(weak);
    }

    Member<LinkedObject> strong;
    WeakMember<LinkedObject> weak;
  };

  ResurrectingPreFinalizer(TestType test_type,
                           GlobalStorage* storage,
                           LinkedObject* object_that_dies)
      : test_type_(test_type),
        storage_(storage),
        object_that_dies_(object_that_dies) {}

  void Trace(Visitor* visitor) {
    visitor->Trace(storage_);
    visitor->Trace(object_that_dies_);
  }

 private:
  void Dispose() {
    switch (test_type_) {
      case TestType::kMember:
        storage_->strong = object_that_dies_;
        break;
      case TestType::kWeakMember:
        storage_->weak = object_that_dies_;
        break;
    }
  }

  TestType test_type_;
  Member<GlobalStorage> storage_;
  Member<LinkedObject> object_that_dies_;
};

}  // namespace

TEST_F(MarkingVerifierDeathTest, DiesOnResurrectedMember) {
  if (!ThreadState::Current()->IsVerifyMarkingEnabled())
    return;

  Persistent<ResurrectingPreFinalizer::GlobalStorage> storage(
      MakeGarbageCollected<ResurrectingPreFinalizer::GlobalStorage>());
  MakeGarbageCollected<ResurrectingPreFinalizer>(
      ResurrectingPreFinalizer::kMember, storage.Get(),
      MakeGarbageCollected<LinkedObject>());
  ASSERT_DEATH_IF_SUPPORTED(PreciselyCollectGarbage(),
                            "MarkingVerifier: Encountered unmarked object.");
}

TEST_F(MarkingVerifierDeathTest, DiesOnResurrectedWeakMember) {
  if (!ThreadState::Current()->IsVerifyMarkingEnabled())
    return;

  Persistent<ResurrectingPreFinalizer::GlobalStorage> storage(
      MakeGarbageCollected<ResurrectingPreFinalizer::GlobalStorage>());
  MakeGarbageCollected<ResurrectingPreFinalizer>(
      ResurrectingPreFinalizer::kWeakMember, storage.Get(),
      MakeGarbageCollected<LinkedObject>());
  ASSERT_DEATH_IF_SUPPORTED(PreciselyCollectGarbage(),
                            "MarkingVerifier: Encountered unmarked object.");
}
#endif  // DCHECK_IS_ON()

}  // namespace blink
