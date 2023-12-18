// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/custom/layout_worklet.h"

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/module_record.h"
#include "third_party/blink/renderer/bindings/core/v8/script_evaluation_result.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_gc_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/layout/custom/css_layout_definition.h"
#include "third_party/blink/renderer/core/layout/custom/layout_worklet_global_scope.h"
#include "third_party/blink/renderer/core/layout/custom/layout_worklet_global_scope_proxy.h"
#include "third_party/blink/renderer/core/script/js_module_script.h"
#include "third_party/blink/renderer/core/testing/module_test_base.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class LayoutWorkletTest : public PageTestBase, public ModuleTestBase {
 public:
  void SetUp() override {
    ModuleTestBase::SetUp();
    PageTestBase::SetUp(gfx::Size());
    layout_worklet_ =
        MakeGarbageCollected<LayoutWorklet>(*GetDocument().domWindow());
    proxy_ = layout_worklet_->CreateGlobalScope();
  }

  void TearDown() override {
    Terminate();
    PageTestBase::TearDown();
    ModuleTestBase::TearDown();
  }

  LayoutWorkletGlobalScopeProxy* GetProxy() {
    return LayoutWorkletGlobalScopeProxy::From(proxy_.Get());
  }

  LayoutWorkletGlobalScope* GetGlobalScope() {
    return GetProxy()->global_scope();
  }

  void Terminate() {
    proxy_->TerminateWorkletGlobalScope();
    proxy_ = nullptr;
  }

  ScriptState* GetScriptState() {
    return GetGlobalScope()->ScriptController()->GetScriptState();
  }

  ScriptEvaluationResult EvaluateScriptModule(const String& source_code) {
    ScriptState* script_state = GetScriptState();
    v8::MicrotasksScope microtasks_scope(
        script_state->GetIsolate(), ToMicrotaskQueue(script_state),
        v8::MicrotasksScope::kDoNotRunMicrotasks);
    EXPECT_TRUE(script_state);

    KURL js_url("https://example.com/worklet.js");
    v8::Local<v8::Module> module =
        ModuleTestBase::CompileModule(script_state, source_code, js_url);
    EXPECT_FALSE(module.IsEmpty());

    ScriptValue exception =
        ModuleRecord::Instantiate(script_state, module, js_url);
    EXPECT_TRUE(exception.IsEmpty());

    return JSModuleScript::CreateForTest(Modulator::From(script_state), module,
                                         js_url)
        ->RunScriptOnScriptStateAndReturnValue(script_state);
  }

 private:
  Persistent<WorkletGlobalScopeProxy> proxy_;
  Persistent<LayoutWorklet> layout_worklet_;
};

TEST_F(LayoutWorkletTest, ParseProperties) {
  ScriptState::Scope scope(GetScriptState());
  ScriptEvaluationResult result = EvaluateScriptModule(R"JS(
    registerLayout('foo', class {
      static get inputProperties() { return ['--prop', 'flex-basis', 'thing'] }
      static get childInputProperties() { return ['--child-prop', 'margin-top', 'other-thing'] }
      async intrinsicSizes() { }
      async layout() { }
    });
  )JS");
  EXPECT_FALSE(GetResult(GetScriptState(), std::move(result)).IsEmpty());

  LayoutWorkletGlobalScope* global_scope = GetGlobalScope();
  CSSLayoutDefinition* definition =
      global_scope->FindDefinition(AtomicString("foo"));
  EXPECT_NE(nullptr, definition);

  Vector<CSSPropertyID> native_invalidation_properties = {
      CSSPropertyID::kFlexBasis};
  Vector<AtomicString> custom_invalidation_properties = {
      AtomicString("--prop")};
  Vector<CSSPropertyID> child_native_invalidation_properties = {
      CSSPropertyID::kMarginTop};
  Vector<AtomicString> child_custom_invalidation_properties = {
      AtomicString("--child-prop")};

  EXPECT_EQ(native_invalidation_properties,
            definition->NativeInvalidationProperties());
  EXPECT_EQ(custom_invalidation_properties,
            definition->CustomInvalidationProperties());
  EXPECT_EQ(child_native_invalidation_properties,
            definition->ChildNativeInvalidationProperties());
  EXPECT_EQ(child_custom_invalidation_properties,
            definition->ChildCustomInvalidationProperties());
}

// TODO(ikilpatrick): Move all the tests below to wpt tests once we have the
// layout API actually have effects that we can test in script.

TEST_F(LayoutWorkletTest, RegisterLayout) {
  ScriptState::Scope scope(GetScriptState());
  ScriptEvaluationResult result = EvaluateScriptModule(R"JS(
    registerLayout('foo', class {
      async intrinsicSizes() { }
      async layout() { }
    });
  )JS");

  EXPECT_FALSE(GetResult(GetScriptState(), std::move(result)).IsEmpty());

  result = EvaluateScriptModule(R"JS(
    registerLayout('bar', class {
      static get inputProperties() { return ['--prop'] }
      static get childInputProperties() { return ['--child-prop'] }
      async intrinsicSizes() { }
      async layout() { }
    });
  )JS");

  EXPECT_FALSE(GetResult(GetScriptState(), std::move(result)).IsEmpty());
}

