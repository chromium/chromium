// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/sequence_test.h"

namespace blink {

SequenceTest::SequenceTest() = default;

SequenceTest::~SequenceTest() = default;

Vector<Vector<String>> SequenceTest::identityByteStringSequenceSequence(
    const Vector<Vector<String>>& arg) const {
  return arg;
}

Vector<double> SequenceTest::identityDoubleSequence(
    const Vector<double>& arg) const {
  return arg;
}

Vector<V8FoodEnum> SequenceTest::identityFoodEnumSequence(
    const Vector<V8FoodEnum>& arg) const {
  return arg;
}

Vector<int32_t> SequenceTest::identityLongSequence(
    const Vector<int32_t>& arg) const {
  return arg;
}

base::Optional<Vector<uint8_t>> SequenceTest::identityOctetSequenceOrNull(
    const base::Optional<Vector<uint8_t>>& arg) const {
  return arg;
}

HeapVector<Member<Element>> SequenceTest::getElementSequence() const {
  return element_sequence_;
}

void SequenceTest::setElementSequence(const HeapVector<Member<Element>>& arg) {
  element_sequence_ = arg;
}

bool SequenceTest::unionReceivedSequence(const DoubleOrDoubleSequence& arg) {
  return arg.IsDoubleSequence();
}

void SequenceTest::Trace(Visitor* visitor) const {
  visitor->Trace(element_sequence_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
