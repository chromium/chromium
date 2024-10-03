// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/module_record.h"

#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/boxed_v8_module.h"
#include "third_party/blink/renderer/bindings/core/v8/module_request.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/core/script/js_module_script.h"
#include "third_party/blink/renderer/core/script/module_record_resolver.h"
#include "third_party/blink/renderer/core/testing/dummy_modulator.h"
#include "third_party/blink/renderer/core/testing/module_test_base.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_context_data.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_deque.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

class TestModuleRecordResolver final : public ModuleRecordResolver {
 public:
  explicit TestModuleRecordResolver(v8::Isolate* isolate) : isolate_(isolate) {}
  ~TestModuleRecordResolver() override = default;

  size_t ResolveCount() const { return specifiers_.size(); }
  const Vector<String>& Specifiers() const { return specifiers_; }
  void PrepareMockResolveResult(v8::Local<v8::Module> module) {
    module_records_.push_back(
        MakeGarbageCollected<BoxedV8Module>(isolate_, module));
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(module_records_);
    ModuleRecordResolver::Trace(visitor);
  }

 private:
  // Implements ModuleRecordResolver:

  void RegisterModuleScript(const ModuleScript*) override {}
  void UnregisterModuleScript(const ModuleScript*) override {
    NOTREACHED_IN_MIGRATION();
  }

  const ModuleScript* GetModuleScriptFromModuleRecord(
      v8::Local<v8::Module>) const override {
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }

  v8::Local<v8::Module> Resolve(const ModuleRequest& module_request,
                                v8::Local<v8::Module> module,
                                ExceptionState&) override {
    specifiers_.push_back(module_request.specifier);
    return module_records_.TakeFirst()->NewLocal(isolate_);
  }

  v8::Isolate* isolate_;
  Vector<String> specifiers_;
  HeapDeque<Member<BoxedV8Module>> module_records_;
};

class ModuleRecordTestModulator final : public DummyModulator {
 public:
  explicit ModuleRecordTestModulator(ScriptState*);
  ~ModuleRecordTestModulator() override = default;

  void Trace(Visitor*) const override;

  TestModuleRecordResolver* GetTestModuleRecordResolver() {
    return resolver_.Get();
  }

 private:
  // Implements Modulator:

  ScriptState* GetScriptState() override { return script_state_.Get(); }

  ModuleRecordResolver* GetModuleRecordResolver() override {
    return resolver_.Get();
  }

  Member<ScriptState> script_state_;
  Member<TestModuleRecordResolver> resolver_;
};

ModuleRecordTestModulator::ModuleRecordTestModulator(ScriptState* script_state)
    : script_state_(script_state),
      resolver_(MakeGarbageCollected<TestModuleRecordResolver>(
          script_state->GetIsolate())) {
  Modulator::SetModulator(script_state, this);
}

void ModuleRecordTestModulator::Trace(Visitor* visitor) const {
  visitor->Trace(script_state_);
  visitor->Trace(resolver_);
  DummyModulator::Trace(visitor);
}

class ModuleRecordTest : public ::testing::Test, public ModuleTestBase {
 public:
  void SetUp() override { ModuleTestBase::SetUp(); }
  void TearDown() override { ModuleTestBase::TearDown(); }

  test::TaskEnvironment task_environment_;
};

TEST_F(ModuleRecordTest, compileSuccess) {
  V8TestingScope scope;
  const KURL js_url("https://example.com/foo.js");
  v8::Local<v8::Module> module = ModuleTestBase::CompileModule(
      scope.GetScriptState(), "export const a = 42;", js_url);
  ASSERT_FALSE(module.IsEmpty());
}

TEST_F(ModuleRecordTest, compileFail) {
  V8TestingScope scope;
  v8::TryCatch try_catch(scope.GetIsolate());
  const KURL js_url("https://example.com/foo.js");
  v8::Local<v8::Module> module = ModuleTestBase::CompileModule(
      scope.GetScriptState(), "123 = 456", js_url);
  ASSERT_TRUE(module.IsEmpty());
  EXPECT_TRUE(try_catch.HasCaught());
}

TEST_F(ModuleRecordTest, moduleRequests) {
  V8TestingScope scope;
  const KURL js_url("https://example.com/foo.js");
  v8::Local<v8::Module> module = ModuleTestBase::CompileModule(
      scope.GetScriptState(), "import 'a'; import 'b'; export const c = 'c';",
      js_url);
  ASSERT_FALSE(module.IsEmpty());

  auto requests = ModuleRecord::ModuleRequests(scope.GetScriptState(), module);
  EXPECT_EQ(2u, requests.size());
  EXPECT_EQ("a", requests[0].specifier);
  EXPECT_EQ(0u, requests[0].import_attributes.size());
  EXPECT_EQ("b", requests[1].specifier);
  EXPECT_EQ(0u, requests[1].import_attributes.size());
}

