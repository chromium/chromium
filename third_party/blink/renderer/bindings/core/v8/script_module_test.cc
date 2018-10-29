// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/script_module.h"

#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/script_source_code.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/script/script_module_resolver.h"
#include "third_party/blink/renderer/core/testing/dummy_modulator.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_context_data.h"
#include "third_party/blink/renderer/platform/loader/fetch/access_control_status.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

class TestScriptModuleResolver final : public ScriptModuleResolver {
 public:
  TestScriptModuleResolver() = default;
  ~TestScriptModuleResolver() override = default;

  size_t ResolveCount() const { return specifiers_.size(); }
  const Vector<String>& Specifiers() const { return specifiers_; }
  void PushScriptModule(ScriptModule script_module) {
    script_modules_.push_back(script_module);
  }

 private:
  // Implements ScriptModuleResolver:

  void RegisterModuleScript(ModuleScript*) override { NOTREACHED(); }
  void UnregisterModuleScript(ModuleScript*) override { NOTREACHED(); }
  ModuleScript* GetHostDefined(const ScriptModule&) const override {
    NOTREACHED();
    return nullptr;
  }

  ScriptModule Resolve(const String& specifier,
                       const ScriptModule&,
                       ExceptionState&) override {
    specifiers_.push_back(specifier);
    return script_modules_.TakeFirst();
  }

  Vector<String> specifiers_;
  Deque<ScriptModule> script_modules_;
};

class ScriptModuleTestModulator final : public DummyModulator {
 public:
  ScriptModuleTestModulator();
  ~ScriptModuleTestModulator() override = default;

  void Trace(blink::Visitor*) override;

  TestScriptModuleResolver* GetTestScriptModuleResolver() {
    return resolver_.Get();
  }

 private:
  // Implements Modulator:

  ScriptModuleResolver* GetScriptModuleResolver() override {
    return resolver_.Get();
  }

  Member<TestScriptModuleResolver> resolver_;
};

ScriptModuleTestModulator::ScriptModuleTestModulator()
    : resolver_(new TestScriptModuleResolver) {}

void ScriptModuleTestModulator::Trace(blink::Visitor* visitor) {
  visitor->Trace(resolver_);
  DummyModulator::Trace(visitor);
}

TEST(ScriptModuleTest, compileSuccess) {
  V8TestingScope scope;
  const KURL js_url("https://example.com/foo.js");
  ScriptModule module = ScriptModule::Compile(
      scope.GetIsolate(), "export const a = 42;", js_url, js_url,
      ScriptFetchOptions(), kSharableCrossOrigin,
      TextPosition::MinimumPosition(), ASSERT_NO_EXCEPTION);
  ASSERT_FALSE(module.IsNull());
}

TEST(ScriptModuleTest, compileFail) {
  V8TestingScope scope;
  const KURL js_url("https://example.com/foo.js");
  ScriptModule module = ScriptModule::Compile(
      scope.GetIsolate(), "123 = 456", js_url, js_url, ScriptFetchOptions(),
      kSharableCrossOrigin, TextPosition::MinimumPosition(),
      scope.GetExceptionState());
  ASSERT_TRUE(module.IsNull());
  EXPECT_TRUE(scope.GetExceptionState().HadException());
}

TEST(ScriptModuleTest, equalAndHash) {
  V8TestingScope scope;
  const KURL js_url_a("https://example.com/a.js");
  const KURL js_url_b("https://example.com/b.js");

  ScriptModule module_null;
  ScriptModule module_a = ScriptModule::Compile(
      scope.GetIsolate(), "export const a = 'a';", js_url_a, js_url_a,
      ScriptFetchOptions(), kSharableCrossOrigin,
      TextPosition::MinimumPosition(), ASSERT_NO_EXCEPTION);
  ASSERT_FALSE(module_a.IsNull());
  ScriptModule module_b = ScriptModule::Compile(
      scope.GetIsolate(), "export const b = 'b';", js_url_b, js_url_b,
      ScriptFetchOptions(), kSharableCrossOrigin,
      TextPosition::MinimumPosition(), ASSERT_NO_EXCEPTION);
  ASSERT_FALSE(module_b.IsNull());
  Vector<char> module_deleted_buffer(sizeof(ScriptModule));
  ScriptModule& module_deleted =
      *reinterpret_cast<ScriptModule*>(module_deleted_buffer.data());
  HashTraits<ScriptModule>::ConstructDeletedValue(module_deleted, true);

  EXPECT_EQ(module_null, module_null);
  EXPECT_EQ(module_a, module_a);
  EXPECT_EQ(module_b, module_b);
  EXPECT_EQ(module_deleted, module_deleted);

  EXPECT_NE(module_null, module_a);
  EXPECT_NE(module_null, module_b);
  EXPECT_NE(module_null, module_deleted);

  EXPECT_NE(module_a, module_null);
  EXPECT_NE(module_a, module_b);
  EXPECT_NE(module_a, module_deleted);

  EXPECT_NE(module_b, module_null);
  EXPECT_NE(module_b, module_a);
  EXPECT_NE(module_b, module_deleted);

  EXPECT_NE(module_deleted, module_null);
  EXPECT_NE(module_deleted, module_a);
  EXPECT_NE(module_deleted, module_b);

  EXPECT_NE(DefaultHash<ScriptModule>::Hash::GetHash(module_a),
            DefaultHash<ScriptModule>::Hash::GetHash(module_b));
  EXPECT_NE(DefaultHash<ScriptModule>::Hash::GetHash(module_null),
            DefaultHash<ScriptModule>::Hash::GetHash(module_a));
  EXPECT_NE(DefaultHash<ScriptModule>::Hash::GetHash(module_null),
            DefaultHash<ScriptModule>::Hash::GetHash(module_b));
}

