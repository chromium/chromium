// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/paint_worklet.h"

#include <memory>
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_gc_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/css/css_syntax_definition.h"
#include "third_party/blink/renderer/core/css/cssom/prepopulated_computed_style_property_map.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/modules/csspaint/css_paint_definition.h"
#include "third_party/blink/renderer/modules/csspaint/paint_worklet_global_scope.h"
#include "third_party/blink/renderer/modules/csspaint/paint_worklet_global_scope_proxy.h"
#include "third_party/blink/renderer/platform/graphics/paint_generated_image.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {
class TestPaintWorklet : public PaintWorklet {
 public:
  explicit TestPaintWorklet(LocalDOMWindow& window) : PaintWorklet(window) {
    ResetIsPaintOffThreadForTesting();
  }

  void SetPaintsToSwitch(int num) { paints_to_switch_ = num; }

  int GetPaintsBeforeSwitching() override { return paints_to_switch_; }

  // We always switch to another global scope so that we can tell how often it
  // was switched in the test.
  wtf_size_t SelectNewGlobalScope() override {
    return (GetActiveGlobalScopeForTesting() + 1) %
           PaintWorklet::kNumGlobalScopesPerThread;
  }

  size_t GetActiveGlobalScope() { return GetActiveGlobalScopeForTesting(); }

 private:
  int paints_to_switch_;
};

class PaintWorkletTest : public PageTestBase {
 public:
  void SetUp() override {
    PageTestBase::SetUp(gfx::Size());
    test_paint_worklet_ =
        MakeGarbageCollected<TestPaintWorklet>(*GetDocument().domWindow());
    proxy_ = test_paint_worklet_->CreateGlobalScope();
  }

  TestPaintWorklet* GetTestPaintWorklet() { return test_paint_worklet_.Get(); }

  size_t SelectGlobalScope(TestPaintWorklet* paint_worklet) {
    return paint_worklet->SelectGlobalScope();
  }

  PaintWorkletGlobalScopeProxy* GetProxy() {
    return PaintWorkletGlobalScopeProxy::From(proxy_.Get());
  }

  ImageResourceObserver* GetImageResourceObserver() {
    return GetDocument().domWindow()->GetFrame()->ContentLayoutObject();
  }

  // Helper function used in GlobalScopeSelection test.
  void ExpectSwitchGlobalScope(bool expect_switch_within_frame,
                               size_t num_paint_calls,
                               int paint_cnt_to_switch,
                               size_t expected_num_paints_before_switch,
                               TestPaintWorklet* paint_worklet_to_test) {
    paint_worklet_to_test->DomWindow()
        ->GetFrame()
        ->View()
        ->UpdateAllLifecyclePhasesForTest();
    paint_worklet_to_test->SetPaintsToSwitch(paint_cnt_to_switch);
    size_t previously_selected_global_scope =
        paint_worklet_to_test->GetActiveGlobalScope();
    size_t global_scope_switch_count = 0u;

    // How many paint calls are there before we switch to another global scope.
    // Because the first paint call in each frame doesn't count as switching,
    // a result of 0 means there is not switching in that frame.
    size_t num_paints_before_switch = 0u;
    for (size_t j = 0; j < num_paint_calls; j++) {
      size_t selected_global_scope = SelectGlobalScope(paint_worklet_to_test);
      if (j == 0) {
        EXPECT_NE(selected_global_scope, previously_selected_global_scope);
      } else if (selected_global_scope != previously_selected_global_scope) {
        num_paints_before_switch = j + 1;
        global_scope_switch_count++;
      }
      previously_selected_global_scope = selected_global_scope;
    }
    EXPECT_LT(global_scope_switch_count, 2u);
    EXPECT_EQ(num_paints_before_switch, expected_num_paints_before_switch);
  }

  void TearDown() override {
    proxy_->TerminateWorkletGlobalScope();
    proxy_ = nullptr;
    PageTestBase::TearDown();
  }

 private:
  Persistent<WorkletGlobalScopeProxy> proxy_;
  Persistent<TestPaintWorklet> test_paint_worklet_;
};

