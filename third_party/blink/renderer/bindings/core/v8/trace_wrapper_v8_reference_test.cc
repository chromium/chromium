// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

namespace {

using TraceWrapperV8ReferenceTest = BindingTestSupportingGC;

class TraceWrapperV8ReferenceHolder final
    : public GarbageCollected<TraceWrapperV8ReferenceHolder> {
 public:
  TraceWrapperV8ReferenceHolder() = default;

  TraceWrapperV8ReferenceHolder(v8::Isolate* isolate,
                                v8::Local<v8::Value> value)
      : value_(isolate, value) {}

  TraceWrapperV8ReferenceHolder(TraceWrapperV8ReferenceHolder&& other)
      : value_(std::move(other.value_)) {}

  TraceWrapperV8ReferenceHolder(const TraceWrapperV8ReferenceHolder& other)
      : value_(other.value_) {}

  virtual void Trace(Visitor* visitor) { visitor->Trace(value_); }

  TraceWrapperV8Reference<v8::Value>* ref() { return &value_; }

 private:
  TraceWrapperV8Reference<v8::Value> value_;
};

void CreateObject(v8::Isolate* isolate,
                  Persistent<TraceWrapperV8ReferenceHolder>* holder,
                  v8::Persistent<v8::Value>* observer) {
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Value> value = v8::Object::New(isolate);
  *holder = MakeGarbageCollected<TraceWrapperV8ReferenceHolder>(isolate, value);
  observer->Reset(isolate, value);
  observer->SetWeak();
}

}  // namespace

TEST_F(TraceWrapperV8ReferenceTest, DefaultCtorIntializesAsEmpty) {
  Persistent<TraceWrapperV8ReferenceHolder> holder(
      MakeGarbageCollected<TraceWrapperV8ReferenceHolder>());
  CHECK(holder->ref()->IsEmpty());
}

TEST_F(TraceWrapperV8ReferenceTest, CtorWithValue) {
  V8TestingScope testing_scope;
  SetIsolate(testing_scope.GetIsolate());

  Persistent<TraceWrapperV8ReferenceHolder> holder1;
  v8::Persistent<v8::Value> observer;
  CreateObject(GetIsolate(), &holder1, &observer);

  CHECK(!holder1->ref()->IsEmpty());
  CHECK(!observer.IsEmpty());
  RunV8FullGC();
  CHECK(!holder1->ref()->IsEmpty());
  CHECK(!observer.IsEmpty());
  holder1->ref()->Clear();
  RunV8FullGC();
  CHECK(holder1->ref()->IsEmpty());
  CHECK(observer.IsEmpty());
}

TEST_F(TraceWrapperV8ReferenceTest, CopyOverEmpty) {
  V8TestingScope testing_scope;
  SetIsolate(testing_scope.GetIsolate());

  Persistent<TraceWrapperV8ReferenceHolder> holder1;
  v8::Persistent<v8::Value> observer1;
  CreateObject(GetIsolate(), &holder1, &observer1);
  Persistent<TraceWrapperV8ReferenceHolder> holder2;

  CHECK(!holder1->ref()->IsEmpty());
  CHECK(!holder2.Get());
  CHECK(!observer1.IsEmpty());
  holder2 = MakeGarbageCollected<TraceWrapperV8ReferenceHolder>(*holder1);
  CHECK(!holder1->ref()->IsEmpty());
  CHECK(*holder1->ref() == *holder2->ref());
  CHECK(!observer1.IsEmpty());
  RunV8FullGC();
  CHECK(!holder1->ref()->IsEmpty());
  CHECK(*holder1->ref() == *holder2->ref());
  CHECK(!observer1.IsEmpty());
  holder1.Clear();
  RunV8FullGC();
  CHECK(!holder2->ref()->IsEmpty());
  CHECK(!observer1.IsEmpty());
  holder2.Clear();
  RunV8FullGC();
  CHECK(observer1.IsEmpty());
}

TEST_F(TraceWrapperV8ReferenceTest, CopyOverNonEmpty) {
  V8TestingScope testing_scope;
  SetIsolate(testing_scope.GetIsolate());

  Persistent<TraceWrapperV8ReferenceHolder> holder1;
  v8::Persistent<v8::Value> observer1;
  CreateObject(GetIsolate(), &holder1, &observer1);
  Persistent<TraceWrapperV8ReferenceHolder> holder2;
  v8::Persistent<v8::Value> observer2;
  CreateObject(GetIsolate(), &holder2, &observer2);

  CHECK(!holder1->ref()->IsEmpty());
  CHECK(!observer1.IsEmpty());
  CHECK(!holder2->ref()->IsEmpty());
  CHECK(!observer2.IsEmpty());
  holder2 = MakeGarbageCollected<TraceWrapperV8ReferenceHolder>(*holder1);
  CHECK(!holder1->ref()->IsEmpty());
  CHECK(*holder1->ref() == *holder2->ref());
  CHECK(!observer1.IsEmpty());
  CHECK(!observer2.IsEmpty());
  RunV8FullGC();
  CHECK(!holder1->ref()->IsEmpty());
  CHECK(*holder1->ref() == *holder2->ref());
  CHECK(!observer1.IsEmpty());
  // Old object in holder2 already gone.
  CHECK(observer2.IsEmpty());
  holder1.Clear();
  RunV8FullGC();
  CHECK(!holder2->ref()->IsEmpty());
  CHECK(!observer1.IsEmpty());
  holder2.Clear();
  RunV8FullGC();
  CHECK(observer1.IsEmpty());
}

