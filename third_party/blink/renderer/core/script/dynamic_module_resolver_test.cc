// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/dynamic_module_resolver.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/referrer_script_info.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_creation_params.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_fetch_request.h"
#include "third_party/blink/renderer/core/script/js_module_script.h"
#include "third_party/blink/renderer/core/testing/dummy_modulator.h"
#include "third_party/blink/renderer/core/testing/module_test_base.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

constexpr const char* kTestReferrerURL = "https://example.com/referrer.js";
constexpr const char* kTestDependencyURL = "https://example.com/dependency.js";
constexpr const char* kTestDependencyURLJSON =
    "https://example.com/dependency.json";

const KURL TestReferrerURL() {
  return KURL(kTestReferrerURL);
}
const KURL TestDependencyURL() {
  return KURL(kTestDependencyURL);
}
const KURL TestDependencyURLJSON() {
  return KURL(kTestDependencyURLJSON);
}
ReferrerScriptInfo TestReferrerScriptInfo() {
  return ReferrerScriptInfo(TestReferrerURL(), ScriptFetchOptions());
}

class DynamicModuleResolverTestModulator final : public DummyModulator {
 public:
  explicit DynamicModuleResolverTestModulator(ScriptState* script_state)
      : script_state_(script_state) {
    Modulator::SetModulator(script_state, this);
  }
  ~DynamicModuleResolverTestModulator() override = default;

  void ResolveTreeFetch(ModuleScript* module_script) {
    ASSERT_TRUE(pending_client_);
    pending_client_->NotifyModuleTreeLoadFinished(module_script);
    pending_client_ = nullptr;
  }
  void SetExpectedFetchTreeURL(const KURL& url) {
    expected_fetch_tree_url_ = url;
  }
  void SetExpectedFetchTreeModuleType(const ModuleType& module_type) {
    expected_fetch_tree_module_type_ = module_type;
  }
  bool fetch_tree_was_called() const { return fetch_tree_was_called_; }

  void Trace(Visitor*) const override;

 private:
  // Implements Modulator:
  ScriptState* GetScriptState() final { return script_state_.Get(); }

  ModuleScript* GetFetchedModuleScript(const KURL& url,
                                       ModuleType module_type) final {
    EXPECT_EQ(TestReferrerURL(), url);
    ModuleScript* module_script =
        JSModuleScript::CreateForTest(this, v8::Local<v8::Module>(), url);
    return module_script;
  }

  KURL ResolveModuleSpecifier(const String& module_request,
                              const KURL& base_url,
                              String*) final {
    if (module_request == "invalid-specifier")
      return KURL();

    return KURL(base_url, module_request);
  }

  void FetchTree(const KURL& url,
                 ModuleType module_type,
                 ResourceFetcher*,
                 mojom::blink::RequestContextType,
                 network::mojom::RequestDestination,
                 const ScriptFetchOptions&,
                 ModuleScriptCustomFetchType custom_fetch_type,
                 ModuleTreeClient* client,
                 String) final {
    EXPECT_EQ(expected_fetch_tree_url_, url);
    EXPECT_EQ(expected_fetch_tree_module_type_, module_type);

    // Currently there are no usage of custom fetch hooks for dynamic import in
    // web specifications.
    EXPECT_EQ(ModuleScriptCustomFetchType::kNone, custom_fetch_type);

    pending_client_ = client;
    fetch_tree_was_called_ = true;
  }

  Member<ScriptState> script_state_;
  Member<ModuleTreeClient> pending_client_;
  KURL expected_fetch_tree_url_;
  ModuleType expected_fetch_tree_module_type_ = ModuleType::kJavaScript;
  bool fetch_tree_was_called_ = false;
};

void DynamicModuleResolverTestModulator::Trace(Visitor* visitor) const {
  visitor->Trace(script_state_);
  visitor->Trace(pending_client_);
  DummyModulator::Trace(visitor);
}

