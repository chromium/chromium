// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/script_promise_property.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_gc_controller.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/garbage_collected_script_wrappable.h"
#include "third_party/blink/renderer/core/testing/gc_observation.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

class NotReachedFunction : public ScriptFunction::Callable {
 public:
  NotReachedFunction() = default;

  ScriptValue Call(ScriptState*, ScriptValue) override;
};

ScriptValue NotReachedFunction::Call(ScriptState*, ScriptValue) {
  EXPECT_TRUE(false) << "'Unreachable' code was reached";
  return ScriptValue();
}

class StubFunction : public ScriptFunction::Callable {
 public:
  StubFunction(ScriptValue& value, size_t& call_count)
      : value_(value), call_count_(call_count) {}

  ScriptValue Call(ScriptState*, ScriptValue arg) override {
    value_ = arg;
    call_count_++;
    return ScriptValue();
  }

 private:
  ScriptValue& value_;
  size_t& call_count_;
};

class GarbageCollectedHolder final : public GarbageCollectedScriptWrappable {
 public:
  typedef ScriptPromiseProperty<GarbageCollectedScriptWrappable,
                                GarbageCollectedScriptWrappable>
      Property;
  GarbageCollectedHolder(ExecutionContext* execution_context)
      : GarbageCollectedScriptWrappable("holder"),
        property_(MakeGarbageCollected<Property>(execution_context)) {}

  Property* GetProperty() { return property_.Get(); }
  GarbageCollectedScriptWrappable* ToGarbageCollectedScriptWrappable() {
    return this;
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(property_);
    GarbageCollectedScriptWrappable::Trace(visitor);
  }

 private:
  Member<Property> property_;
};

class ScriptPromisePropertyResetter : public ScriptFunction::Callable {
 public:
  using Property = ScriptPromiseProperty<GarbageCollectedScriptWrappable,
                                         GarbageCollectedScriptWrappable>;

  explicit ScriptPromisePropertyResetter(Property* property)
      : property_(property) {}

  void Trace(Visitor* visitor) const override {
    visitor->Trace(property_);
    ScriptFunction::Callable::Trace(visitor);
  }

  ScriptValue Call(ScriptState*, ScriptValue arg) override {
    property_->Reset();
    return ScriptValue();
  }

 private:
  const Member<Property> property_;
};

class ScriptPromisePropertyTestBase {
 public:
  ScriptPromisePropertyTestBase()
      : page_(std::make_unique<DummyPageHolder>(gfx::Size(1, 1))),
        other_world_(DOMWrapperWorld::EnsureIsolatedWorld(GetIsolate(), 1)) {
    v8::HandleScope handle_scope(GetIsolate());
    // Force initialization of v8::Context and ScriptState for the other world.
    page_->GetFrame().GetWindowProxy(OtherWorld());
  }

  virtual ~ScriptPromisePropertyTestBase() { DestroyContext(); }

  LocalDOMWindow* DomWindow() { return page_->GetFrame().DomWindow(); }
  v8::Isolate* GetIsolate() { return DomWindow()->GetIsolate(); }
  ScriptState* MainScriptState() {
    return ToScriptStateForMainWorld(&page_->GetFrame());
  }
  DOMWrapperWorld& MainWorld() { return MainScriptState()->World(); }
  ScriptState* OtherScriptState() {
    return ToScriptState(&page_->GetFrame(), OtherWorld());
  }
  DOMWrapperWorld& OtherWorld() { return *other_world_; }
  ScriptState* CurrentScriptState() {
    return ScriptState::ForCurrentRealm(GetIsolate());
  }

  void PerformMicrotaskCheckpoint() {
    {
      ScriptState::Scope scope(MainScriptState());
      MainScriptState()->GetContext()->GetMicrotaskQueue()->PerformCheckpoint(
          GetIsolate());
    }
    {
      ScriptState::Scope scope(OtherScriptState());
      OtherScriptState()->GetContext()->GetMicrotaskQueue()->PerformCheckpoint(
          GetIsolate());
    }
  }