// This is a crash test for crbug.com/803026. At some point, we shipped the
// CSSPaintAPI without shipping the CSSPaintAPIArguments, the result of it is
// that the |paint_arguments| in the CSSPaintDefinition::Paint() becomes
// nullptr and the renderer crashes. This is a regression test to ensure that
// we will never crash.
TEST_F(PaintWorkletTest, PaintWithNullPaintArguments) {
  PaintWorkletGlobalScope* global_scope = GetProxy()->global_scope();
  ClassicScript::CreateUnspecifiedScript(
      "registerPaint('foo', class { paint() { } });")
      ->RunScriptOnScriptState(
          global_scope->ScriptController()->GetScriptState());

  CSSPaintDefinition* definition = global_scope->FindDefinition("foo");
  ASSERT_TRUE(definition);

  ImageResourceObserver* observer = GetImageResourceObserver();
  ASSERT_TRUE(observer);

  const gfx::SizeF container_size(100, 100);
  const LayoutObject& layout_object =
      static_cast<const LayoutObject&>(*observer);
  float zoom = layout_object.StyleRef().EffectiveZoom();
  StylePropertyMapReadOnly* style_map =
      MakeGarbageCollected<PrepopulatedComputedStylePropertyMap>(
          layout_object.GetDocument(), layout_object.StyleRef(),
          definition->NativeInvalidationProperties(),
          definition->CustomInvalidationProperties());
  scoped_refptr<Image> image = PaintGeneratedImage::Create(
      definition->Paint(container_size, zoom, style_map, nullptr),
      container_size);
  EXPECT_NE(image, nullptr);
}

// In this test, we have only one global scope, which means registerPaint is
// called only once, and hence we have only one document paint definition
// registered. In the real world, this document paint definition should not be
// used to paint until we see a second one being registed with the same name.
TEST_F(PaintWorkletTest, SinglyRegisteredDocumentDefinitionNotUsed) {
  PaintWorklet* paint_worklet_to_test =
      PaintWorklet::From(*GetFrame().GetDocument()->domWindow());
  paint_worklet_to_test->ResetIsPaintOffThreadForTesting();

  PaintWorkletGlobalScope* global_scope = GetProxy()->global_scope();
  ClassicScript::CreateUnspecifiedScript(
      "registerPaint('foo', class { paint() { } });")
      ->RunScriptOnScriptState(
          global_scope->ScriptController()->GetScriptState());

  CSSPaintImageGeneratorImpl* generator =
      static_cast<CSSPaintImageGeneratorImpl*>(
          CSSPaintImageGeneratorImpl::Create("foo", GetDocument(), nullptr));
  EXPECT_TRUE(generator);
  EXPECT_EQ(generator->GetRegisteredDefinitionCountForTesting(), 1u);
  DocumentPaintDefinition* definition;
  // Please refer to CSSPaintImageGeneratorImpl::GetValidDocumentDefinition for
  // the logic.
  if (RuntimeEnabledFeatures::OffMainThreadCSSPaintEnabled()) {
    EXPECT_TRUE(generator->GetValidDocumentDefinitionForTesting(definition));
  } else {
    EXPECT_FALSE(generator->GetValidDocumentDefinitionForTesting(definition));
    EXPECT_FALSE(definition);
  }
}

// In this test, we set a list of "paints_to_switch" numbers, and in each frame,
// we switch to a new global scope when the number of paint calls is >= the
// corresponding number.
TEST_F(PaintWorkletTest, GlobalScopeSelection) {
  TestPaintWorklet* paint_worklet_to_test = GetTestPaintWorklet();

  ExpectSwitchGlobalScope(false, 5, 1, 0, paint_worklet_to_test);
  ExpectSwitchGlobalScope(true, 15, 10, 10, paint_worklet_to_test);
  // In the last one where |paints_to_switch| is 20, there is no switching after
  // the first paint call.
  ExpectSwitchGlobalScope(false, 10, 20, 0, paint_worklet_to_test);
}

TEST_F(PaintWorkletTest, NativeAndCustomProperties) {
  ScopedOffMainThreadCSSPaintForTest off_main_thread_css_paint(true);
  Vector<CSSPropertyID> native_invalidation_properties = {
      CSSPropertyID::kColor,
      CSSPropertyID::kZoom,
      CSSPropertyID::kTop,
  };
  Vector<String> custom_invalidation_properties = {
      "--my-property",
      "--another-property",
  };

  TestPaintWorklet* paint_worklet_to_test = GetTestPaintWorklet();
  paint_worklet_to_test->RegisterMainThreadDocumentPaintDefinition(
      "foo", native_invalidation_properties, custom_invalidation_properties,
      Vector<CSSSyntaxDefinition>(), true);

  CSSPaintImageGeneratorImpl* generator =
      MakeGarbageCollected<CSSPaintImageGeneratorImpl>(paint_worklet_to_test,
                                                       "foo");
  EXPECT_NE(generator, nullptr);
  EXPECT_EQ(generator->NativeInvalidationProperties().size(), 3u);
  EXPECT_EQ(generator->CustomInvalidationProperties().size(), 2u);
  EXPECT_TRUE(generator->HasAlpha());
}