TEST_F(ModuleRecordTest, moduleRequestsWithImportAttributes) {
  V8TestingScope scope;
  v8::V8::SetFlagsFromString("--harmony-import-attributes");
  const KURL js_url("https://example.com/foo.js");
  v8::Local<v8::Module> module =
      ModuleTestBase::CompileModule(scope.GetScriptState(),
                                    "import 'a' with { };"
                                    "import 'b' with { type: 'x'};"
                                    "import 'c' with { foo: 'y', type: 'z' };",
                                    js_url);
  ASSERT_FALSE(module.IsEmpty());

  auto requests = ModuleRecord::ModuleRequests(scope.GetScriptState(), module);
  EXPECT_EQ(3u, requests.size());
  EXPECT_EQ("a", requests[0].specifier);
  EXPECT_EQ(0u, requests[0].import_attributes.size());
  EXPECT_EQ(String(), requests[0].GetModuleTypeString());

  EXPECT_EQ("b", requests[1].specifier);
  EXPECT_EQ(1u, requests[1].import_attributes.size());
  EXPECT_EQ("x", requests[1].GetModuleTypeString());

  EXPECT_EQ("c", requests[2].specifier);
  EXPECT_EQ("z", requests[2].GetModuleTypeString());
}

TEST_F(ModuleRecordTest, instantiateNoDeps) {
  V8TestingScope scope;

  auto* modulator =
      MakeGarbageCollected<ModuleRecordTestModulator>(scope.GetScriptState());
  auto* resolver = modulator->GetTestModuleRecordResolver();

  const KURL js_url("https://example.com/foo.js");
  v8::Local<v8::Module> module = ModuleTestBase::CompileModule(
      scope.GetScriptState(), "export const a = 42;", js_url);
  ASSERT_FALSE(module.IsEmpty());
  ScriptValue exception =
      ModuleRecord::Instantiate(scope.GetScriptState(), module, js_url);
  ASSERT_TRUE(exception.IsEmpty());

  EXPECT_EQ(0u, resolver->ResolveCount());
}

TEST_F(ModuleRecordTest, instantiateWithDeps) {
  V8TestingScope scope;

  auto* modulator =
      MakeGarbageCollected<ModuleRecordTestModulator>(scope.GetScriptState());
  auto* resolver = modulator->GetTestModuleRecordResolver();

  const KURL js_url_a("https://example.com/a.js");
  v8::Local<v8::Module> module_a = ModuleTestBase::CompileModule(
      scope.GetScriptState(), "export const a = 'a';", js_url_a);
  ASSERT_FALSE(module_a.IsEmpty());
  resolver->PrepareMockResolveResult(module_a);

  const KURL js_url_b("https://example.com/b.js");
  v8::Local<v8::Module> module_b = ModuleTestBase::CompileModule(
      scope.GetScriptState(), "export const b = 'b';", js_url_b);
  ASSERT_FALSE(module_b.IsEmpty());
  resolver->PrepareMockResolveResult(module_b);

  const KURL js_url_c("https://example.com/c.js");
  v8::Local<v8::Module> module = ModuleTestBase::CompileModule(
      scope.GetScriptState(), "import 'a'; import 'b'; export const c = 123;",
      js_url_c);
  ASSERT_FALSE(module.IsEmpty());
  ScriptValue exception =
      ModuleRecord::Instantiate(scope.GetScriptState(), module, js_url_c);
  ASSERT_TRUE(exception.IsEmpty());

  ASSERT_EQ(2u, resolver->ResolveCount());
  EXPECT_EQ("a", resolver->Specifiers()[0]);
  EXPECT_EQ("b", resolver->Specifiers()[1]);
}

