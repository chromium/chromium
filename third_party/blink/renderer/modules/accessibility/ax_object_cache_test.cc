// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"

#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_view_transition_callback.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/testing/mock_function_scope.h"
#include "third_party/blink/renderer/core/view_transition/dom_view_transition.h"
#include "third_party/blink/renderer/core/view_transition/view_transition.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_supplement.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_utils.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/blink/renderer/modules/accessibility/testing/accessibility_test.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

// TODO(nektar): Break test up into multiple tests.
TEST_F(AccessibilityTest, IsARIAWidget) {
  String test_content =
      "<body>"
      "<span id=\"plain\">plain</span><br>"
      "<span id=\"button\" role=\"button\">button</span><br>"
      "<span id=\"button-parent\" "
      "role=\"button\"><span>button-parent</span></span><br>"
      "<span id=\"button-caps\" role=\"BUTTON\">button-caps</span><br>"
      "<span id=\"button-second\" role=\"another-role "
      "button\">button-second</span><br>"
      "<span id=\"aria-bogus\" aria-bogus=\"bogus\">aria-bogus</span><br>"
      "<span id=\"aria-selected\" aria-selected>aria-selected</span><br>"
      "<span id=\"haspopup\" "
      "aria-haspopup=\"true\">aria-haspopup-true</span><br>"
      "<div id=\"focusable\" tabindex=\"1\">focusable</div><br>"
      "<div tabindex=\"2\"><div "
      "id=\"focusable-parent\">focusable-parent</div></div><br>"
      "</body>";

  SetBodyInnerHTML(test_content);
  Element* root(GetDocument().documentElement());
  EXPECT_FALSE(AXObjectCache::IsInsideFocusableElementOrARIAWidget(
      *root->getElementById(AtomicString("plain"))));
  EXPECT_TRUE(AXObjectCache::IsInsideFocusableElementOrARIAWidget(
      *root->getElementById(AtomicString("button"))));
  EXPECT_TRUE(AXObjectCache::IsInsideFocusableElementOrARIAWidget(
      *root->getElementById(AtomicString("button-parent"))));
  EXPECT_TRUE(AXObjectCache::IsInsideFocusableElementOrARIAWidget(
      *root->getElementById(AtomicString("button-caps"))));
  EXPECT_TRUE(AXObjectCache::IsInsideFocusableElementOrARIAWidget(
      *root->getElementById(AtomicString("button-second"))));
  EXPECT_FALSE(AXObjectCache::IsInsideFocusableElementOrARIAWidget(
      *root->getElementById(AtomicString("aria-bogus"))));
  EXPECT_TRUE(AXObjectCache::IsInsideFocusableElementOrARIAWidget(
      *root->getElementById(AtomicString("aria-selected"))));
  EXPECT_TRUE(AXObjectCache::IsInsideFocusableElementOrARIAWidget(
      *root->getElementById(AtomicString("haspopup"))));
  EXPECT_TRUE(AXObjectCache::IsInsideFocusableElementOrARIAWidget(
      *root->getElementById(AtomicString("focusable"))));
  EXPECT_TRUE(AXObjectCache::IsInsideFocusableElementOrARIAWidget(
      *root->getElementById(AtomicString("focusable-parent"))));
}

