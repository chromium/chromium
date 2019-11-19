// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/script_promise_property.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_gc_controller.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/garbage_collected_script_wrappable.h"
#include "third_party/blink/renderer/core/testing/gc_observation.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

class NotReached : public ScriptFunction {
 public:
  static v8::Local<v8::Function> CreateFunction(ScriptState* script_state) {
    NotReached* self = MakeGarbageCollected<NotReached>(script_state);
    return self->BindToV8Function();
  }

  explicit NotReached(ScriptState* script_state)
      : ScriptFunction(script_state) {}

 private:
  ScriptValue Call(ScriptValue) override;
};

ScriptValue NotReached::Call(ScriptValue) {
  EXPECT_TRUE(false) << "'Unreachable' code was reached";
  return ScriptValue();
}

class StubFunction : public ScriptFunction {
 public:
  static v8::Local<v8::Function> CreateFunction(ScriptState* script_state,
                                                ScriptValue& value,
                                                size_t& call_count) {
    StubFunction* self =
        MakeGarbageCollected<StubFunction>(script_state, value, call_count);
    return self->BindToV8Function();
  }

  StubFunction(ScriptState* script_state,
               ScriptValue& value,
               size_t& call_count)
      : ScriptFunction(script_state), value_(value), call_count_(call_count) {}

 private:
  ScriptValue Call(ScriptValue arg) override {
    value_ = arg;
    call_count_++;
    return ScriptValue();
  }

  ScriptValue& value_;
  size_t& call_count_;
};

class GarbageCollectedHolder final : public GarbageCollectedScriptWrappable {
 public:
  typedef ScriptPromiseProperty<Member<GarbageCollectedScriptWrappable>,
                                Member<GarbageCollectedScriptWrappable>,
                                Member<GarbageCollectedScriptWrappable>>
      Property;
  GarbageCollectedHolder(ExecutionContext* execution_context)
      : GarbageCollectedScriptWrappable("holder"),
        property_(
            MakeGarbageCollected<Property>(execution_context,
                                           ToGarbageCollectedScriptWrappable(),
                                           Property::kReady)) {}

  Property* GetProperty() { return property_; }
  GarbageCollectedScriptWrappable* ToGarbageCollectedScriptWrappable() {
    return this;
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(property_);
    GarbageCollectedScriptWrappable::Trace(visitor);
  }

 private:
  Member<Property> property_;
};

class ScriptPromisePropertyTestBase {
 public:
  ScriptPromisePropertyTestBase()
      : page_(std::make_unique<DummyPageHolder>(IntSize(1, 1))) {
    v8::HandleScope handle_scope(GetIsolate());
    other_script_state_ = MakeGarbageCollected<ScriptState>(
        v8::Context::New(GetIsolate()),
        DOMWrapperWorld::EnsureIsolatedWorld(GetIsolate(), 1));
  }

  virtual ~ScriptPromisePropertyTestBase() { DestroyContext(); }

  Document& GetDocument() { return page_->GetDocument(); }
  v8::Isolate* GetIsolate() { return GetDocument().GetIsolate(); }
  ScriptState* MainScriptState() {
    return ToScriptStateForMainWorld(GetDocument().GetFrame());
  }
  DOMWrapperWorld& MainWorld() { return MainScriptState()->World(); }
  ScriptState* OtherScriptState() { return other_script_state_; }
  DOMWrapperWorld& OtherWorld() { return other_script_state_->World(); }
  ScriptState* CurrentScriptState() {
    return ScriptState::Current(GetIsolate());
  }

  void DestroyContext() {
    page_.reset();
    if (other_script_state_) {
      other_script_state_->DisposePerContextData();
      other_script_state_ = nullptr;
    }
  }

  void Gc() {
    V8GCController::CollectAllGarbageForTesting(
        v8::Isolate::GetCurrent(),
        v8::EmbedderHeapTracer::EmbedderStackState::kEmpty);
  }

  v8::Local<v8::Function> NotReached(ScriptState* script_state) {
    return NotReached::CreateFunction(script_state);
  }
  v8::Local<v8::Function> Stub(ScriptState* script_state,
                               ScriptValue& value,
                               size_t& call_count) {
    return StubFunction::CreateFunction(script_state, value, call_count);
  }

  template <typename T>
  ScriptValue Wrap(DOMWrapperWorld& world, const T& value) {
    v8::HandleScope handle_scope(GetIsolate());
    ScriptState* script_state =
        ScriptState::From(ToV8Context(&GetDocument(), world));
    ScriptState::Scope scope(script_state);
    return ScriptValue(
        GetIsolate(),
        ToV8(value, script_state->GetContext()->Global(), GetIsolate()));
  }

 private:
  std::unique_ptr<DummyPageHolder> page_;
  Persistent<ScriptState> other_script_state_;
};