class MainOrOffThreadPaintWorkletTest
    : public PageTestBase,
      public ::testing::WithParamInterface<bool>,
      private ScopedOffMainThreadCSSPaintForTest {
 public:
  MainOrOffThreadPaintWorkletTest()
      : ScopedOffMainThreadCSSPaintForTest(GetParam()) {}
};

INSTANTIATE_TEST_SUITE_P(All,
                         MainOrOffThreadPaintWorkletTest,
                         ::testing::Bool());

class MockObserver final : public CSSPaintImageGenerator::Observer {
 public:
  MOCK_METHOD0(PaintImageGeneratorReady, void());
};

TEST_P(MainOrOffThreadPaintWorkletTest, ConsistentGlobalScopeOnMainThread) {
  PaintWorklet* paint_worklet_to_test =
      PaintWorklet::From(*GetFrame().GetDocument()->domWindow());
  paint_worklet_to_test->ResetIsPaintOffThreadForTesting();

  MockObserver* observer = MakeGarbageCollected<MockObserver>();
  CSSPaintImageGeneratorImpl* generator_foo =
      MakeGarbageCollected<CSSPaintImageGeneratorImpl>(
          observer, paint_worklet_to_test, "foo");
  paint_worklet_to_test->AddPendingGenerator("foo", generator_foo);
  CSSPaintImageGeneratorImpl* generator_bar =
      MakeGarbageCollected<CSSPaintImageGeneratorImpl>(
          observer, paint_worklet_to_test, "bar");
  paint_worklet_to_test->AddPendingGenerator("bar", generator_bar);

  // The generator should not fire unless it is the second registration
  // for the main thread case
  EXPECT_CALL(*observer, PaintImageGeneratorReady).Times(0);

  Vector<Persistent<PaintWorkletGlobalScope>> global_scopes;
  for (wtf_size_t i = 0; i < PaintWorklet::kNumGlobalScopesPerThread; ++i) {
    paint_worklet_to_test->AddGlobalScopeForTesting();
    global_scopes.push_back(
        PaintWorkletGlobalScopeProxy::From(
            paint_worklet_to_test->GetGlobalScopesForTesting()[i])
            ->global_scope());
  }

  String foo0 = R"JS(registerPaint('foo', class {
        static get inputProperties() { return ['--foo0']; }
        paint() {}
      });)JS";
  String foo1 = R"JS(registerPaint('foo', class {
        static get inputProperties() { return ['--foo1']; }
        paint() {}
      });)JS";
  String bar = R"JS(registerPaint('bar', class {
        static get inputProperties() { return ['--bar']; }
        paint() {}
      });)JS";

  ClassicScript::CreateUnspecifiedScript(foo0)->RunScriptOnScriptState(
      global_scopes[0]->ScriptController()->GetScriptState());

  EXPECT_TRUE(global_scopes[0]->FindDefinition("foo"));
  EXPECT_TRUE(paint_worklet_to_test->GetDocumentDefinitionMap().at("foo"));

  ClassicScript::CreateUnspecifiedScript(foo1)->RunScriptOnScriptState(
      global_scopes[1]->ScriptController()->GetScriptState());

  // foo0 and foo1 have the same name but different definitions, therefore
  // this definition must become invalid.
  EXPECT_FALSE(paint_worklet_to_test->GetDocumentDefinitionMap().at("foo"));

  ClassicScript::CreateUnspecifiedScript(bar)->RunScriptOnScriptState(
      global_scopes[0]->ScriptController()->GetScriptState());

  EXPECT_TRUE(global_scopes[0]->FindDefinition("bar"));
  EXPECT_TRUE(paint_worklet_to_test->GetDocumentDefinitionMap().at("bar"));

  // When running in main-thread mode, the generator is now ready after this
  // call. For off-thread, we are still waiting on the cross-thread
  // registration.
  if (!RuntimeEnabledFeatures::OffMainThreadCSSPaintEnabled())
    EXPECT_CALL(*observer, PaintImageGeneratorReady).Times(1);

  ClassicScript::CreateUnspecifiedScript(bar)->RunScriptOnScriptState(
      global_scopes[1]->ScriptController()->GetScriptState());

  EXPECT_TRUE(paint_worklet_to_test->GetDocumentDefinitionMap().at("bar"));
}