// CaptureExportedStringFunction implements a javascript function
// with a single argument of type module namespace.
// CaptureExportedStringFunction captures the exported string value
// from the module namespace as a WTF::String, exposed via CapturedValue().
class CaptureExportedStringFunction final : public ScriptFunction::Callable {
 public:
  explicit CaptureExportedStringFunction(const String& export_name)
      : export_name_(export_name) {}

  bool WasCalled() const { return was_called_; }
  const String& CapturedValue() const { return captured_value_; }

  ScriptValue Call(ScriptState* script_state, ScriptValue value) override {
    was_called_ = true;

    v8::Isolate* isolate = script_state->GetIsolate();
    v8::Local<v8::Context> context = script_state->GetContext();

    v8::Local<v8::Object> module_namespace =
        value.V8Value()->ToObject(context).ToLocalChecked();
    v8::Local<v8::Value> exported_value =
        module_namespace->Get(context, V8String(isolate, export_name_))
            .ToLocalChecked();
    captured_value_ = ToCoreString(
        isolate, exported_value->ToString(context).ToLocalChecked());

    return ScriptValue();
  }

 private:
  const String export_name_;
  bool was_called_ = false;
  String captured_value_;
};

// CaptureErrorFunction implements a javascript function which captures
// name and error of the exception passed as its argument.
class CaptureErrorFunction final : public ScriptFunction::Callable {
 public:
  CaptureErrorFunction() = default;

  bool WasCalled() const { return was_called_; }
  const String& Name() const { return name_; }
  const String& Message() const { return message_; }

  ScriptValue Call(ScriptState* script_state, ScriptValue value) override {
    was_called_ = true;

    v8::Isolate* isolate = script_state->GetIsolate();
    v8::Local<v8::Context> context = script_state->GetContext();

    v8::Local<v8::Object> error_object =
        value.V8Value()->ToObject(context).ToLocalChecked();

    v8::Local<v8::Value> name =
        error_object->Get(context, V8String(isolate, "name")).ToLocalChecked();
    name_ = ToCoreString(isolate, name->ToString(context).ToLocalChecked());
    v8::Local<v8::Value> message =
        error_object->Get(context, V8String(isolate, "message"))
            .ToLocalChecked();
    message_ =
        ToCoreString(isolate, message->ToString(context).ToLocalChecked());

    return ScriptValue();
  }

 private:
  bool was_called_ = false;
  String name_;
  String message_;
};

class DynamicModuleResolverTestNotReached final
    : public ScriptFunction::Callable {
 public:
  DynamicModuleResolverTestNotReached() = default;

  ScriptValue Call(ScriptState*, ScriptValue) override {
    ADD_FAILURE();
    return ScriptValue();
  }
};

class DynamicModuleResolverTest : public testing::Test, public ModuleTestBase {
 public:
  void SetUp() override { ModuleTestBase::SetUp(); }

  void TearDown() override { ModuleTestBase::TearDown(); }
  test::TaskEnvironment task_environment_;
};

}  // namespace