TEST_F(AccessibilityTest, HistogramTest) {
  SetBodyInnerHTML("<body><button>Press Me</button></body>");

  auto& cache = GetAXObjectCache();
  cache.SetAXMode(ui::kAXModeBasic);

  // No logs initially.
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "Accessibility.Performance.AXObjectCacheImpl.Snapshot", 0);
  histogram_tester.ExpectTotalCount(
      "Accessibility.Performance.AXObjectCacheImpl.Incremental", 0);
  histogram_tester.ExpectTotalCount(
      "Accessibility.Performance.AXObjectCacheImpl.Incremental.Float", 0);
  histogram_tester.ExpectTotalCount(
      "Accessibility.Performance.AXObjectCacheImpl.Incremental.Int", 0);
  histogram_tester.ExpectTotalCount(
      "Accessibility.Performance.AXObjectCacheImpl.Incremental.HTML", 0);
  histogram_tester.ExpectTotalCount(
      "Accessibility.Performance.AXObjectCacheImpl.Incremental.String", 0);

  {
    ui::AXTreeUpdate response;
    ScopedFreezeAXCache freeze(cache);
    cache.SerializeEntireTree(/* max_node_count */ 1000,
                              base::TimeDelta::FiniteMax(), &response);
    histogram_tester.ExpectTotalCount(
        "Accessibility.Performance.AXObjectCacheImpl.Snapshot", 1);
    histogram_tester.ExpectTotalCount(
        "Accessibility.Performance.AXObjectCacheImpl.Incremental", 0);
    histogram_tester.ExpectTotalCount(
        "Accessibility.Performance.AXObjectCacheImpl.Incremental.Float", 0);
    histogram_tester.ExpectTotalCount(
        "Accessibility.Performance.AXObjectCacheImpl.Incremental.Int", 0);
    histogram_tester.ExpectTotalCount(
        "Accessibility.Performance.AXObjectCacheImpl.Incremental.HTML", 0);
    histogram_tester.ExpectTotalCount(
        "Accessibility.Performance.AXObjectCacheImpl.Incremental.String", 0);
  }

  {
    std::vector<ui::AXTreeUpdate> updates;
    std::vector<ui::AXEvent> events;
    bool had_end_of_test_event = true;
    bool had_load_complete_messages = true;
    ScopedFreezeAXCache freeze(cache);
    cache.GetUpdatesAndEventsForSerialization(
        updates, events, had_end_of_test_event, had_load_complete_messages);
    histogram_tester.ExpectTotalCount(
        "Accessibility.Performance.AXObjectCacheImpl.Snapshot", 1);
    histogram_tester.ExpectTotalCount(
        "Accessibility.Performance.AXObjectCacheImpl.Incremental", 1);
    histogram_tester.ExpectTotalCount(
        "Accessibility.Performance.AXObjectCacheImpl.Incremental.Float", 1);
    histogram_tester.ExpectTotalCount(
        "Accessibility.Performance.AXObjectCacheImpl.Incremental.Int", 1);
    histogram_tester.ExpectTotalCount(
        "Accessibility.Performance.AXObjectCacheImpl.Incremental.HTML", 1);
    histogram_tester.ExpectTotalCount(
        "Accessibility.Performance.AXObjectCacheImpl.Incremental.String", 1);
  }
}

TEST_F(AccessibilityTest, RemoveReferencesToAXID) {
  auto& cache = GetAXObjectCache();
  SetBodyInnerHTML(R"HTML(
      <div id="f" style="position:fixed">aaa</div>
      <h2 id="h">Heading</h2>)HTML");
  AXObject* fixed = GetAXObjectByElementId("f");
  // GetBoundsInFrameCoordinates() updates fixed_or_sticky_node_ids_.
  fixed->GetBoundsInFrameCoordinates();
  EXPECT_EQ(1u, cache.fixed_or_sticky_node_ids_.size());

  // RemoveReferencesToAXID() on node that is not fixed or sticky should not
  // affect fixed_or_sticky_node_ids_.
  cache.RemoveReferencesToAXID(GetAXObjectByElementId("h")->AXObjectID());
  EXPECT_EQ(1u, cache.fixed_or_sticky_node_ids_.size());

  // RemoveReferencesToAXID() on node that fixed should affect
  // fixed_or_sticky_node_ids_.
  cache.RemoveReferencesToAXID(GetAXObjectByElementId("f")->AXObjectID());
  EXPECT_EQ(0u, cache.fixed_or_sticky_node_ids_.size());
}

class MockAXObject : public AXObject {
 public:
  explicit MockAXObject(AXObjectCacheImpl& ax_object_cache)
      : AXObject(ax_object_cache) {}
  static unsigned num_children_changed_calls_;

  void ChildrenChangedWithCleanLayout() final { num_children_changed_calls_++; }
  Document* GetDocument() const final { return &AXObjectCache().GetDocument(); }
  void AddChildren() final {}
  ax::mojom::blink::Role NativeRoleIgnoringAria() const override {
    return ax::mojom::blink::Role::kUnknown;
  }
};

unsigned MockAXObject::num_children_changed_calls_ = 0;