// This is the main test class.
// If you want to examine a testcase independent of holder types, place the
// test on this class.
class ScriptPromisePropertyGarbageCollectedTest
    : public ScriptPromisePropertyTestBase,
      public testing::Test {
 public:
  typedef GarbageCollectedHolder::Property Property;

  ScriptPromisePropertyGarbageCollectedTest()
      : holder_(MakeGarbageCollected<GarbageCollectedHolder>(&GetDocument())) {}

  void ClearHolder() { holder_.Clear(); }
  GarbageCollectedHolder* Holder() { return holder_; }
  Property* GetProperty() { return holder_->GetProperty(); }
  ScriptPromise Promise(DOMWrapperWorld& world) {
    return GetProperty()->Promise(world);
  }

 private:
  Persistent<GarbageCollectedHolder> holder_;
};

// Tests that ScriptPromiseProperty works with a non ScriptWrappable resolution
// target.
class ScriptPromisePropertyNonScriptWrappableResolutionTargetTest
    : public ScriptPromisePropertyTestBase,
      public testing::Test {
 public:
  template <typename T>
  void Test(const T& value, const char* expected, const char* file, int line) {
    typedef ScriptPromiseProperty<Member<GarbageCollectedScriptWrappable>, T,
                                  ToV8UndefinedGenerator>
        Property;
    Property* property = MakeGarbageCollected<Property>(
        &GetDocument(),
        MakeGarbageCollected<GarbageCollectedScriptWrappable>("holder"),
        Property::kReady);
    size_t n_resolve_calls = 0;
    ScriptValue actual_value;
    String actual;
    {
      ScriptState::Scope scope(MainScriptState());
      property->Promise(DOMWrapperWorld::MainWorld())
          .Then(Stub(CurrentScriptState(), actual_value, n_resolve_calls),
                NotReached(CurrentScriptState()));
    }
    property->Resolve(value);
    v8::MicrotasksScope::PerformCheckpoint(GetIsolate());
    {
      ScriptState::Scope scope(MainScriptState());
      actual = ToCoreString(actual_value.V8Value()
                                ->ToString(MainScriptState()->GetContext())
                                .ToLocalChecked());
    }
    if (expected != actual) {
      ADD_FAILURE_AT(file, line)
          << "toV8 returns an incorrect value.\n  Actual: " << actual.Utf8()
          << "\nExpected: " << expected;
      return;
    }
  }
};

}  // namespace

TEST_F(ScriptPromisePropertyGarbageCollectedTest,
       Promise_IsStableObjectInMainWorld) {
  ScriptPromise v = GetProperty()->Promise(DOMWrapperWorld::MainWorld());
  ScriptPromise w = GetProperty()->Promise(DOMWrapperWorld::MainWorld());
  EXPECT_EQ(v, w);
  ASSERT_FALSE(v.IsEmpty());
  {
    ScriptState::Scope scope(MainScriptState());
    EXPECT_EQ(v.V8Value().As<v8::Object>()->CreationContext(),
              ToV8Context(&GetDocument(), MainWorld()));
  }
  EXPECT_EQ(Property::kPending, GetProperty()->GetState());
}

TEST_F(ScriptPromisePropertyGarbageCollectedTest,
       Promise_IsStableObjectInVariousWorlds) {
  ScriptPromise u = GetProperty()->Promise(OtherWorld());
  ScriptPromise v = GetProperty()->Promise(DOMWrapperWorld::MainWorld());
  ScriptPromise w = GetProperty()->Promise(DOMWrapperWorld::MainWorld());
  EXPECT_NE(MainScriptState(), OtherScriptState());
  EXPECT_NE(&MainWorld(), &OtherWorld());
  EXPECT_NE(u, v);
  EXPECT_EQ(v, w);
  ASSERT_FALSE(u.IsEmpty());
  ASSERT_FALSE(v.IsEmpty());
  {
    ScriptState::Scope scope(OtherScriptState());
    EXPECT_EQ(u.V8Value().As<v8::Object>()->CreationContext(),
              ToV8Context(&GetDocument(), OtherWorld()));
  }
  {
    ScriptState::Scope scope(MainScriptState());
    EXPECT_EQ(v.V8Value().As<v8::Object>()->CreationContext(),
              ToV8Context(&GetDocument(), MainWorld()));
  }
  EXPECT_EQ(Property::kPending, GetProperty()->GetState());
}

TEST_F(ScriptPromisePropertyGarbageCollectedTest,
       Promise_IsStableObjectAfterSettling) {
  ScriptPromise v = Promise(DOMWrapperWorld::MainWorld());
  GarbageCollectedScriptWrappable* value =
      MakeGarbageCollected<GarbageCollectedScriptWrappable>("value");

  GetProperty()->Resolve(value);
  EXPECT_EQ(Property::kResolved, GetProperty()->GetState());

  ScriptPromise w = Promise(DOMWrapperWorld::MainWorld());
  EXPECT_EQ(v, w);
  EXPECT_FALSE(v.IsEmpty());
}