TEST(ScriptModuleTest, moduleRequests) {
  V8TestingScope scope;
  const KURL js_url("https://example.com/foo.js");
  ScriptModule module = ScriptModule::Compile(
      scope.GetIsolate(), "import 'a'; import 'b'; export const c = 'c';",
      js_url, js_url, ScriptFetchOptions(), kSharableCrossOrigin,
      TextPosition::MinimumPosition(), ASSERT_NO_EXCEPTION);
  ASSERT_FALSE(module.IsNull());

  auto requests = module.ModuleRequests(scope.GetScriptState());
  EXPECT_THAT(requests, testing::ContainerEq<Vector<String>>({"a", "b"}));
}

TEST(ScriptModuleTest, instantiateNoDeps) {
  V8TestingScope scope;

  auto* modulator = new ScriptModuleTestModulator();
  auto* resolver = modulator->GetTestScriptModuleResolver();

  Modulator::SetModulator(scope.GetScriptState(), modulator);

  const KURL js_url("https://example.com/foo.js");
  ScriptModule module = ScriptModule::Compile(
      scope.GetIsolate(), "export const a = 42;", js_url, js_url,
      ScriptFetchOptions(), kSharableCrossOrigin,
      TextPosition::MinimumPosition(), ASSERT_NO_EXCEPTION);
  ASSERT_FALSE(module.IsNull());
  ScriptValue exception = module.Instantiate(scope.GetScriptState());
  ASSERT_TRUE(exception.IsEmpty());

  EXPECT_EQ(0u, resolver->ResolveCount());
}

TEST(ScriptModuleTest, instantiateWithDeps) {
  V8TestingScope scope;

  auto* modulator = new ScriptModuleTestModulator();
  auto* resolver = modulator->GetTestScriptModuleResolver();

  Modulator::SetModulator(scope.GetScriptState(), modulator);

  const KURL js_url_a("https://example.com/a.js");
  ScriptModule module_a = ScriptModule::Compile(
      scope.GetIsolate(), "export const a = 'a';", js_url_a, js_url_a,
      ScriptFetchOptions(), kSharableCrossOrigin,
      TextPosition::MinimumPosition(), ASSERT_NO_EXCEPTION);
  ASSERT_FALSE(module_a.IsNull());
  resolver->PushScriptModule(module_a);

  const KURL js_url_b("https://example.com/b.js");
  ScriptModule module_b = ScriptModule::Compile(
      scope.GetIsolate(), "export const b = 'b';", js_url_b, js_url_b,
      ScriptFetchOptions(), kSharableCrossOrigin,
      TextPosition::MinimumPosition(), ASSERT_NO_EXCEPTION);
  ASSERT_FALSE(module_b.IsNull());
  resolver->PushScriptModule(module_b);

  const KURL js_url_c("https://example.com/c.js");
  ScriptModule module = ScriptModule::Compile(
      scope.GetIsolate(), "import 'a'; import 'b'; export const c = 123;",
      js_url_c, js_url_c, ScriptFetchOptions(), kSharableCrossOrigin,
      TextPosition::MinimumPosition(), ASSERT_NO_EXCEPTION);
  ASSERT_FALSE(module.IsNull());
  ScriptValue exception = module.Instantiate(scope.GetScriptState());
  ASSERT_TRUE(exception.IsEmpty());

  ASSERT_EQ(2u, resolver->ResolveCount());
  EXPECT_EQ("a", resolver->Specifiers()[0]);
  EXPECT_EQ("b", resolver->Specifiers()[1]);
}