  void DestroyContext() {
    page_.reset();
    other_world_ = nullptr;
  }

  void Gc() { ThreadState::Current()->CollectAllGarbageForTesting(); }

  ScriptFunction* NotReached(ScriptState* script_state) {
    return MakeGarbageCollected<ScriptFunction>(
        script_state, MakeGarbageCollected<NotReachedFunction>());
  }
  ScriptFunction* Stub(ScriptState* script_state,
                       ScriptValue& value,
                       size_t& call_count) {
    return MakeGarbageCollected<ScriptFunction>(
        script_state, MakeGarbageCollected<StubFunction>(value, call_count));
  }

  ScriptValue Wrap(DOMWrapperWorld& world,
                   GarbageCollectedScriptWrappable* value) {
    v8::Isolate* isolate = GetIsolate();
    v8::HandleScope handle_scope(isolate);
    ScriptState* script_state =
        ScriptState::From(isolate, ToV8Context(DomWindow(), world));
    ScriptState::Scope scope(script_state);
    return ScriptValue(
        isolate,
        ToV8Traits<GarbageCollectedScriptWrappable>::ToV8(script_state, value));
  }

 private:
  test::TaskEnvironment task_environment_;
  std::unique_ptr<DummyPageHolder> page_;
  Persistent<DOMWrapperWorld> other_world_;
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
      : holder_(MakeGarbageCollected<GarbageCollectedHolder>(DomWindow())) {}

