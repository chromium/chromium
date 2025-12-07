// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/heap_traits.h"

#include <type_traits>
#include <utility>
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

// No gtest tests; only static_assert checks.

namespace blink {

namespace {

struct Empty {};

// Similar to an IDL union or dictionary, which have Trace() methods but are
// not garbage-collected types themselves.
struct StructWithTraceMethod {
  void Trace(Visitor*) const {}
};

struct GarbageCollectedStruct
    : public GarbageCollected<GarbageCollectedStruct> {
  void Trace(Visitor*) const {}
};

// AddMemberIfNeeded<T>
static_assert(std::is_same<AddMemberIfNeeded<double>, double>::value,
              "AddMemberIfNeeded<double> must not add a Member wrapper");
static_assert(std::is_same<AddMemberIfNeeded<double*>, double*>::value,
              "AddMemberIfNeeded<double*> must not add a Member wrapper");

static_assert(std::is_same<AddMemberIfNeeded<Empty>, Empty>::value,
              "AddMemberIfNeeded<Empty> must not add a Member wrapper");

static_assert(
    std::is_same<AddMemberIfNeeded<StructWithTraceMethod>,
                 StructWithTraceMethod>::value,
    "AddMemberIfNeeded<StructWithTraceMethod> must not add a Member wrapper");

static_assert(
    std::is_same<AddMemberIfNeeded<GarbageCollectedStruct>,
                 Member<GarbageCollectedStruct>>::value,
    "AddMemberIfNeeded<GarbageCollectedStruct> must not add a Member wrapper");

static_assert(
    std::is_same<AddMemberIfNeeded<HeapVector<Member<GarbageCollectedStruct>>>,
                 HeapVector<Member<GarbageCollectedStruct>>>::value,
    "AddMemberIfNeeded on a HeapVector<Member<T>> must not wrap it in a "
    "Member<>");

// VectorOf<T>
static_assert(std::is_same<VectorOf<double>, Vector<double>>::value,
              "VectorOf<double> should use a Vector");
static_assert(std::is_same<VectorOf<double*>, Vector<double*>>::value,
              "VectorOf<double*> should use a Vector");
static_assert(std::is_same<VectorOf<Empty>, Vector<Empty>>::value,
              "VectorOf<Empty> should use a Vector");

static_assert(
    std::is_same<VectorOf<StructWithTraceMethod>,
                 HeapVector<StructWithTraceMethod>>::value,
    "VectorOf<StructWithTraceMethod> must not add a Member<> wrapper");
static_assert(std::is_same<VectorOf<GarbageCollectedStruct>,
                           HeapVector<Member<GarbageCollectedStruct>>>::value,
              "VectorOf<GarbageCollectedStruct> must add a Member<> wrapper");

static_assert(
    std::is_same<VectorOf<Vector<double>>, Vector<Vector<double>>>::value,
    "Nested Vectors must not add HeapVectors");
static_assert(
    std::is_same<VectorOf<HeapVector<StructWithTraceMethod>>,
                 HeapVector<HeapVector<StructWithTraceMethod>>>::value,
    "Nested HeapVector<StructWithTraceMethod> must add a HeapVector");
static_assert(
    std::is_same<VectorOf<HeapVector<Member<GarbageCollectedStruct>>>,
                 HeapVector<HeapVector<Member<GarbageCollectedStruct>>>>::value,
    "Nested HeapVectors must not add Vectors");

// VectorOfPairs<T, U>
static_assert(std::is_same<VectorOfPairs<int, double>,
                           Vector<std::pair<int, double>>>::value,
              "POD types must use a regular Vector");
static_assert(std::is_same<VectorOfPairs<Empty, double>,
                           Vector<std::pair<Empty, double>>>::value,
              "POD types must use a regular Vector");

static_assert(
    std::is_same<VectorOfPairs<StructWithTraceMethod, float>,
                 HeapVector<std::pair<StructWithTraceMethod, float>>>::value,
    "StructWithTraceMethod causes a HeapVector to be used");
static_assert(
    std::is_same<VectorOfPairs<float, StructWithTraceMethod>,
                 HeapVector<std::pair<float, StructWithTraceMethod>>>::value,
    "StructWithTraceMethod causes a HeapVector to be used");
static_assert(
    std::is_same<VectorOfPairs<StructWithTraceMethod, StructWithTraceMethod>,
                 HeapVector<std::pair<StructWithTraceMethod,
                                      StructWithTraceMethod>>>::value,
    "StructWithTraceMethod causes a HeapVector to be used");

static_assert(
    std::is_same<
        VectorOfPairs<GarbageCollectedStruct, float>,
        HeapVector<std::pair<Member<GarbageCollectedStruct>, float>>>::value,
    "GarbageCollectedStruct causes a HeapVector to be used");
static_assert(
    std::is_same<
        VectorOfPairs<float, GarbageCollectedStruct>,
        HeapVector<std::pair<float, Member<GarbageCollectedStruct>>>>::value,
    "GarbageCollectedStruct causes a HeapVector to be used");
static_assert(
    std::is_same<VectorOfPairs<GarbageCollectedStruct, GarbageCollectedStruct>,
                 HeapVector<std::pair<Member<GarbageCollectedStruct>,
                                      Member<GarbageCollectedStruct>>>>::value,
    "GarbageCollectedStruct causes a HeapVector to be used");

}  // namespace

}  // namespace blink