TEST(ScriptModuleTest, EvaluationErrrorIsRemembered) {
  V8TestingScope scope;

  auto* modulator = new ScriptModuleTestModulator();
  auto* resolver = modulator->GetTestScriptModuleResolver();

  Modulator::SetModulator(scope.GetScriptState(), modulator);

  const KURL js_url_f("https://example.com/failure.js");
  ScriptModule module_failure = ScriptModule::Compile(
      scope.GetIsolate(), "nonexistent_function()", js_url_f, js_url_f,
      ScriptFetchOptions(), kSharableCrossOrigin,
      TextPosition::MinimumPosition(), ASSERT_NO_EXCEPTION);
  ASSERT_FALSE(module_failure.IsNull());
  ASSERT_TRUE(module_failure.Instantiate(scope.GetScriptState()).IsEmpty());
  ScriptValue evaluation_error =
      module_failure.Evaluate(scope.GetScriptState());
  EXPECT_FALSE(evaluation_error.IsEmpty());

  resolver->PushScriptModule(module_failure);

  const KURL js_url_c("https://example.com/c.js");
  ScriptModule module = ScriptModule::Compile(
      scope.GetIsolate(), "import 'failure'; export const c = 123;", js_url_c,
      js_url_c, ScriptFetchOptions(), kSharableCrossOrigin,
      TextPosition::MinimumPosition(), scope.GetExceptionState());
  ASSERT_FALSE(module.IsNull());
  ASSERT_TRUE(module.Instantiate(scope.GetScriptState()).IsEmpty());
  ScriptValue evaluation_error2 = module.Evaluate(scope.GetScriptState());
  EXPECT_FALSE(evaluation_error2.IsEmpty());

  EXPECT_EQ(evaluation_error, evaluation_error2);

  ASSERT_EQ(1u, resolver->ResolveCount());
  EXPECT_EQ("failure", resolver->Specifiers()[0]);
}

TEST(ScriptModuleTest, Evaluate) {
  V8TestingScope scope;

  auto* modulator = new ScriptModuleTestModulator();
  Modulator::SetModulator(scope.GetScriptState(), modulator);

  const KURL js_url("https://example.com/foo.js");
  ScriptModule module = ScriptModule::Compile(
      scope.GetIsolate(), "export const a = 42; window.foo = 'bar';", js_url,
      js_url, ScriptFetchOptions(), kSharableCrossOrigin,
      TextPosition::MinimumPosition(), ASSERT_NO_EXCEPTION);
  ASSERT_FALSE(module.IsNull());
  ScriptValue exception = module.Instantiate(scope.GetScriptState());
  ASSERT_TRUE(exception.IsEmpty());

  EXPECT_TRUE(module.Evaluate(scope.GetScriptState()).IsEmpty());
  v8::Local<v8::Value> value =
      scope.GetFrame()
          .GetScriptController()
          .ExecuteScriptInMainWorldAndReturnValue(
              ScriptSourceCode("window.foo"), KURL(), kOpaqueResource);
  ASSERT_TRUE(value->IsString());
  EXPECT_EQ("bar", ToCoreString(v8::Local<v8::String>::Cast(value)));

  v8::Local<v8::Object> module_namespace =
      v8::Local<v8::Object>::Cast(module.V8Namespace(scope.GetIsolate()));
  EXPECT_FALSE(module_namespace.IsEmpty());
  v8::Local<v8::Value> exported_value =
      module_namespace
          ->Get(scope.GetContext(), V8String(scope.GetIsolate(), "a"))
          .ToLocalChecked();
  EXPECT_EQ(42.0, exported_value->NumberValue(scope.GetContext()).ToChecked());
}

TEST(ScriptModuleTest, EvaluateCaptureError) {
  V8TestingScope scope;

  auto* modulator = new ScriptModuleTestModulator();
  Modulator::SetModulator(scope.GetScriptState(), modulator);

  const KURL js_url("https://example.com/foo.js");
  ScriptModule module = ScriptModule::Compile(
      scope.GetIsolate(), "throw 'bar';", js_url, js_url, ScriptFetchOptions(),
      kSharableCrossOrigin, TextPosition::MinimumPosition(),
      ASSERT_NO_EXCEPTION);
  ASSERT_FALSE(module.IsNull());
  ScriptValue exception = module.Instantiate(scope.GetScriptState());
  ASSERT_TRUE(exception.IsEmpty());

  ScriptValue error = module.Evaluate(scope.GetScriptState());
  ASSERT_FALSE(error.IsEmpty());
  ASSERT_TRUE(error.V8Value()->IsString());
  EXPECT_EQ("bar", ToCoreString(v8::Local<v8::String>::Cast(error.V8Value())));
}

}  // namespace

}  // namespace blink