  void ClearHolder() { holder_.Clear(); }
  GarbageCollectedHolder* Holder() { return holder_; }
  Property* GetProperty() { return holder_->GetProperty(); }
  ScriptPromise<GarbageCollectedScriptWrappable> Promise(
      DOMWrapperWorld& world) {
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
  void Test(const T::ImplType& value,
            const char* expected,
            const char* file,
            int line) {
    typedef ScriptPromiseProperty<T, IDLUndefined> Property;
    Property* property = MakeGarbageCollected<Property>(DomWindow());
    size_t n_resolve_calls = 0;
    ScriptValue actual_value;
    String actual;
    {
      ScriptState::Scope scope(MainScriptState());
      property->Promise(DOMWrapperWorld::MainWorld(GetIsolate()))
          .Then(Stub(CurrentScriptState(), actual_value, n_resolve_calls),
                NotReached(CurrentScriptState()));
    }
    property->Resolve(value);
    PerformMicrotaskCheckpoint();
    {
      ScriptState::Scope scope(MainScriptState());
      actual = ToCoreString(MainScriptState()->GetIsolate(),
                            actual_value.V8Value()
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
  auto v = GetProperty()->Promise(DOMWrapperWorld::MainWorld(GetIsolate()));
  auto w = GetProperty()->Promise(DOMWrapperWorld::MainWorld(GetIsolate()));
  EXPECT_EQ(v, w);
  ASSERT_FALSE(v.IsEmpty());
  {
    ScriptState::Scope scope(MainScriptState());
    EXPECT_EQ(v.V8Value().As<v8::Object>()->GetCreationContextChecked(),
              ToV8Context(DomWindow(), MainWorld()));
  }
  EXPECT_EQ(Property::kPending, GetProperty()->GetState());
}

TEST_F(ScriptPromisePropertyGarbageCollectedTest,
       Promise_IsStableObjectInVariousWorlds) {
  auto u = GetProperty()->Promise(OtherWorld());
  auto v = GetProperty()->Promise(DOMWrapperWorld::MainWorld(GetIsolate()));
  auto w = GetProperty()->Promise(DOMWrapperWorld::MainWorld(GetIsolate()));
  EXPECT_NE(MainScriptState(), OtherScriptState());
  EXPECT_NE(&MainWorld(), &OtherWorld());
  EXPECT_NE(u, v);
  EXPECT_EQ(v, w);
  ASSERT_FALSE(u.IsEmpty());
  ASSERT_FALSE(v.IsEmpty());
  {
    ScriptState::Scope scope(OtherScriptState());
    EXPECT_EQ(u.V8Value().As<v8::Object>()->GetCreationContextChecked(),
              ToV8Context(DomWindow(), OtherWorld()));
  }
  {
    ScriptState::Scope scope(MainScriptState());
    EXPECT_EQ(v.V8Value().As<v8::Object>()->GetCreationContextChecked(),
              ToV8Context(DomWindow(), MainWorld()));
  }
  EXPECT_EQ(Property::kPending, GetProperty()->GetState());
}

TEST_F(ScriptPromisePropertyGarbageCollectedTest,
       Promise_IsStableObjectAfterSettling) {
  auto v = Promise(DOMWrapperWorld::MainWorld(GetIsolate()));
  GarbageCollectedScriptWrappable* value =
      MakeGarbageCollected<GarbageCollectedScriptWrappable>("value");

  GetProperty()->Resolve(value);
  EXPECT_EQ(Property::kResolved, GetProperty()->GetState());

  auto w = Promise(DOMWrapperWorld::MainWorld(GetIsolate()));
  EXPECT_EQ(v, w);
  EXPECT_FALSE(v.IsEmpty());
}

TEST_F(ScriptPromisePropertyGarbageCollectedTest,
       Promise_DoesNotImpedeGarbageCollection) {
  Persistent<GCObservation> observation;
  {
    ScriptState::Scope scope(MainScriptState());
    // Here we have a reference cylce between Holder() and the promise.
    Holder()->GetProperty()->Resolve(Holder());

    observation = MakeGarbageCollected<GCObservation>(
        GetIsolate(),
        Promise(DOMWrapperWorld::MainWorld(GetIsolate())).V8Value());
  }

  Gc();
  EXPECT_FALSE(observation->wasCollected());

  ClearHolder();

  Gc();
  EXPECT_TRUE(observation->wasCollected());
}

TEST_F(ScriptPromisePropertyGarbageCollectedTest,
       Resolve_ResolvesScriptPromise) {
  auto promise =
      GetProperty()->Promise(DOMWrapperWorld::MainWorld(GetIsolate()));
  auto other_promise = GetProperty()->Promise(OtherWorld());
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

  PerformMicrotaskCheckpoint();
  EXPECT_EQ(1u, n_resolve_calls);
  EXPECT_EQ(1u, n_other_resolve_calls);
  EXPECT_EQ(Wrap(MainWorld(), value), actual);
  EXPECT_NE(actual, other_actual);
  EXPECT_EQ(Wrap(OtherWorld(), value), other_actual);
}

TEST_F(ScriptPromisePropertyGarbageCollectedTest,
       ResolveAndGetPromiseOnOtherWorld) {
  auto promise =
      GetProperty()->Promise(DOMWrapperWorld::MainWorld(GetIsolate()));
  auto other_promise = GetProperty()->Promise(OtherWorld());
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

  PerformMicrotaskCheckpoint();
  EXPECT_EQ(1u, n_resolve_calls);
  EXPECT_EQ(0u, n_other_resolve_calls);

  {
    ScriptState::Scope scope(OtherScriptState());
    other_promise.Then(
        Stub(CurrentScriptState(), other_actual, n_other_resolve_calls),
        NotReached(CurrentScriptState()));
  }

  PerformMicrotaskCheckpoint();
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
        ->Promise(DOMWrapperWorld::MainWorld(GetIsolate()))
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

  PerformMicrotaskCheckpoint();
  EXPECT_EQ(1u, n_reject_calls);
  EXPECT_EQ(Wrap(MainWorld(), reason), actual);
  EXPECT_EQ(1u, n_other_reject_calls);
  EXPECT_NE(actual, other_actual);
  EXPECT_EQ(Wrap(OtherWorld(), reason), other_actual);
}

TEST_F(ScriptPromisePropertyGarbageCollectedTest, Promise_DeadContext) {
  v8::Isolate* isolate = GetIsolate();
  GetProperty()->Resolve(
      MakeGarbageCollected<GarbageCollectedScriptWrappable>("value"));
  EXPECT_EQ(Property::kResolved, GetProperty()->GetState());

  DestroyContext();

  EXPECT_TRUE(
      GetProperty()->Promise(DOMWrapperWorld::MainWorld(isolate)).IsEmpty());
}

TEST_F(ScriptPromisePropertyGarbageCollectedTest, Resolve_DeadContext) {
  {
    ScriptState::Scope scope(MainScriptState());
    GetProperty()
        ->Promise(DOMWrapperWorld::MainWorld(GetIsolate()))
        .Then(NotReached(CurrentScriptState()),
              NotReached(CurrentScriptState()));
  }

  DestroyContext();
  EXPECT_TRUE(!GetProperty()->GetExecutionContext() ||
              GetProperty()->GetExecutionContext()->IsContextDestroyed());

  GetProperty()->Resolve(
      MakeGarbageCollected<GarbageCollectedScriptWrappable>("value"));
  EXPECT_EQ(Property::kPending, GetProperty()->GetState());
}

TEST_F(ScriptPromisePropertyGarbageCollectedTest, Reset) {
  ScriptState::Scope scope(MainScriptState());

  ScriptPromise<GarbageCollectedScriptWrappable> old_promise, new_promise;
  ScriptValue old_actual, new_actual;
  GarbageCollectedScriptWrappable* old_value =
      MakeGarbageCollected<GarbageCollectedScriptWrappable>("old");
  GarbageCollectedScriptWrappable* new_value =
      MakeGarbageCollected<GarbageCollectedScriptWrappable>("new");
  size_t n_old_resolve_calls = 0;
  size_t n_new_reject_calls = 0;

  {
    ScriptState::Scope scope2(MainScriptState());
    GetProperty()->Resolve(old_value);
    old_promise = GetProperty()->Promise(MainWorld());
    old_promise.Then(
        Stub(CurrentScriptState(), old_actual, n_old_resolve_calls),
        NotReached(CurrentScriptState()));
  }

  GetProperty()->Reset();

  {
    ScriptState::Scope scope2(MainScriptState());
    new_promise = GetProperty()->Promise(MainWorld());
    new_promise.Then(
        NotReached(CurrentScriptState()),
        Stub(CurrentScriptState(), new_actual, n_new_reject_calls));
    GetProperty()->Reject(new_value);
  }

  EXPECT_EQ(0u, n_old_resolve_calls);
  EXPECT_EQ(0u, n_new_reject_calls);

  PerformMicrotaskCheckpoint();
  EXPECT_EQ(1u, n_old_resolve_calls);
  EXPECT_EQ(1u, n_new_reject_calls);
  EXPECT_NE(old_promise, new_promise);
  EXPECT_EQ(Wrap(MainWorld(), old_value), old_actual);
  EXPECT_EQ(Wrap(MainWorld(), new_value), new_actual);
  EXPECT_NE(old_actual, new_actual);
}

TEST_F(ScriptPromisePropertyGarbageCollectedTest, MarkAsHandled) {
  {
    // Unhandled promise.
    ScriptState::Scope scope(MainScriptState());
    auto promise =
        GetProperty()->Promise(DOMWrapperWorld::MainWorld(GetIsolate()));
    GarbageCollectedScriptWrappable* reason =
        MakeGarbageCollected<GarbageCollectedScriptWrappable>("reason");
    GetProperty()->Reject(reason);
    EXPECT_FALSE(promise.V8Promise()->HasHandler());
  }

  GetProperty()->Reset();

  {
    // MarkAsHandled applies to newly created promises.
    ScriptState::Scope scope(MainScriptState());
    GetProperty()->MarkAsHandled();
    auto promise =
        GetProperty()->Promise(DOMWrapperWorld::MainWorld(GetIsolate()));
    GarbageCollectedScriptWrappable* reason =
        MakeGarbageCollected<GarbageCollectedScriptWrappable>("reason");
    GetProperty()->Reject(reason);
    EXPECT_TRUE(promise.V8Promise()->HasHandler());
  }

  GetProperty()->Reset();

  {
    // MarkAsHandled applies to previously vended promises.
    ScriptState::Scope scope(MainScriptState());
    auto promise =
        GetProperty()->Promise(DOMWrapperWorld::MainWorld(GetIsolate()));
    GetProperty()->MarkAsHandled();
    GarbageCollectedScriptWrappable* reason =
        MakeGarbageCollected<GarbageCollectedScriptWrappable>("reason");
    GetProperty()->Reject(reason);
    EXPECT_TRUE(promise.V8Promise()->HasHandler());
  }
}

TEST_F(ScriptPromisePropertyGarbageCollectedTest, SyncResolve) {
  // Call getters to create resolvers in the property.
  GetProperty()->Promise(DOMWrapperWorld::MainWorld(GetIsolate()));
  GetProperty()->Promise(OtherWorld());

  auto* resolution =
      MakeGarbageCollected<GarbageCollectedScriptWrappable>("hi");
  v8::HandleScope handle_scope(GetIsolate());
  v8::Local<v8::Object> main_v8_resolution;
  v8::Local<v8::Object> other_v8_resolution;
  {
    ScriptState::Scope scope(MainScriptState());
    v8::MicrotasksScope microtasks_scope(
        GetIsolate(), ToMicrotaskQueue(MainScriptState()),
        v8::MicrotasksScope::kDoNotRunMicrotasks);
    main_v8_resolution = ToV8Traits<GarbageCollectedScriptWrappable>::ToV8(
                             MainScriptState(), resolution)
                             .As<v8::Object>();
    v8::PropertyDescriptor descriptor(
        MakeGarbageCollected<ScriptFunction>(
            MainScriptState(),
            MakeGarbageCollected<ScriptPromisePropertyResetter>(GetProperty()))
            ->V8Function(),
        v8::Undefined(GetIsolate()));
    ASSERT_EQ(
        v8::Just(true),
        main_v8_resolution->DefineProperty(
            MainScriptState()->GetContext(),
            v8::String::NewFromUtf8Literal(GetIsolate(), "then"), descriptor));
  }
  {
    ScriptState::Scope scope(OtherScriptState());
    v8::MicrotasksScope microtasks_scope(
        GetIsolate(), ToMicrotaskQueue(OtherScriptState()),
        v8::MicrotasksScope::kDoNotRunMicrotasks);
    other_v8_resolution = ToV8Traits<GarbageCollectedScriptWrappable>::ToV8(
                              OtherScriptState(), resolution)
                              .As<v8::Object>();
    v8::PropertyDescriptor descriptor(
        MakeGarbageCollected<ScriptFunction>(
            OtherScriptState(),
            MakeGarbageCollected<ScriptPromisePropertyResetter>(GetProperty()))
            ->V8Function(),
        v8::Undefined(GetIsolate()));
    ASSERT_EQ(
        v8::Just(true),
        other_v8_resolution->DefineProperty(
            OtherScriptState()->GetContext(),
            v8::String::NewFromUtf8Literal(GetIsolate(), "then"), descriptor));
  }

  // This shouldn't crash.
  GetProperty()->Resolve(resolution);
  EXPECT_EQ(GetProperty()->GetState(), Property::State::kPending);
}

TEST_F(ScriptPromisePropertyNonScriptWrappableResolutionTargetTest,
       ResolveWithUndefined) {
  Test<IDLUndefined>(ToV8UndefinedGenerator(), "undefined", __FILE__, __LINE__);
}

TEST_F(ScriptPromisePropertyNonScriptWrappableResolutionTargetTest,
       ResolveWithString) {
  Test<IDLString>(String("hello"), "hello", __FILE__, __LINE__);
}

TEST_F(ScriptPromisePropertyNonScriptWrappableResolutionTargetTest,
       ResolveWithInteger) {
  Test<IDLLong>(-1, "-1", __FILE__, __LINE__);
}

}  // namespace blink