TEST_F(AccessibilityTest, PauseUpdatesAfterMaxNumberQueued) {
  auto& document = GetDocument();
  auto* ax_object_cache =
      To<AXObjectCacheImpl>(document.ExistingAXObjectCache());
  DCHECK(ax_object_cache);

  wtf_size_t max_updates = 10;
  ax_object_cache->SetMaxPendingUpdatesForTesting(max_updates);

  MockAXObject* ax_obj = MakeGarbageCollected<MockAXObject>(*ax_object_cache);
  ax_object_cache->AssociateAXID(ax_obj);
  for (unsigned i = 0; i < max_updates + 1; i++) {
    ax_object_cache->DeferTreeUpdate(
        AXObjectCacheImpl::TreeUpdateReason::kChildrenChanged, ax_obj);
  }
  ax_object_cache->ProcessCleanLayoutCallbacks(document);

  ASSERT_EQ(0u, MockAXObject::num_children_changed_calls_);
}

TEST_F(AccessibilityTest, UpdateAXForAllDocumentsAfterPausedUpdates) {
  auto& document = GetDocument();
  auto* ax_object_cache =
      To<AXObjectCacheImpl>(document.ExistingAXObjectCache());
  DCHECK(ax_object_cache);

  wtf_size_t max_updates = 1;
  ax_object_cache->SetMaxPendingUpdatesForTesting(max_updates);

  UpdateAllLifecyclePhasesForTest();
  AXObject* root = ax_object_cache->Root();
  // Queue one update too many.
  ax_object_cache->DeferTreeUpdate(
      AXObjectCacheImpl::TreeUpdateReason::kChildrenChanged, root);
  ax_object_cache->DeferTreeUpdate(
      AXObjectCacheImpl::TreeUpdateReason::kChildrenChanged, root);

  ax_object_cache->UpdateAXForAllDocuments();
  ScopedFreezeAXCache freeze(*ax_object_cache);
  CHECK(!root->NeedsToUpdateCachedValues());
}

class AXViewTransitionTest : public testing::Test {
 public:
  AXViewTransitionTest() {}

  void SetUp() override {
    web_view_helper_ = std::make_unique<frame_test_helpers::WebViewHelper>();
    web_view_helper_->Initialize();
    web_view_helper_->Resize(gfx::Size(200, 200));
  }

  void TearDown() override { web_view_helper_.reset(); }

  Document& GetDocument() {
    return *web_view_helper_->GetWebView()
                ->MainFrameImpl()
                ->GetFrame()
                ->GetDocument();
  }

  void UpdateAllLifecyclePhasesAndFinishDirectives() {
    UpdateAllLifecyclePhasesForTest();
    for (auto& callback :
         LayerTreeHost()->TakeViewTransitionCallbacksForTesting()) {
      std::move(callback).Run();
    }
  }

  cc::LayerTreeHost* LayerTreeHost() {
    return web_view_helper_->LocalMainFrame()
        ->FrameWidgetImpl()
        ->LayerTreeHostForTesting();
  }

  void SetHtmlInnerHTML(const String& content) {
    GetDocument().body()->setInnerHTML(content);
    UpdateAllLifecyclePhasesForTest();
  }

  void UpdateAllLifecyclePhasesForTest() {
    web_view_helper_->GetWebView()->MainFrameWidget()->UpdateAllLifecyclePhases(
        DocumentUpdateReason::kTest);
  }

  using State = ViewTransition::State;

  State GetState(DOMViewTransition* transition) const {
    return transition->GetViewTransitionForTest()->state_;
  }

 protected:
  test::TaskEnvironment task_environment_;
  std::unique_ptr<frame_test_helpers::WebViewHelper> web_view_helper_;
};

