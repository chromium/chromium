// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/dynamic_module_resolver.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/referrer_script_info.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_fetch_request.h"
#include "third_party/blink/renderer/core/script/js_module_script.h"
#include "third_party/blink/renderer/core/testing/dummy_modulator.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

constexpr const char* kTestReferrerURL = "https://example.com/referrer.js";
constexpr const char* kTestDependencyURL = "https://example.com/dependency.js";

const KURL TestReferrerURL() {
  return KURL(kTestReferrerURL);
}
const KURL TestDependencyURL() {
  return KURL(kTestDependencyURL);
}

class DynamicModuleResolverTestModulator final : public DummyModulator {
 public:
  explicit DynamicModuleResolverTestModulator(ScriptState* script_state)
      : script_state_(script_state) {}
  ~DynamicModuleResolverTestModulator() override = default;

  void ResolveTreeFetch(ModuleScript* module_script) {
    ASSERT_TRUE(pending_client_);
    pending_client_->NotifyModuleTreeLoadFinished(module_script);
    pending_client_ = nullptr;
  }
  void SetExpectedFetchTreeURL(const KURL& url) {
    expected_fetch_tree_url_ = url;
  }
  bool fetch_tree_was_called() const { return fetch_tree_was_called_; }

  void Trace(Visitor*) override;

 private:
  // Implements Modulator:
  ScriptState* GetScriptState() final { return script_state_; }

  ModuleScript* GetFetchedModuleScript(const KURL& url) final {
    EXPECT_EQ(TestReferrerURL(), url);
    ModuleScript* module_script =
        JSModuleScript::CreateForTest(this, v8::Local<v8::Module>(), url);
    return module_script;
  }

  KURL ResolveModuleSpecifier(const String& module_request,
                              const KURL& base_url,
                              String* failure_reason) final {
    if (module_request == "invalid-specifier")
      return KURL();

    return KURL(base_url, module_request);
  }

  void ClearIsAcquiringImportMaps() final {}

  void FetchTree(const KURL& url,
                 ResourceFetcher*,
                 mojom::RequestContextType,
                 const ScriptFetchOptions&,
                 ModuleScriptCustomFetchType custom_fetch_type,
                 ModuleTreeClient* client) final {
    EXPECT_EQ(expected_fetch_tree_url_, url);

    // Currently there are no usage of custom fetch hooks for dynamic import in
    // web specifications.
    EXPECT_EQ(ModuleScriptCustomFetchType::kNone, custom_fetch_type);

    pending_client_ = client;
    fetch_tree_was_called_ = true;
  }

  ScriptValue ExecuteModule(ModuleScript* module_script,
                            CaptureEvalErrorFlag capture_error) final {
    EXPECT_EQ(CaptureEvalErrorFlag::kCapture, capture_error);

    ScriptState::Scope scope(script_state_);
    return ModuleRecord::Evaluate(script_state_, module_script->V8Module(),
                                  module_script->SourceURL());
  }

  Member<ScriptState> script_state_;
  Member<ModuleTreeClient> pending_client_;
  KURL expected_fetch_tree_url_;
  bool fetch_tree_was_called_ = false;
};

void DynamicModuleResolverTestModulator::Trace(Visitor* visitor) {
  visitor->Trace(script_state_);
  visitor->Trace(pending_client_);
  DummyModulator::Trace(visitor);
}

// CaptureExportedStringFunction implements a javascript function
// with a single argument of type module namespace.
// CaptureExportedStringFunction captures the exported string value
// from the module namespace as a WTF::String, exposed via CapturedValue().
class CaptureExportedStringFunction final : public ScriptFunction {
 public:
  CaptureExportedStringFunction(ScriptState* script_state,
                                const String& export_name)
      : ScriptFunction(script_state), export_name_(export_name) {}

  v8::Local<v8::Function> Bind() { return BindToV8Function(); }
  bool WasCalled() const { return was_called_; }
  const String& CapturedValue() const { return captured_value_; }

 private:
  ScriptValue Call(ScriptValue value) override {
    was_called_ = true;

    v8::Isolate* isolate = GetScriptState()->GetIsolate();
    v8::Local<v8::Context> context = GetScriptState()->GetContext();

    v8::Local<v8::Object> module_namespace =
        value.V8Value()->ToObject(context).ToLocalChecked();
    v8::Local<v8::Value> exported_value =
        module_namespace->Get(context, V8String(isolate, export_name_))
            .ToLocalChecked();
    captured_value_ =
        ToCoreString(exported_value->ToString(context).ToLocalChecked());

    return ScriptValue();
  }

  const String export_name_;
  bool was_called_ = false;
  String captured_value_;
};

// CaptureErrorFunction implements a javascript function which captures
// name and error of the exception passed as its argument.
class CaptureErrorFunction final : public ScriptFunction {
 public:
  explicit CaptureErrorFunction(ScriptState* script_state)
      : ScriptFunction(script_state) {}

  v8::Local<v8::Function> Bind() { return BindToV8Function(); }
  bool WasCalled() const { return was_called_; }
  const String& Name() const { return name_; }
  const String& Message() const { return message_; }