TEST_F(DynamicModuleResolverTest, ResolveSuccess) {
  V8TestingScope scope;
  auto* modulator = MakeGarbageCollected<DynamicModuleResolverTestModulator>(
      scope.GetScriptState());
  modulator->SetExpectedFetchTreeURL(TestDependencyURL());

  auto* promise_resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLAny>>(
      scope.GetScriptState());
  auto promise = promise_resolver->Promise();

  auto* capture = MakeGarbageCollected<CaptureExportedStringFunction>("foo");
  promise.Then(
      MakeGarbageCollected<ScriptFunction>(scope.GetScriptState(), capture),
      MakeGarbageCollected<ScriptFunction>(
          scope.GetScriptState(),
          MakeGarbageCollected<DynamicModuleResolverTestNotReached>()));

  auto* resolver = MakeGarbageCollected<DynamicModuleResolver>(modulator);
  ModuleRequest module_request("./dependency.js",
                               TextPosition::MinimumPosition(),
                               Vector<ImportAttribute>());
  resolver->ResolveDynamically(module_request, TestReferrerScriptInfo(),
                               promise_resolver);

  scope.PerformMicrotaskCheckpoint();
  EXPECT_FALSE(capture->WasCalled());

  v8::Local<v8::Module> record = ModuleTestBase::CompileModule(
      scope.GetScriptState(), "export const foo = 'hello';", TestReferrerURL());
  ModuleScript* module_script =
      JSModuleScript::CreateForTest(modulator, record, TestDependencyURL());
  EXPECT_TRUE(ModuleRecord::Instantiate(scope.GetScriptState(), record,
                                        TestReferrerURL())
                  .IsEmpty());
  modulator->ResolveTreeFetch(module_script);

  scope.PerformMicrotaskCheckpoint();
  EXPECT_TRUE(capture->WasCalled());
  EXPECT_EQ("hello", capture->CapturedValue());
}

TEST_F(DynamicModuleResolverTest, ResolveJSONModuleSuccess) {
  V8TestingScope scope;
  auto* modulator = MakeGarbageCollected<DynamicModuleResolverTestModulator>(
      scope.GetScriptState());
  modulator->SetExpectedFetchTreeURL(TestDependencyURLJSON());
  modulator->SetExpectedFetchTreeModuleType(ModuleType::kJSON);

  auto* promise_resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLAny>>(
      scope.GetScriptState());
  auto promise = promise_resolver->Promise();

  auto* resolver = MakeGarbageCollected<DynamicModuleResolver>(modulator);
  Vector<ImportAttribute> import_attributes{
      ImportAttribute("type", "json", TextPosition::MinimumPosition())};
  ModuleRequest module_request(
      "./dependency.json", TextPosition::MinimumPosition(), import_attributes);
  resolver->ResolveDynamically(module_request, TestReferrerScriptInfo(),
                               promise_resolver);

  // Instantiating and evaluating a JSON module requires a lot of
  // machinery not currently available in this unit test suite. For
  // the purposes of a DynamicModuleResolver unit test, it should be sufficient
  // to validate that the correct arguments are passed from
  // DynamicModuleResolver::ResolveDynamically to Modulator::FetchTree, which is
  // validated during DynamicModuleResolverTestModulator::FetchTree.
}

TEST_F(DynamicModuleResolverTest, ResolveSpecifierFailure) {
  V8TestingScope scope;
  auto* modulator = MakeGarbageCollected<DynamicModuleResolverTestModulator>(
      scope.GetScriptState());
  modulator->SetExpectedFetchTreeURL(TestDependencyURL());

  auto* promise_resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLAny>>(
      scope.GetScriptState());
  auto promise = promise_resolver->Promise();

  auto* capture = MakeGarbageCollected<CaptureErrorFunction>();
  promise.Then(
      MakeGarbageCollected<ScriptFunction>(
          scope.GetScriptState(),
          MakeGarbageCollected<DynamicModuleResolverTestNotReached>()),
      MakeGarbageCollected<ScriptFunction>(scope.GetScriptState(), capture));

  auto* resolver = MakeGarbageCollected<DynamicModuleResolver>(modulator);
  ModuleRequest module_request("invalid-specifier",
                               TextPosition::MinimumPosition(),
                               Vector<ImportAttribute>());
  resolver->ResolveDynamically(module_request, TestReferrerScriptInfo(),
                               promise_resolver);

  scope.PerformMicrotaskCheckpoint();
  EXPECT_TRUE(capture->WasCalled());
  EXPECT_EQ("TypeError", capture->Name());
  EXPECT_TRUE(capture->Message().StartsWith("Failed to resolve"));
}