// TODO(crbug.com/1430318): All/MainOrOffThreadPaintWorkletTest.
// AllGlobalScopesMustBeCreated/1 is failing on Linux TSan Tests.
#if defined(THREAD_SANITIZER)
#define MAYBE_AllGlobalScopesMustBeCreated DISABLED_AllGlobalScopesMustBeCreated
#else
#define MAYBE_AllGlobalScopesMustBeCreated AllGlobalScopesMustBeCreated
#endif
TEST_P(MainOrOffThreadPaintWorkletTest, MAYBE_AllGlobalScopesMustBeCreated) {
  PaintWorklet* paint_worklet_to_test =
      MakeGarbageCollected<PaintWorklet>(*GetFrame().DomWindow());
  paint_worklet_to_test->ResetIsPaintOffThreadForTesting();

  EXPECT_TRUE(paint_worklet_to_test->GetGlobalScopesForTesting().empty());

  std::unique_ptr<PaintWorkletPaintDispatcher> dispatcher =
      std::make_unique<PaintWorkletPaintDispatcher>();
  Persistent<PaintWorkletProxyClient> proxy_client =
      MakeGarbageCollected<PaintWorkletProxyClient>(
          1, paint_worklet_to_test,
          GetFrame().GetTaskRunner(TaskType::kInternalDefault),
          dispatcher->GetWeakPtr(), nullptr);
  paint_worklet_to_test->SetProxyClientForTesting(proxy_client);

  while (paint_worklet_to_test->NeedsToCreateGlobalScopeForTesting()) {
    paint_worklet_to_test->AddGlobalScopeForTesting();
  }

  if (RuntimeEnabledFeatures::OffMainThreadCSSPaintEnabled()) {
    EXPECT_EQ(paint_worklet_to_test->GetGlobalScopesForTesting().size(),
              2 * PaintWorklet::kNumGlobalScopesPerThread);
  } else {
    EXPECT_EQ(paint_worklet_to_test->GetGlobalScopesForTesting().size(),
              PaintWorklet::kNumGlobalScopesPerThread);
  }
}