 private:
  ScriptValue Call(ScriptValue value) override {
    was_called_ = true;

    v8::Isolate* isolate = GetScriptState()->GetIsolate();
    v8::Local<v8::Context> context = GetScriptState()->GetContext();

    v8::Local<v8::Object> error_object =
        value.V8Value()->ToObject(context).ToLocalChecked();

    v8::Local<v8::Value> name =
        error_object->Get(context, V8String(isolate, "name")).ToLocalChecked();
    name_ = ToCoreString(name->ToString(context).ToLocalChecked());
    v8::Local<v8::Value> message =
        error_object->Get(context, V8String(isolate, "message"))
            .ToLocalChecked();
    message_ = ToCoreString(message->ToString(context).ToLocalChecked());

    return ScriptValue();
  }

  bool was_called_ = false;
  String name_;
  String message_;
};

class DynamicModuleResolverTestNotReached final : public ScriptFunction {
 public:
  static v8::Local<v8::Function> CreateFunction(ScriptState* script_state) {
    auto* not_reached =
        MakeGarbageCollected<DynamicModuleResolverTestNotReached>(script_state);
    return not_reached->BindToV8Function();
  }

  explicit DynamicModuleResolverTestNotReached(ScriptState* script_state)
      : ScriptFunction(script_state) {}

 private:
  ScriptValue Call(ScriptValue) override {
    ADD_FAILURE();
    return ScriptValue();
  }
};

}  // namespace

TEST(DynamicModuleResolverTest, ResolveSuccess) {
  V8TestingScope scope;
  auto* modulator = MakeGarbageCollected<DynamicModuleResolverTestModulator>(
      scope.GetScriptState());
  modulator->SetExpectedFetchTreeURL(TestDependencyURL());

  auto* promise_resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(scope.GetScriptState());
  ScriptPromise promise = promise_resolver->Promise();

  auto* capture = MakeGarbageCollected<CaptureExportedStringFunction>(
      scope.GetScriptState(), "foo");
  promise.Then(capture->Bind(),
               DynamicModuleResolverTestNotReached::CreateFunction(
                   scope.GetScriptState()));

  auto* resolver = MakeGarbageCollected<DynamicModuleResolver>(modulator);
  resolver->ResolveDynamically("./dependency.js", TestReferrerURL(),
                               ReferrerScriptInfo(), promise_resolver);

  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());
  EXPECT_FALSE(capture->WasCalled());

  v8::Local<v8::Module> record = ModuleRecord::Compile(
      scope.GetIsolate(), "export const foo = 'hello';", TestReferrerURL(),
      TestReferrerURL(), ScriptFetchOptions(), TextPosition::MinimumPosition(),
      ASSERT_NO_EXCEPTION);
  ModuleScript* module_script =
      JSModuleScript::CreateForTest(modulator, record, TestDependencyURL());
  EXPECT_TRUE(ModuleRecord::Instantiate(scope.GetScriptState(), record,
                                        TestReferrerURL())
                  .IsEmpty());
  modulator->ResolveTreeFetch(module_script);

  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());
  EXPECT_TRUE(capture->WasCalled());
  EXPECT_EQ("hello", capture->CapturedValue());
}

TEST(DynamicModuleResolverTest, ResolveSpecifierFailure) {
  V8TestingScope scope;
  auto* modulator = MakeGarbageCollected<DynamicModuleResolverTestModulator>(
      scope.GetScriptState());
  modulator->SetExpectedFetchTreeURL(TestDependencyURL());

  auto* promise_resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(scope.GetScriptState());
  ScriptPromise promise = promise_resolver->Promise();

  auto* capture =
      MakeGarbageCollected<CaptureErrorFunction>(scope.GetScriptState());
  promise.Then(DynamicModuleResolverTestNotReached::CreateFunction(
                   scope.GetScriptState()),
               capture->Bind());

  auto* resolver = MakeGarbageCollected<DynamicModuleResolver>(modulator);
  resolver->ResolveDynamically("invalid-specifier", TestReferrerURL(),
                               ReferrerScriptInfo(), promise_resolver);

  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());
  EXPECT_TRUE(capture->WasCalled());
  EXPECT_EQ("TypeError", capture->Name());
  EXPECT_TRUE(capture->Message().StartsWith("Failed to resolve"));
}

TEST(DynamicModuleResolverTest, FetchFailure) {
  V8TestingScope scope;
  auto* modulator = MakeGarbageCollected<DynamicModuleResolverTestModulator>(
      scope.GetScriptState());
  modulator->SetExpectedFetchTreeURL(TestDependencyURL());

  auto* promise_resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(scope.GetScriptState());
  ScriptPromise promise = promise_resolver->Promise();

  auto* capture =
      MakeGarbageCollected<CaptureErrorFunction>(scope.GetScriptState());
  promise.Then(DynamicModuleResolverTestNotReached::CreateFunction(
                   scope.GetScriptState()),
               capture->Bind());

  auto* resolver = MakeGarbageCollected<DynamicModuleResolver>(modulator);
  resolver->ResolveDynamically("./dependency.js", TestReferrerURL(),
                               ReferrerScriptInfo(), promise_resolver);

  EXPECT_FALSE(capture->WasCalled());

  modulator->ResolveTreeFetch(nullptr);

  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());
  EXPECT_TRUE(capture->WasCalled());
  EXPECT_EQ("TypeError", capture->Name());
  EXPECT_TRUE(capture->Message().StartsWith("Failed to fetch"));
}