TEST_F(DynamicModuleResolverTest, ResolveModuleTypeFailure) {
  V8TestingScope scope;
  auto* modulator = MakeGarbageCollected<DynamicModuleResolverTestModulator>(
      scope.GetScriptState());
  modulator->SetExpectedFetchTreeURL(TestDependencyURL());

  auto* promise_resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLAny>>(
      scope.GetScriptState());
  auto promise = promise_resolver->Promise();

  auto* capture = MakeGarbageCollected<CaptureErrorFunction>();
  promise.Then(
      MakeGarbageCollected<ScriptFunction>(
          scope.GetScriptState(),
          MakeGarbageCollected<DynamicModuleResolverTestNotReached>()),
      MakeGarbageCollected<ScriptFunction>(scope.GetScriptState(), capture));

  auto* resolver = MakeGarbageCollected<DynamicModuleResolver>(modulator);
  Vector<ImportAttribute> import_attributes{
      ImportAttribute("type", "notARealType", TextPosition::MinimumPosition())};
  ModuleRequest module_request(
      "./dependency.js", TextPosition::MinimumPosition(), import_attributes);
  resolver->ResolveDynamically(module_request, TestReferrerScriptInfo(),
                               promise_resolver);

  scope.PerformMicrotaskCheckpoint();
  EXPECT_TRUE(capture->WasCalled());
  EXPECT_EQ("TypeError", capture->Name());
  EXPECT_EQ("\"notARealType\" is not a valid module type.", capture->Message());
}

TEST_F(DynamicModuleResolverTest, FetchFailure) {
  V8TestingScope scope;
  auto* modulator = MakeGarbageCollected<DynamicModuleResolverTestModulator>(
      scope.GetScriptState());
  modulator->SetExpectedFetchTreeURL(TestDependencyURL());

  auto* promise_resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLAny>>(
      scope.GetScriptState());
  auto promise = promise_resolver->Promise();

  auto* capture = MakeGarbageCollected<CaptureErrorFunction>();
  promise.Then(
      MakeGarbageCollected<ScriptFunction>(
          scope.GetScriptState(),
          MakeGarbageCollected<DynamicModuleResolverTestNotReached>()),
      MakeGarbageCollected<ScriptFunction>(scope.GetScriptState(), capture));

  auto* resolver = MakeGarbageCollected<DynamicModuleResolver>(modulator);
  ModuleRequest module_request("./dependency.js",
                               TextPosition::MinimumPosition(),
                               Vector<ImportAttribute>());
  resolver->ResolveDynamically(module_request, TestReferrerScriptInfo(),
                               promise_resolver);

  EXPECT_FALSE(capture->WasCalled());

  modulator->ResolveTreeFetch(nullptr);

  scope.PerformMicrotaskCheckpoint();
  EXPECT_TRUE(capture->WasCalled());
  EXPECT_EQ("TypeError", capture->Name());
  EXPECT_TRUE(capture->Message().StartsWith("Failed to fetch"));
}

TEST_F(DynamicModuleResolverTest, ExceptionThrown) {
  V8TestingScope scope;
  auto* modulator = MakeGarbageCollected<DynamicModuleResolverTestModulator>(
      scope.GetScriptState());
  modulator->SetExpectedFetchTreeURL(TestDependencyURL());

  auto* promise_resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLAny>>(
      scope.GetScriptState());
  auto promise = promise_resolver->Promise();

  auto* capture = MakeGarbageCollected<CaptureErrorFunction>();
  promise.Then(
      MakeGarbageCollected<ScriptFunction>(
          scope.GetScriptState(),
          MakeGarbageCollected<DynamicModuleResolverTestNotReached>()),
      MakeGarbageCollected<ScriptFunction>(scope.GetScriptState(), capture));

  auto* resolver = MakeGarbageCollected<DynamicModuleResolver>(modulator);
  ModuleRequest module_request("./dependency.js",
                               TextPosition::MinimumPosition(),
                               Vector<ImportAttribute>());
  resolver->ResolveDynamically(module_request, TestReferrerScriptInfo(),
                               promise_resolver);

  EXPECT_FALSE(capture->WasCalled());

  v8::Local<v8::Module> record = ModuleTestBase::CompileModule(
      scope.GetScriptState(), "throw Error('bar')", TestReferrerURL());
  ModuleScript* module_script =
      JSModuleScript::CreateForTest(modulator, record, TestDependencyURL());
  EXPECT_TRUE(ModuleRecord::Instantiate(scope.GetScriptState(), record,
                                        TestReferrerURL())
                  .IsEmpty());
  modulator->ResolveTreeFetch(module_script);

  scope.PerformMicrotaskCheckpoint();
  EXPECT_TRUE(capture->WasCalled());
  EXPECT_EQ("Error", capture->Name());
  EXPECT_EQ("bar", capture->Message());
}