TEST_F(PaintWorkletTest, ConsistentGlobalScopeCrossThread) {
  ScopedOffMainThreadCSSPaintForTest off_main_thread_css_paint(true);
  PaintWorklet* paint_worklet_to_test =
      PaintWorklet::From(*GetFrame().GetDocument()->domWindow());
  paint_worklet_to_test->ResetIsPaintOffThreadForTesting();

  MockObserver* observer = MakeGarbageCollected<MockObserver>();
  CSSPaintImageGeneratorImpl* generator_foo =
      MakeGarbageCollected<CSSPaintImageGeneratorImpl>(
          observer, paint_worklet_to_test, "foo");
  paint_worklet_to_test->AddPendingGenerator("foo", generator_foo);
  CSSPaintImageGeneratorImpl* generator_bar =
      MakeGarbageCollected<CSSPaintImageGeneratorImpl>(
          observer, paint_worklet_to_test, "bar");
  paint_worklet_to_test->AddPendingGenerator("bar", generator_bar);
  CSSPaintImageGeneratorImpl* generator_loo =
      MakeGarbageCollected<CSSPaintImageGeneratorImpl>(
          observer, paint_worklet_to_test, "loo");
  paint_worklet_to_test->AddPendingGenerator("loo", generator_loo);
  CSSPaintImageGeneratorImpl* generator_gar =
      MakeGarbageCollected<CSSPaintImageGeneratorImpl>(
          observer, paint_worklet_to_test, "gar");
  paint_worklet_to_test->AddPendingGenerator("gar", generator_gar);

  // None of the situations covered in this test should cause the generator to
  // fire.
  EXPECT_CALL(*observer, PaintImageGeneratorReady).Times(0);

  Vector<Persistent<PaintWorkletGlobalScope>> global_scopes;
  for (wtf_size_t i = 0; i < PaintWorklet::kNumGlobalScopesPerThread; ++i) {
    paint_worklet_to_test->AddGlobalScopeForTesting();
    global_scopes.push_back(
        PaintWorkletGlobalScopeProxy::From(
            paint_worklet_to_test->GetGlobalScopesForTesting()[i])
            ->global_scope());
  }

  String foo0 = R"JS(registerPaint('foo', class {
        static get inputProperties() { return ['--foo0']; }
        paint() {}
      });)JS";
  String foo1 = R"JS(registerPaint('foo', class {
        static get inputProperties() { return ['--foo1']; }
        paint() {}
      });)JS";
  String bar0 = R"JS(registerPaint('bar', class {
        static get inputProperties() { return ['--bar0']; }
        paint() {}
      });)JS";
  String loo0 = R"JS(registerPaint('loo', class {
        static get inputProperties() { return ['--loo0']; }
        paint() {}
      });)JS";
  String loo1 = R"JS(registerPaint('loo', class {
        static get inputProperties() { return ['--loo1']; }
        paint() {}
      });)JS";
  String gar0 = R"JS(registerPaint('gar', class {
        static get inputProperties() { return ['--gar0']; }
        paint() {}
      });)JS";

  // Definition invalidated before cross thread check
  ClassicScript::CreateUnspecifiedScript(foo0)->RunScriptOnScriptState(
      global_scopes[0]->ScriptController()->GetScriptState());

  EXPECT_TRUE(global_scopes[0]->FindDefinition("foo"));
  EXPECT_TRUE(paint_worklet_to_test->GetDocumentDefinitionMap().at("foo"));

  ClassicScript::CreateUnspecifiedScript(foo1)->RunScriptOnScriptState(
      global_scopes[1]->ScriptController()->GetScriptState());

  EXPECT_FALSE(paint_worklet_to_test->GetDocumentDefinitionMap().at("foo"));

  CSSPaintDefinition* definition = global_scopes[0]->FindDefinition("foo");
  Vector<String> foo_custom_properties;
  for (const auto& s : definition->CustomInvalidationProperties()) {
    foo_custom_properties.push_back(s);
  }

  paint_worklet_to_test->RegisterMainThreadDocumentPaintDefinition(
      "foo", definition->NativeInvalidationProperties(), foo_custom_properties,
      definition->InputArgumentTypes(),
      definition->GetPaintRenderingContext2DSettings()->alpha());

  EXPECT_FALSE(paint_worklet_to_test->GetDocumentDefinitionMap().at("foo"));

  // Definition invalidated by cross thread check
  ClassicScript::CreateUnspecifiedScript(bar0)->RunScriptOnScriptState(
      global_scopes[0]->ScriptController()->GetScriptState());

  EXPECT_TRUE(global_scopes[0]->FindDefinition("bar"));
  EXPECT_TRUE(paint_worklet_to_test->GetDocumentDefinitionMap().at("bar"));

  ClassicScript::CreateUnspecifiedScript(bar0)->RunScriptOnScriptState(
      global_scopes[1]->ScriptController()->GetScriptState());

  EXPECT_TRUE(paint_worklet_to_test->GetDocumentDefinitionMap().at("bar"));

  definition = global_scopes[0]->FindDefinition("bar");

  // Manually change the custom properties
  Vector<String> bar_custom_properties({"--bar1"});

  paint_worklet_to_test->RegisterMainThreadDocumentPaintDefinition(
      "bar", definition->NativeInvalidationProperties(), bar_custom_properties,
      definition->InputArgumentTypes(),
      definition->GetPaintRenderingContext2DSettings()->alpha());

  // Although the main thread definitions were the same, the definition sent
  // cross thread differed from the main thread definitions so it must become
  // invalid.
  EXPECT_FALSE(paint_worklet_to_test->GetDocumentDefinitionMap().at("bar"));

  // Definition invalidated by second main thread call after cross thread check
  ClassicScript::CreateUnspecifiedScript(loo0)->RunScriptOnScriptState(
      global_scopes[0]->ScriptController()->GetScriptState());

  EXPECT_TRUE(global_scopes[0]->FindDefinition("loo"));
  EXPECT_TRUE(paint_worklet_to_test->GetDocumentDefinitionMap().at("loo"));

  definition = global_scopes[0]->FindDefinition("loo");
  Vector<String> loo_custom_properties;
  for (const auto& s : definition->CustomInvalidationProperties()) {
    loo_custom_properties.push_back(s);
  }

  paint_worklet_to_test->RegisterMainThreadDocumentPaintDefinition(
      "loo", definition->NativeInvalidationProperties(), loo_custom_properties,
      definition->InputArgumentTypes(),
      definition->GetPaintRenderingContext2DSettings()->alpha());

  EXPECT_TRUE(paint_worklet_to_test->GetDocumentDefinitionMap().at("loo"));

  ClassicScript::CreateUnspecifiedScript(loo1)->RunScriptOnScriptState(
      global_scopes[1]->ScriptController()->GetScriptState());

  // Although the first main thread call and the cross thread definition are the
  // same, the second main thread call differs so the definition must become
  // invalid
  EXPECT_FALSE(paint_worklet_to_test->GetDocumentDefinitionMap().at("loo"));

  // Definition invalidated by cross thread check before second main thread call
  ClassicScript::CreateUnspecifiedScript(gar0)->RunScriptOnScriptState(
      global_scopes[0]->ScriptController()->GetScriptState());

  EXPECT_TRUE(global_scopes[0]->FindDefinition("gar"));
  EXPECT_TRUE(paint_worklet_to_test->GetDocumentDefinitionMap().at("gar"));

  definition = global_scopes[0]->FindDefinition("gar");

  // Manually change custom properties
  Vector<String> gar_custom_properties({"--gar1"});

  paint_worklet_to_test->RegisterMainThreadDocumentPaintDefinition(
      "gar", definition->NativeInvalidationProperties(), gar_custom_properties,
      definition->InputArgumentTypes(),
      definition->GetPaintRenderingContext2DSettings()->alpha());

  EXPECT_FALSE(paint_worklet_to_test->GetDocumentDefinitionMap().at("gar"));

  ClassicScript::CreateUnspecifiedScript(gar0)->RunScriptOnScriptState(
      global_scopes[1]->ScriptController()->GetScriptState());

  // Although the main thread definitions were the same, the definition sent
  // cross thread differed from the main thread definitions so it must stay
  // invalid.
  EXPECT_FALSE(paint_worklet_to_test->GetDocumentDefinitionMap().at("gar"));
}