TEST_F(ScriptPromisePropertyGarbageCollectedTest,
       Promise_DoesNotImpedeGarbageCollection) {
  ScriptValue holder_wrapper =
      Wrap(MainWorld(), Holder()->ToGarbageCollectedScriptWrappable());

  Persistent<GCObservation> observation;
  {
    ScriptState::Scope scope(MainScriptState());
    observation = MakeGarbageCollected<GCObservation>(
        Promise(DOMWrapperWorld::MainWorld()).V8Value());
  }

  Gc();
  EXPECT_FALSE(observation->wasCollected());

  holder_wrapper.Clear();
  ClearHolder();
  Gc();
  EXPECT_TRUE(observation->wasCollected());
}

TEST_F(ScriptPromisePropertyGarbageCollectedTest,
       Resolve_ResolvesScriptPromise) {
  ScriptPromise promise = GetProperty()->Promise(DOMWrapperWorld::MainWorld());
  ScriptPromise other_promise = GetProperty()->Promise(OtherWorld());
  ScriptValue actual, other_actual;
  size_t n_resolve_calls = 0;
  size_t n_other_resolve_calls = 0;

  {
    ScriptState::Scope scope(MainScriptState());
    promise.Then(Stub(CurrentScriptState(), actual, n_resolve_calls),
                 NotReached(CurrentScriptState()));
  }

  {
    ScriptState::Scope scope(OtherScriptState());
    other_promise.Then(
        Stub(CurrentScriptState(), other_actual, n_other_resolve_calls),
        NotReached(CurrentScriptState()));
  }

  EXPECT_NE(promise, other_promise);

  GarbageCollectedScriptWrappable* value =
      MakeGarbageCollected<GarbageCollectedScriptWrappable>("value");
  GetProperty()->Resolve(value);
  EXPECT_EQ(Property::kResolved, GetProperty()->GetState());

  v8::MicrotasksScope::PerformCheckpoint(GetIsolate());
  EXPECT_EQ(1u, n_resolve_calls);
  EXPECT_EQ(1u, n_other_resolve_calls);
  EXPECT_EQ(Wrap(MainWorld(), value), actual);
  EXPECT_NE(actual, other_actual);
  EXPECT_EQ(Wrap(OtherWorld(), value), other_actual);
}

TEST_F(ScriptPromisePropertyGarbageCollectedTest,
       ResolveAndGetPromiseOnOtherWorld) {
  ScriptPromise promise = GetProperty()->Promise(DOMWrapperWorld::MainWorld());
  ScriptPromise other_promise = GetProperty()->Promise(OtherWorld());
  ScriptValue actual, other_actual;
  size_t n_resolve_calls = 0;
  size_t n_other_resolve_calls = 0;

  {
    ScriptState::Scope scope(MainScriptState());
    promise.Then(Stub(CurrentScriptState(), actual, n_resolve_calls),
                 NotReached(CurrentScriptState()));
  }

  EXPECT_NE(promise, other_promise);
  GarbageCollectedScriptWrappable* value =
      MakeGarbageCollected<GarbageCollectedScriptWrappable>("value");
  GetProperty()->Resolve(value);
  EXPECT_EQ(Property::kResolved, GetProperty()->GetState());

  v8::MicrotasksScope::PerformCheckpoint(GetIsolate());
  EXPECT_EQ(1u, n_resolve_calls);
  EXPECT_EQ(0u, n_other_resolve_calls);

  {
    ScriptState::Scope scope(OtherScriptState());
    other_promise.Then(
        Stub(CurrentScriptState(), other_actual, n_other_resolve_calls),
        NotReached(CurrentScriptState()));
  }

  v8::MicrotasksScope::PerformCheckpoint(GetIsolate());
  EXPECT_EQ(1u, n_resolve_calls);
  EXPECT_EQ(1u, n_other_resolve_calls);
  EXPECT_EQ(Wrap(MainWorld(), value), actual);
  EXPECT_NE(actual, other_actual);
  EXPECT_EQ(Wrap(OtherWorld(), value), other_actual);
}