TEST_F(TraceWrapperV8ReferenceTest, MoveOverEmpty) {
  V8TestingScope testing_scope;
  SetIsolate(testing_scope.GetIsolate());

  Persistent<TraceWrapperV8ReferenceHolder> holder1;
  v8::Persistent<v8::Value> observer1;
  CreateObject(GetIsolate(), &holder1, &observer1);
  Persistent<TraceWrapperV8ReferenceHolder> holder2;

  CHECK(!holder1->ref()->IsEmpty());
  CHECK(!holder2.Get());
  CHECK(!observer1.IsEmpty());
  holder2 =
      MakeGarbageCollected<TraceWrapperV8ReferenceHolder>(std::move(*holder1));
  CHECK(holder1->ref()->IsEmpty());
  CHECK(!holder2->ref()->IsEmpty());
  CHECK(!observer1.IsEmpty());
  RunV8FullGC();
  CHECK(holder1->ref()->IsEmpty());
  CHECK(!holder2->ref()->IsEmpty());
  CHECK(!observer1.IsEmpty());
  holder1.Clear();
  holder2.Clear();
  RunV8FullGC();
  CHECK(observer1.IsEmpty());
}

TEST_F(TraceWrapperV8ReferenceTest, MoveOverNonEmpty) {
  V8TestingScope testing_scope;
  SetIsolate(testing_scope.GetIsolate());

  Persistent<TraceWrapperV8ReferenceHolder> holder1;
  v8::Persistent<v8::Value> observer1;
  CreateObject(GetIsolate(), &holder1, &observer1);
  Persistent<TraceWrapperV8ReferenceHolder> holder2;
  v8::Persistent<v8::Value> observer2;
  CreateObject(GetIsolate(), &holder2, &observer2);

  CHECK(!holder1->ref()->IsEmpty());
  CHECK(!observer1.IsEmpty());
  CHECK(!holder2->ref()->IsEmpty());
  CHECK(!observer2.IsEmpty());
  holder2 =
      MakeGarbageCollected<TraceWrapperV8ReferenceHolder>(std::move(*holder1));
  CHECK(holder1->ref()->IsEmpty());
  CHECK(!holder2->ref()->IsEmpty());
  CHECK(!observer1.IsEmpty());
  CHECK(!observer2.IsEmpty());
  RunV8FullGC();
  CHECK(holder1->ref()->IsEmpty());
  CHECK(!holder2->ref()->IsEmpty());
  CHECK(!observer1.IsEmpty());
  CHECK(observer2.IsEmpty());
  holder1.Clear();
  holder2.Clear();
  RunV8FullGC();
  CHECK(observer1.IsEmpty());
}

TEST_F(TraceWrapperV8ReferenceTest, HeapVector) {
  V8TestingScope testing_scope;
  SetIsolate(testing_scope.GetIsolate());

  using VectorContainer = HeapVector<TraceWrapperV8Reference<v8::Value>>;
  Persistent<VectorContainer> holder(MakeGarbageCollected<VectorContainer>());
  v8::Persistent<v8::Value> observer;
  {
    v8::HandleScope handle_scope(GetIsolate());
    v8::Local<v8::Value> value = v8::Object::New(GetIsolate());
    observer.Reset(GetIsolate(), value);
    observer.SetWeak();
    holder->push_back(TraceWrapperV8Reference<v8::Value>(GetIsolate(), value));
  }
  RunV8FullGC();
  CHECK(!observer.IsEmpty());
  holder.Clear();
  RunV8FullGC();
  CHECK(observer.IsEmpty());
}

TEST_F(TraceWrapperV8ReferenceTest, Ephemeron) {
  V8TestingScope testing_scope;
  SetIsolate(testing_scope.GetIsolate());

  using EphemeronMap = HeapHashMap<WeakMember<TraceWrapperV8ReferenceHolder>,
                                   TraceWrapperV8Reference<v8::Value>>;
  Persistent<EphemeronMap> holder(MakeGarbageCollected<EphemeronMap>());
  v8::Persistent<v8::Value> observer;
  Persistent<TraceWrapperV8ReferenceHolder> object(
      MakeGarbageCollected<TraceWrapperV8ReferenceHolder>());
  {
    v8::HandleScope handle_scope(GetIsolate());
    v8::Local<v8::Value> value = v8::Object::New(GetIsolate());
    observer.Reset(GetIsolate(), value);
    observer.SetWeak();
    holder->insert(WeakMember<TraceWrapperV8ReferenceHolder>(object),
                   TraceWrapperV8Reference<v8::Value>(GetIsolate(), value));
  }
  RunV8FullGC();
  EXPECT_TRUE(!observer.IsEmpty());
  holder.Clear();
  RunV8FullGC();
  CHECK(observer.IsEmpty());
}

}  // namespace blink