TEST_F(PaintWorkletTest, GeneratorNotifiedAfterAllRegistrations) {
  ScopedOffMainThreadCSSPaintForTest off_main_thread_css_paint(true);
  PaintWorklet* paint_worklet_to_test =
      PaintWorklet::From(*GetFrame().GetDocument()->domWindow());
  paint_worklet_to_test->ResetIsPaintOffThreadForTesting();

  MockObserver* observer = MakeGarbageCollected<MockObserver>();
  CSSPaintImageGeneratorImpl* generator =
      MakeGarbageCollected<CSSPaintImageGeneratorImpl>(
          observer, paint_worklet_to_test, "foo");
  paint_worklet_to_test->AddPendingGenerator("foo", generator);

  // The generator should not fire until the cross thread check
  EXPECT_CALL(*observer, PaintImageGeneratorReady).Times(0);

  Vector<Persistent<PaintWorkletGlobalScope>> global_scopes;
  for (wtf_size_t i = 0; i < PaintWorklet::kNumGlobalScopesPerThread; ++i) {
    paint_worklet_to_test->AddGlobalScopeForTesting();
    global_scopes.push_back(
        PaintWorkletGlobalScopeProxy::From(
            paint_worklet_to_test->GetGlobalScopesForTesting()[i])
            ->global_scope());
  }

  String foo = R"JS(registerPaint('foo', class {
        static get inputProperties() { return ['--foo']; }
        paint() {}
      });)JS";

  ClassicScript::CreateUnspecifiedScript(foo)->RunScriptOnScriptState(
      global_scopes[0]->ScriptController()->GetScriptState());

  EXPECT_TRUE(global_scopes[0]->FindDefinition("foo"));
  EXPECT_TRUE(paint_worklet_to_test->GetDocumentDefinitionMap().at("foo"));

  ClassicScript::CreateUnspecifiedScript(foo)->RunScriptOnScriptState(
      global_scopes[1]->ScriptController()->GetScriptState());

  EXPECT_TRUE(paint_worklet_to_test->GetDocumentDefinitionMap().at("foo"));

  CSSPaintDefinition* definition = global_scopes[0]->FindDefinition("foo");
  Vector<String> custom_properties;
  for (const auto& s : definition->CustomInvalidationProperties()) {
    custom_properties.push_back(s);
  }

  // The cross thread check should cause the generator to fire
  EXPECT_CALL(*observer, PaintImageGeneratorReady).Times(1);

  paint_worklet_to_test->RegisterMainThreadDocumentPaintDefinition(
      "foo", definition->NativeInvalidationProperties(), custom_properties,
      definition->InputArgumentTypes(),
      definition->GetPaintRenderingContext2DSettings()->alpha());

  EXPECT_TRUE(paint_worklet_to_test->GetDocumentDefinitionMap().at("foo"));
}

}  // namespace blink