TEST_F(AXViewTransitionTest, TransitionPseudoNotRelevant) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      .shared {
        width: 100px;
        height: 100px;
        view-transition-name: shared;
        contain: layout;
        background: green;
      }
    </style>
    <div id=target class=shared></div>
  )HTML");

  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();

  MockFunctionScope funcs(script_state);
  auto* view_transition_callback =
      V8ViewTransitionCallback::Create(funcs.ExpectCall()->V8Function());

  auto* transition = ViewTransitionSupplement::startViewTransition(
      script_state, GetDocument(), view_transition_callback, exception_state);

  ScriptPromiseTester finish_tester(script_state,
                                    transition->finished(script_state));

  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(GetState(transition), State::kCapturing);

  UpdateAllLifecyclePhasesAndFinishDirectives();
  EXPECT_EQ(GetState(transition), State::kDOMCallbackRunning);

  // We should have a start request from the async callback passed to start()
  // resolving.
  test::RunPendingTasks();
  UpdateAllLifecyclePhasesAndFinishDirectives();

  // We should have a transition pseudo
  auto* transition_pseudo = GetDocument().documentElement()->GetPseudoElement(
      kPseudoIdViewTransition);
  ASSERT_TRUE(transition_pseudo);
  auto* container_pseudo = transition_pseudo->GetPseudoElement(
      kPseudoIdViewTransitionGroup, AtomicString("shared"));
  ASSERT_TRUE(container_pseudo);
  auto* image_wrapper_pseudo = container_pseudo->GetPseudoElement(
      kPseudoIdViewTransitionImagePair, AtomicString("shared"));
  ASSERT_TRUE(image_wrapper_pseudo);
  auto* incoming_image_pseudo = image_wrapper_pseudo->GetPseudoElement(
      kPseudoIdViewTransitionNew, AtomicString("shared"));
  ASSERT_TRUE(incoming_image_pseudo);
  auto* outgoing_image_pseudo = image_wrapper_pseudo->GetPseudoElement(
      kPseudoIdViewTransitionOld, AtomicString("shared"));
  ASSERT_TRUE(outgoing_image_pseudo);

  ASSERT_TRUE(transition_pseudo->GetLayoutObject());
  ASSERT_TRUE(container_pseudo->GetLayoutObject());
  ASSERT_TRUE(image_wrapper_pseudo->GetLayoutObject());
  ASSERT_TRUE(incoming_image_pseudo->GetLayoutObject());
  ASSERT_TRUE(outgoing_image_pseudo->GetLayoutObject());

  EXPECT_FALSE(AXObjectCacheImpl::IsRelevantPseudoElement(*transition_pseudo));
  EXPECT_FALSE(AXObjectCacheImpl::IsRelevantPseudoElement(*container_pseudo));
  EXPECT_FALSE(
      AXObjectCacheImpl::IsRelevantPseudoElement(*image_wrapper_pseudo));
  EXPECT_FALSE(
      AXObjectCacheImpl::IsRelevantPseudoElement(*incoming_image_pseudo));
  EXPECT_FALSE(
      AXObjectCacheImpl::IsRelevantPseudoElement(*outgoing_image_pseudo));
}

class AccessibilityEnabledLaterTest : public AccessibilityTest {
  USING_FAST_MALLOC(AccessibilityEnabledLaterTest);

 public:
  AccessibilityEnabledLaterTest(LocalFrameClient* local_frame_client = nullptr)
      : AccessibilityTest(local_frame_client) {}

  void SetUp() override { RenderingTest::SetUp(); }

  void EnableAccessibility() {
    ax_context_ =
        std::make_unique<AXContext>(GetDocument(), ui::kAXModeComplete);
  }
};

TEST_F(AccessibilityEnabledLaterTest, CSSAnchorPositioning) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      .anchor {
        anchor-name: --anchor-el;
       }
      .anchored-notice {
        position: absolute;
        position-anchor: --anchor-el;
        bottom: anchor(top);
        right: anchor(right);
      }
    </style>
    <body>
      <button id="1" class="anchor">
        <p>anchor</p>
      </button>
      <div id="2" class="anchored-notice">
        <p>positioned element tethered to the top-right of the anchor at bottom-right</p>
      </div>
    </body>
  )HTML");

  // Turning on a11y later should still set anchor relationships correctly.
  UpdateAllLifecyclePhasesForTest();
  DCHECK(!GetDocument().ExistingAXObjectCache());
  DCHECK(GetElementById("1")
             ->GetComputedStyle()
             ->AnchorName()
             ->GetNames()[0]
             ->GetName() == "--anchor-el");
  DCHECK(GetElementById("2")->GetComputedStyle()->PositionAnchor()->GetName() ==
         "--anchor-el");

  EnableAccessibility();
  AXObject* anchor = GetAXObjectByElementId("1");
  AXObject* positioned_object = GetAXObjectByElementId("2");
  EXPECT_EQ(GetAXObjectCache().GetPositionedObjectForAnchor(anchor),
            positioned_object);
  EXPECT_EQ(GetAXObjectCache().GetAnchorForPositionedObject(positioned_object),
            anchor);
}

}  // namespace blink