TEST_F(ModuleRecordTest, EvaluationErrorIsRemembered) {
  V8TestingScope scope;
  ScriptState* state = scope.GetScriptState();

  auto* modulator = MakeGarbageCollected<ModuleRecordTestModulator>(state);
  auto* resolver = modulator->GetTestModuleRecordResolver();

  const KURL js_url_f("https://example.com/failure.js");
  v8::Local<v8::Module> module_failure = ModuleTestBase::CompileModule(
      scope.GetScriptState(), "nonexistent_function()", js_url_f);
  ASSERT_FALSE(module_failure.IsEmpty());
  ASSERT_TRUE(
      ModuleRecord::Instantiate(state, module_failure, js_url_f).IsEmpty());
  ScriptEvaluationResult evaluation_result1 =
      JSModuleScript::CreateForTest(modulator, module_failure, js_url_f)
          ->RunScriptOnScriptStateAndReturnValue(scope.GetScriptState());

  resolver->PrepareMockResolveResult(module_failure);

  const KURL js_url_c("https://example.com/c.js");
  v8::Local<v8::Module> module = ModuleTestBase::CompileModule(
      scope.GetScriptState(), "import 'failure'; export const c = 123;",
      js_url_c);
  ASSERT_FALSE(module.IsEmpty());
  ASSERT_TRUE(ModuleRecord::Instantiate(state, module, js_url_c).IsEmpty());
  ScriptEvaluationResult evaluation_result2 =
      JSModuleScript::CreateForTest(modulator, module, js_url_c)
          ->RunScriptOnScriptStateAndReturnValue(scope.GetScriptState());

  v8::Local<v8::Value> exception1 =
      GetException(state, std::move(evaluation_result1));
  v8::Local<v8::Value> exception2 =
      GetException(state, std::move(evaluation_result2));
  EXPECT_FALSE(exception1.IsEmpty());
  EXPECT_FALSE(exception2.IsEmpty());
  EXPECT_EQ(exception1, exception2);

  ASSERT_EQ(1u, resolver->ResolveCount());
  EXPECT_EQ("failure", resolver->Specifiers()[0]);
}

TEST_F(ModuleRecordTest, Evaluate) {
  V8TestingScope scope;

  auto* modulator =
      MakeGarbageCollected<ModuleRecordTestModulator>(scope.GetScriptState());

  const KURL js_url("https://example.com/foo.js");
  v8::Local<v8::Module> module = ModuleTestBase::CompileModule(
      scope.GetScriptState(), "export const a = 42; window.foo = 'bar';",
      js_url);
  ASSERT_FALSE(module.IsEmpty());
  ScriptValue exception =
      ModuleRecord::Instantiate(scope.GetScriptState(), module, js_url);
  ASSERT_TRUE(exception.IsEmpty());

  EXPECT_EQ(JSModuleScript::CreateForTest(modulator, module, js_url)
                ->RunScriptOnScriptStateAndReturnValue(scope.GetScriptState())
                .GetResultType(),
            ScriptEvaluationResult::ResultType::kSuccess);
  v8::Local<v8::Value> value =
      ClassicScript::CreateUnspecifiedScript("window.foo")
          ->RunScriptAndReturnValue(&scope.GetWindow())
          .GetSuccessValueOrEmpty();
  ASSERT_TRUE(value->IsString());
  EXPECT_EQ("bar", ToCoreString(scope.GetIsolate(),
                                v8::Local<v8::String>::Cast(value)));

  v8::Local<v8::Object> module_namespace =
      v8::Local<v8::Object>::Cast(ModuleRecord::V8Namespace(module));
  EXPECT_FALSE(module_namespace.IsEmpty());
  v8::Local<v8::Value> exported_value =
      module_namespace
          ->Get(scope.GetContext(), V8String(scope.GetIsolate(), "a"))
          .ToLocalChecked();
  EXPECT_EQ(42.0, exported_value->NumberValue(scope.GetContext()).ToChecked());
}

TEST_F(ModuleRecordTest, EvaluateCaptureError) {
  V8TestingScope scope;

  auto* modulator =
      MakeGarbageCollected<ModuleRecordTestModulator>(scope.GetScriptState());

  const KURL js_url("https://example.com/foo.js");
  v8::Local<v8::Module> module = ModuleTestBase::CompileModule(
      scope.GetScriptState(), "throw 'bar';", js_url);
  ASSERT_FALSE(module.IsEmpty());
  ScriptValue instantiation_exception =
      ModuleRecord::Instantiate(scope.GetScriptState(), module, js_url);
  ASSERT_TRUE(instantiation_exception.IsEmpty());

  ScriptEvaluationResult result =
      JSModuleScript::CreateForTest(modulator, module, js_url)
          ->RunScriptOnScriptStateAndReturnValue(scope.GetScriptState());

  v8::Local<v8::Value> exception =
      GetException(scope.GetScriptState(), std::move(result));
  ASSERT_TRUE(exception->IsString());
  EXPECT_EQ("bar",
            ToCoreString(scope.GetIsolate(), exception.As<v8::String>()));
}

}  // namespace

}  // namespace blink