TEST_F(ScriptPromisePropertyGarbageCollectedTest, Reject_RejectsScriptPromise) {
  GarbageCollectedScriptWrappable* reason =
      MakeGarbageCollected<GarbageCollectedScriptWrappable>("reason");
  GetProperty()->Reject(reason);
  EXPECT_EQ(Property::kRejected, GetProperty()->GetState());

  ScriptValue actual, other_actual;
  size_t n_reject_calls = 0;
  size_t n_other_reject_calls = 0;
  {
    ScriptState::Scope scope(MainScriptState());
    GetProperty()
        ->Promise(DOMWrapperWorld::MainWorld())
        .Then(NotReached(CurrentScriptState()),
              Stub(CurrentScriptState(), actual, n_reject_calls));
  }

  {
    ScriptState::Scope scope(OtherScriptState());
    GetProperty()
        ->Promise(OtherWorld())
        .Then(NotReached(CurrentScriptState()),
              Stub(CurrentScriptState(), other_actual, n_other_reject_calls));
  }

  v8::MicrotasksScope::PerformCheckpoint(GetIsolate());
  EXPECT_EQ(1u, n_reject_calls);
  EXPECT_EQ(Wrap(MainWorld(), reason), actual);
  EXPECT_EQ(1u, n_other_reject_calls);
  EXPECT_NE(actual, other_actual);
  EXPECT_EQ(Wrap(OtherWorld(), reason), other_actual);
}

TEST_F(ScriptPromisePropertyGarbageCollectedTest, Promise_DeadContext) {
  GetProperty()->Resolve(
      MakeGarbageCollected<GarbageCollectedScriptWrappable>("value"));
  EXPECT_EQ(Property::kResolved, GetProperty()->GetState());

  DestroyContext();

  EXPECT_TRUE(GetProperty()->Promise(DOMWrapperWorld::MainWorld()).IsEmpty());
}

TEST_F(ScriptPromisePropertyGarbageCollectedTest, Resolve_DeadContext) {
  {
    ScriptState::Scope scope(MainScriptState());
    GetProperty()
        ->Promise(DOMWrapperWorld::MainWorld())
        .Then(NotReached(CurrentScriptState()),
              NotReached(CurrentScriptState()));
  }

  DestroyContext();
  EXPECT_TRUE(!GetProperty()->GetExecutionContext() ||
              GetProperty()->GetExecutionContext()->IsContextDestroyed());

  GetProperty()->Resolve(
      MakeGarbageCollected<GarbageCollectedScriptWrappable>("value"));
  EXPECT_EQ(Property::kPending, GetProperty()->GetState());

  v8::MicrotasksScope::PerformCheckpoint(v8::Isolate::GetCurrent());
}

TEST_F(ScriptPromisePropertyGarbageCollectedTest, Reset) {
  ScriptState::Scope scope(MainScriptState());

  ScriptPromise old_promise, new_promise;
  ScriptValue old_actual, new_actual;
  GarbageCollectedScriptWrappable* old_value =
      MakeGarbageCollected<GarbageCollectedScriptWrappable>("old");
  GarbageCollectedScriptWrappable* new_value =
      MakeGarbageCollected<GarbageCollectedScriptWrappable>("new");
  size_t n_old_resolve_calls = 0;
  size_t n_new_reject_calls = 0;

  {
    ScriptState::Scope scope(MainScriptState());
    GetProperty()->Resolve(old_value);
    old_promise = GetProperty()->Promise(MainWorld());
    old_promise.Then(
        Stub(CurrentScriptState(), old_actual, n_old_resolve_calls),
        NotReached(CurrentScriptState()));
  }

  GetProperty()->Reset();

  {
    ScriptState::Scope scope(MainScriptState());
    new_promise = GetProperty()->Promise(MainWorld());
    new_promise.Then(
        NotReached(CurrentScriptState()),
        Stub(CurrentScriptState(), new_actual, n_new_reject_calls));
    GetProperty()->Reject(new_value);
  }

  EXPECT_EQ(0u, n_old_resolve_calls);
  EXPECT_EQ(0u, n_new_reject_calls);

  v8::MicrotasksScope::PerformCheckpoint(GetIsolate());
  EXPECT_EQ(1u, n_old_resolve_calls);
  EXPECT_EQ(1u, n_new_reject_calls);
  EXPECT_NE(old_promise, new_promise);
  EXPECT_EQ(Wrap(MainWorld(), old_value), old_actual);
  EXPECT_EQ(Wrap(MainWorld(), new_value), new_actual);
  EXPECT_NE(old_actual, new_actual);
}

TEST_F(ScriptPromisePropertyNonScriptWrappableResolutionTargetTest,
       ResolveWithUndefined) {
  Test(ToV8UndefinedGenerator(), "undefined", __FILE__, __LINE__);
}

TEST_F(ScriptPromisePropertyNonScriptWrappableResolutionTargetTest,
       ResolveWithString) {
  Test(String("hello"), "hello", __FILE__, __LINE__);
}

TEST_F(ScriptPromisePropertyNonScriptWrappableResolutionTargetTest,
       ResolveWithInteger) {
  Test(-1, "-1", __FILE__, __LINE__);
}

}  // namespace blink