TEST_F(LayoutWorkletTest, RegisterLayout_EmptyName) {
  ScriptState::Scope scope(GetScriptState());
  ScriptEvaluationResult result = EvaluateScriptModule(R"JS(
    registerLayout('', class {
    });
  )JS");

  // "The empty string is not a valid name."
  EXPECT_FALSE(GetException(GetScriptState(), std::move(result)).IsEmpty());
}

TEST_F(LayoutWorkletTest, RegisterLayout_Duplicate) {
  ScriptState::Scope scope(GetScriptState());
  ScriptEvaluationResult result = EvaluateScriptModule(R"JS(
    registerLayout('foo', class {
      async intrinsicSizes() { }
      async layout() { }
    });
    registerLayout('foo', class {
      async intrinsicSizes() { }
      async layout() { }
    });
  )JS");

  // "A class with name:'foo' is already registered."
  EXPECT_FALSE(GetException(GetScriptState(), std::move(result)).IsEmpty());
}

TEST_F(LayoutWorkletTest, RegisterLayout_NoIntrinsicSizes) {
  ScriptState::Scope scope(GetScriptState());
  ScriptEvaluationResult result = EvaluateScriptModule(R"JS(
    registerLayout('foo', class {
    });
  )JS");

  // "The 'intrinsicSizes' property on the prototype does not exist."
  EXPECT_FALSE(GetException(GetScriptState(), std::move(result)).IsEmpty());
}

TEST_F(LayoutWorkletTest, RegisterLayout_ThrowingPropertyGetter) {
  ScriptState::Scope scope(GetScriptState());
  ScriptEvaluationResult result = EvaluateScriptModule(R"JS(
    registerLayout('foo', class {
      static get inputProperties() { throw Error(); }
    });
  )JS");

  // "Uncaught Error"
  EXPECT_FALSE(GetException(GetScriptState(), std::move(result)).IsEmpty());
}

TEST_F(LayoutWorkletTest, RegisterLayout_BadPropertyGetter) {
  ScriptState::Scope scope(GetScriptState());
  ScriptEvaluationResult result = EvaluateScriptModule(R"JS(
    registerLayout('foo', class {
      static get inputProperties() { return 42; }
    });
  )JS");

  // "The provided value cannot be converted to a sequence."
  EXPECT_FALSE(GetException(GetScriptState(), std::move(result)).IsEmpty());
}

TEST_F(LayoutWorkletTest, RegisterLayout_NoPrototype) {
  ScriptState::Scope scope(GetScriptState());
  ScriptEvaluationResult result = EvaluateScriptModule(R"JS(
    const foo = function() { };
    foo.prototype = undefined;
    registerLayout('foo', foo);
  )JS");

  // "The 'prototype' object on the class does not exist."
  EXPECT_FALSE(GetException(GetScriptState(), std::move(result)).IsEmpty());
}

TEST_F(LayoutWorkletTest, RegisterLayout_BadPrototype) {
  ScriptState::Scope scope(GetScriptState());
  ScriptEvaluationResult result = EvaluateScriptModule(R"JS(
    const foo = function() { };
    foo.prototype = 42;
    registerLayout('foo', foo);
  )JS");

  // "The 'prototype' property on the class is not an object."
  EXPECT_FALSE(GetException(GetScriptState(), std::move(result)).IsEmpty());
}

TEST_F(LayoutWorkletTest, RegisterLayout_BadIntrinsicSizes) {
  ScriptState::Scope scope(GetScriptState());
  ScriptEvaluationResult result = EvaluateScriptModule(R"JS(
    registerLayout('foo', class {
      get intrinsicSizes() { return 42; }
    });
  )JS");

  // "The 'intrinsicSizes' property on the prototype is not a function."
  EXPECT_FALSE(GetException(GetScriptState(), std::move(result)).IsEmpty());
}

TEST_F(LayoutWorkletTest, RegisterLayout_NoLayout) {
  ScriptState::Scope scope(GetScriptState());
  ScriptEvaluationResult result = EvaluateScriptModule(R"JS(
    registerLayout('foo', class {
      async intrinsicSizes() { }
    });
  )JS");

  // "The 'layout' property on the prototype does not exist."
  EXPECT_FALSE(GetException(GetScriptState(), std::move(result)).IsEmpty());
}

TEST_F(LayoutWorkletTest, RegisterLayout_BadLayout) {
  ScriptState::Scope scope(GetScriptState());
  ScriptEvaluationResult result = EvaluateScriptModule(R"JS(
    registerLayout('foo', class {
      async intrinsicSizes() { }
      get layout() { return 42; }
    });
  )JS");

  // "The 'layout' property on the prototype is not a function."
  EXPECT_FALSE(GetException(GetScriptState(), std::move(result)).IsEmpty());
}

}  // namespace blink