TEST_F(DynamicModuleResolverTest, ResolveWithNullReferrerScriptSuccess) {
  V8TestingScope scope;
  scope.GetDocument().SetURL(KURL("https://example.com"));

  auto* modulator = MakeGarbageCollected<DynamicModuleResolverTestModulator>(
      scope.GetScriptState());
  modulator->SetExpectedFetchTreeURL(TestDependencyURL());

  auto* promise_resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLAny>>(
      scope.GetScriptState());
  auto promise = promise_resolver->Promise();

  auto* capture = MakeGarbageCollected<CaptureExportedStringFunction>("foo");
  promise.Then(
      MakeGarbageCollected<ScriptFunction>(scope.GetScriptState(), capture),
      MakeGarbageCollected<ScriptFunction>(
          scope.GetScriptState(),
          MakeGarbageCollected<DynamicModuleResolverTestNotReached>()));

  auto* resolver = MakeGarbageCollected<DynamicModuleResolver>(modulator);
  ModuleRequest module_request("./dependency.js",
                               TextPosition::MinimumPosition(),
                               Vector<ImportAttribute>());
  resolver->ResolveDynamically(module_request, ReferrerScriptInfo(),
                               promise_resolver);

  scope.PerformMicrotaskCheckpoint();
  EXPECT_FALSE(capture->WasCalled());

  v8::Local<v8::Module> record = ModuleTestBase::CompileModule(
      scope.GetScriptState(), "export const foo = 'hello';",
      TestDependencyURL());
  ModuleScript* module_script =
      JSModuleScript::CreateForTest(modulator, record, TestDependencyURL());
  EXPECT_TRUE(ModuleRecord::Instantiate(scope.GetScriptState(), record,
                                        TestDependencyURL())
                  .IsEmpty());
  modulator->ResolveTreeFetch(module_script);

  scope.PerformMicrotaskCheckpoint();
  EXPECT_TRUE(capture->WasCalled());
  EXPECT_EQ("hello", capture->CapturedValue());
}

TEST_F(DynamicModuleResolverTest, ResolveWithReferrerScriptInfoBaseURL) {
  V8TestingScope scope;
  scope.GetDocument().SetURL(KURL("https://example.com"));

  auto* modulator = MakeGarbageCollected<DynamicModuleResolverTestModulator>(
      scope.GetScriptState());
  modulator->SetExpectedFetchTreeURL(
      KURL("https://example.com/correct/dependency.js"));

  auto* promise_resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLAny>>(
      scope.GetScriptState());
  auto* resolver = MakeGarbageCollected<DynamicModuleResolver>(modulator);
  KURL correct_base_url("https://example.com/correct/baz.js");
  ModuleRequest module_request("./dependency.js",
                               TextPosition::MinimumPosition(),
                               Vector<ImportAttribute>());
  resolver->ResolveDynamically(
      module_request,
      ReferrerScriptInfo(correct_base_url, ScriptFetchOptions()),
      promise_resolver);

  scope.PerformMicrotaskCheckpoint();
  EXPECT_TRUE(modulator->fetch_tree_was_called());
}

}  // namespace blink