TEST(DynamicModuleResolverTest, ExceptionThrown) {
  V8TestingScope scope;
  auto* modulator = MakeGarbageCollected<DynamicModuleResolverTestModulator>(
      scope.GetScriptState());
  modulator->SetExpectedFetchTreeURL(TestDependencyURL());

  auto* promise_resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(scope.GetScriptState());
  ScriptPromise promise = promise_resolver->Promise();

  auto* capture =
      MakeGarbageCollected<CaptureErrorFunction>(scope.GetScriptState());
  promise.Then(DynamicModuleResolverTestNotReached::CreateFunction(
                   scope.GetScriptState()),
               capture->Bind());

  auto* resolver = MakeGarbageCollected<DynamicModuleResolver>(modulator);
  resolver->ResolveDynamically("./dependency.js", TestReferrerURL(),
                               ReferrerScriptInfo(), promise_resolver);

  EXPECT_FALSE(capture->WasCalled());

  v8::Local<v8::Module> record = ModuleRecord::Compile(
      scope.GetIsolate(), "throw Error('bar')", TestReferrerURL(),
      TestReferrerURL(), ScriptFetchOptions(), TextPosition::MinimumPosition(),
      ASSERT_NO_EXCEPTION);
  ModuleScript* module_script =
      JSModuleScript::CreateForTest(modulator, record, TestDependencyURL());
  EXPECT_TRUE(ModuleRecord::Instantiate(scope.GetScriptState(), record,
                                        TestReferrerURL())
                  .IsEmpty());
  modulator->ResolveTreeFetch(module_script);

  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());
  EXPECT_TRUE(capture->WasCalled());
  EXPECT_EQ("Error", capture->Name());
  EXPECT_EQ("bar", capture->Message());
}

TEST(DynamicModuleResolverTest, ResolveWithNullReferrerScriptSuccess) {
  V8TestingScope scope;
  scope.GetDocument().SetURL(KURL("https://example.com"));

  auto* modulator = MakeGarbageCollected<DynamicModuleResolverTestModulator>(
      scope.GetScriptState());
  modulator->SetExpectedFetchTreeURL(TestDependencyURL());

  auto* promise_resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(scope.GetScriptState());
  ScriptPromise promise = promise_resolver->Promise();

  auto* capture = MakeGarbageCollected<CaptureExportedStringFunction>(
      scope.GetScriptState(), "foo");
  promise.Then(capture->Bind(),
               DynamicModuleResolverTestNotReached::CreateFunction(
                   scope.GetScriptState()));

  auto* resolver = MakeGarbageCollected<DynamicModuleResolver>(modulator);
  resolver->ResolveDynamically("./dependency.js", /* null referrer */ KURL(),
                               ReferrerScriptInfo(), promise_resolver);

  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());
  EXPECT_FALSE(capture->WasCalled());

  v8::Local<v8::Module> record = ModuleRecord::Compile(
      scope.GetIsolate(), "export const foo = 'hello';", TestDependencyURL(),
      TestDependencyURL(), ScriptFetchOptions(),
      TextPosition::MinimumPosition(), ASSERT_NO_EXCEPTION);
  ModuleScript* module_script =
      JSModuleScript::CreateForTest(modulator, record, TestDependencyURL());
  EXPECT_TRUE(ModuleRecord::Instantiate(scope.GetScriptState(), record,
                                        TestDependencyURL())
                  .IsEmpty());
  modulator->ResolveTreeFetch(module_script);

  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());
  EXPECT_TRUE(capture->WasCalled());
  EXPECT_EQ("hello", capture->CapturedValue());
}

TEST(DynamicModuleResolverTest, ResolveWithReferrerScriptInfoBaseURL) {
  V8TestingScope scope;
  scope.GetDocument().SetURL(KURL("https://example.com"));

  auto* modulator = MakeGarbageCollected<DynamicModuleResolverTestModulator>(
      scope.GetScriptState());
  modulator->SetExpectedFetchTreeURL(
      KURL("https://example.com/correct/dependency.js"));

  auto* promise_resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(scope.GetScriptState());
  auto* resolver = MakeGarbageCollected<DynamicModuleResolver>(modulator);
  KURL wrong_base_url("https://example.com/wrong/bar.js");
  KURL correct_base_url("https://example.com/correct/baz.js");
  resolver->ResolveDynamically(
      "./dependency.js", wrong_base_url,
      ReferrerScriptInfo(correct_base_url, ScriptFetchOptions()),
      promise_resolver);

  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());
  EXPECT_TRUE(modulator->fetch_tree_was_called());
}

}  // namespace blink
