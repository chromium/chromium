// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/display_lock/display_lock_context.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/test/scoped_feature_list.h"
#include "cc/base/features.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_document_state.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/finder/text_finder.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/frame/find_in_page.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/html/html_plugin_element.h"
#include "third_party/blink/renderer/core/html/html_template_element.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {
namespace {
class DisplayLockTestFindInPageClient : public mojom::blink::FindInPageClient {
 public:
  DisplayLockTestFindInPageClient()
      : find_results_are_ready_(false), active_index_(-1), count_(-1) {}

  ~DisplayLockTestFindInPageClient() override = default;

  void SetFrame(WebLocalFrameImpl* frame) {
    frame->GetFindInPage()->SetClient(receiver_.BindNewPipeAndPassRemote());
  }

  void SetNumberOfMatches(
      int request_id,
      unsigned int current_number_of_matches,
      mojom::blink::FindMatchUpdateType final_update) final {
    count_ = current_number_of_matches;
    find_results_are_ready_ =
        (final_update == mojom::blink::FindMatchUpdateType::kFinalUpdate);
  }

  void SetActiveMatch(int request_id,
                      const gfx::Rect& active_match_rect,
                      int active_match_ordinal,
                      mojom::blink::FindMatchUpdateType final_update) final {
    active_match_rect_ = active_match_rect;
    active_index_ = active_match_ordinal;
    find_results_are_ready_ =
        (final_update == mojom::blink::FindMatchUpdateType::kFinalUpdate);
  }

  bool FindResultsAreReady() const { return find_results_are_ready_; }
  int Count() const { return count_; }
  int ActiveIndex() const { return active_index_; }
  gfx::Rect ActiveMatchRect() const { return active_match_rect_; }

  void Reset() {
    find_results_are_ready_ = false;
    count_ = -1;
    active_index_ = -1;
    active_match_rect_ = gfx::Rect();
  }

 private:
  gfx::Rect active_match_rect_;
  bool find_results_are_ready_;
  int active_index_;

  int count_;
  mojo::Receiver<mojom::blink::FindInPageClient> receiver_{this};
};

class DisplayLockEmptyEventListener final : public NativeEventListener {
 public:
  void Invoke(ExecutionContext*, Event*) final {}
};
}  // namespace

class DisplayLockContextTest : public testing::Test,
                               public testing::WithParamInterface<bool>,
                               private ScopedIntersectionOptimizationForTest {
 public:
  DisplayLockContextTest()
      : ScopedIntersectionOptimizationForTest(GetParam()) {}

  void SetUp() override { web_view_helper_.Initialize(); }

  void TearDown() override { web_view_helper_.Reset(); }

  Document& GetDocument() {
    return *static_cast<Document*>(
        web_view_helper_.LocalMainFrame()->GetDocument());
  }
  FindInPage* GetFindInPage() {
    return web_view_helper_.LocalMainFrame()->GetFindInPage();
  }
  WebLocalFrameImpl* LocalMainFrame() {
    return web_view_helper_.LocalMainFrame();
  }

  FrameSelection& Selection() {
    return LocalMainFrame()->GetFrame()->Selection();
  }

  void UpdateAllLifecyclePhasesForTest() {
    GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  }

  void SetHtmlInnerHTML(const char* content) {
    GetDocument().documentElement()->setInnerHTML(String::FromUTF8(content));
    UpdateAllLifecyclePhasesForTest();
  }

  void ResizeAndFocus() {
    web_view_helper_.Resize(gfx::Size(640, 480));
    web_view_helper_.GetWebView()->MainFrameWidget()->SetFocus(true);
    test::RunPendingTasks();
  }

  void LockElement(Element& element, bool activatable) {
    if (activatable) {
      element.setAttribute(html_names::kHiddenAttr,
                           AtomicString("until-found"));
    } else {
      element.setAttribute(html_names::kStyleAttr,
                           AtomicString("content-visibility: hidden"));
    }
    UpdateAllLifecyclePhasesForTest();
  }

  void CommitElement(Element& element, bool update_lifecycle = true) {
    element.setAttribute(html_names::kStyleAttr, g_empty_atom);
    if (update_lifecycle)
      UpdateAllLifecyclePhasesForTest();
  }

  void UnlockImmediate(DisplayLockContext* context) {
    context->SetRequestedState(EContentVisibility::kVisible);
  }

  mojom::blink::FindOptionsPtr FindOptions(bool new_session = true) {
    auto find_options = mojom::blink::FindOptions::New();
    find_options->run_synchronously_for_testing = true;
    find_options->new_session = new_session;
    find_options->forward = true;
    return find_options;
  }

  void Find(String search_text,
            DisplayLockTestFindInPageClient& client,
            bool new_session = true) {
    client.Reset();
    GetFindInPage()->Find(FAKE_FIND_ID, search_text, FindOptions(new_session));
    test::RunPendingTasks();
  }

  bool ReattachWasBlocked(DisplayLockContext* context) {
    return context->blocked_child_recalc_change_.ReattachLayoutTree();
  }

  bool HasSelection(DisplayLockContext* context) {
    return context->render_affecting_state_[static_cast<int>(
        DisplayLockContext::RenderAffectingState::kSubtreeHasSelection)];
  }
  DisplayLockUtilities::ScopedForcedUpdate GetScopedForcedUpdate(
      const Node* node,
      DisplayLockContext::ForcedPhase phase,
      bool include_self = false) {
    return DisplayLockUtilities::ScopedForcedUpdate(node, phase, include_self);
  }

  const int FAKE_FIND_ID = 1;

 private:
  test::TaskEnvironment task_environment;

  frame_test_helpers::WebViewHelper web_view_helper_;
};

INSTANTIATE_TEST_SUITE_P(All, DisplayLockContextTest, testing::Bool());

TEST_P(DisplayLockContextTest, LockAfterAppendStyleDirtyBits) {
  SetHtmlInnerHTML(R"HTML(
    <style>
    div {
      width: 100px;
      height: 100px;
      contain: style layout paint;
    }
    </style>
    <body><div id="container"><div id="child"></div></div></body>
  )HTML");

  auto* element = GetDocument().getElementById(AtomicString("container"));
  LockElement(*element, false);

  // Finished acquiring the lock.
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldStyleChildren());
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldLayoutChildren());
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldPaintChildren());
  EXPECT_EQ(
      GetDocument().GetDisplayLockDocumentState().LockedDisplayLockCount(), 1);

  // If the element is dirty, style recalc would handle it in the next recalc.
  element->setAttribute(
      html_names::kStyleAttr,
      AtomicString("content-visibility: hidden; color: red;"));
  EXPECT_TRUE(GetDocument().body()->ChildNeedsStyleRecalc());
  EXPECT_TRUE(element->NeedsStyleRecalc());
  EXPECT_FALSE(element->ChildNeedsStyleRecalc());

  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(element->NeedsStyleRecalc());
  EXPECT_FALSE(element->ChildNeedsStyleRecalc());
  EXPECT_TRUE(element->GetComputedStyle());
  EXPECT_EQ(
      element->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()),
      Color::FromRGB(255, 0, 0));
  // Manually commit the lock so that we can verify which dirty bits get
  // propagated.
  UnlockImmediate(element->GetDisplayLockContext());
  element->setAttribute(html_names::kStyleAttr, AtomicString("color: red;"));

  auto* child = GetDocument().getElementById(AtomicString("child"));
  EXPECT_TRUE(GetDocument().body()->ChildNeedsStyleRecalc());
  EXPECT_TRUE(element->NeedsStyleRecalc());
  EXPECT_FALSE(element->ChildNeedsStyleRecalc());
  EXPECT_FALSE(child->NeedsStyleRecalc());
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(GetDocument().body()->ChildNeedsStyleRecalc());
  EXPECT_FALSE(element->NeedsStyleRecalc());
  EXPECT_FALSE(element->ChildNeedsStyleRecalc());
  EXPECT_FALSE(child->NeedsStyleRecalc());

  // Lock the child.
  child->setAttribute(html_names::kStyleAttr,
                      AtomicString("content-visibility: hidden; color: blue;"));
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(GetDocument().body()->ChildNeedsStyleRecalc());
  EXPECT_FALSE(element->NeedsStyleRecalc());
  EXPECT_FALSE(element->ChildNeedsStyleRecalc());
  EXPECT_FALSE(child->NeedsStyleRecalc());
  ASSERT_TRUE(child->GetComputedStyle());
  EXPECT_EQ(
      child->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()),
      Color::FromRGB(0, 0, 255));

  UnlockImmediate(child->GetDisplayLockContext());
  child->setAttribute(html_names::kStyleAttr, AtomicString("color: blue;"));
  EXPECT_TRUE(GetDocument().body()->ChildNeedsStyleRecalc());
  EXPECT_FALSE(element->NeedsStyleRecalc());
  EXPECT_TRUE(element->ChildNeedsStyleRecalc());
  EXPECT_TRUE(child->NeedsStyleRecalc());
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(GetDocument().body()->ChildNeedsStyleRecalc());
  EXPECT_FALSE(element->NeedsStyleRecalc());
  EXPECT_FALSE(element->ChildNeedsStyleRecalc());
  EXPECT_FALSE(child->NeedsStyleRecalc());
  ASSERT_TRUE(child->GetComputedStyle());
  EXPECT_EQ(
      child->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()),
      Color::FromRGB(0, 0, 255));
}

TEST_P(DisplayLockContextTest, LockedElementIsNotSearchableViaFindInPage) {
  ResizeAndFocus();
  SetHtmlInnerHTML(R"HTML(
    <style>
    #container {
      width: 100px;
      height: 100px;
      contain: style layout paint;
    }
    </style>
    <body><div id="container">testing</div></body>
  )HTML");

  const String search_text = "testing";
  DisplayLockTestFindInPageClient client;
  client.SetFrame(LocalMainFrame());

  auto* container = GetDocument().getElementById(AtomicString("container"));
  LockElement(*container, false /* activatable */);
  Find(search_text, client);
  EXPECT_EQ(0, client.Count());

  // Check if we can find the result after we commit.
  CommitElement(*container);
  Find(search_text, client);
  EXPECT_EQ(1, client.Count());
}

TEST_P(DisplayLockContextTest,
       ActivatableLockedElementIsSearchableViaFindInPage) {
  ResizeAndFocus();
  SetHtmlInnerHTML(R"HTML(
    <style>
    .spacer {
      height: 10000px;
    }
    #container {
      width: 100px;
      height: 100px;
      contain: style layout paint;
    }
    </style>
    <body><div class=spacer></div><div id="container">testing</div></body>
  )HTML");

  const String search_text = "testing";
  DisplayLockTestFindInPageClient client;
  client.SetFrame(LocalMainFrame());

  // Finds on a normal element.
  Find(search_text, client);
  EXPECT_EQ(1, client.Count());
  // Clears selections since we're going to use the same query next time.
  GetFindInPage()->StopFinding(
      mojom::StopFindAction::kStopFindActionClearSelection);

  auto* container = GetDocument().getElementById(AtomicString("container"));
  LockElement(*container, true /* activatable */);

  EXPECT_TRUE(container->GetDisplayLockContext()->IsLocked());
  // Check if we can still get the same result with the same query.
  Find(search_text, client);
  EXPECT_EQ(1, client.Count());
  EXPECT_FALSE(container->GetDisplayLockContext()->IsLocked());
  EXPECT_GT(GetDocument().scrollingElement()->scrollTop(), 1000);
}

TEST_P(DisplayLockContextTest, FindInPageContinuesAfterRelock) {
  ResizeAndFocus();
  SetHtmlInnerHTML(R"HTML(
    <style>
    .spacer {
      height: 10000px;
    }
    #container {
      width: 100px;
      height: 100px;
    }
    .auto { content-visibility: auto }
    </style>
    <body><div class=spacer></div><div id="container" class=auto>testing</div></body>
  )HTML");

  const String search_text = "testing";
  DisplayLockTestFindInPageClient client;
  client.SetFrame(LocalMainFrame());

  // Finds on a normal element.
  Find(search_text, client);
  EXPECT_EQ(1, client.Count());

  auto* container = GetDocument().getElementById(AtomicString("container"));
  GetDocument().scrollingElement()->setScrollTop(0);

  UpdateAllLifecyclePhasesForTest();
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(container->GetDisplayLockContext()->IsLocked());

  // Clears selections since we're going to use the same query next time.
  GetFindInPage()->StopFinding(
      mojom::StopFindAction::kStopFindActionKeepSelection);

  UpdateAllLifecyclePhasesForTest();

  // This should not crash.
  Find(search_text, client, false);

  EXPECT_EQ(1, client.Count());
}

TEST_P(DisplayLockContextTest, FindInPageTargetBelowLockedSize) {
  ResizeAndFocus();
  SetHtmlInnerHTML(R"HTML(
    <style>
    .spacer { height: 1000px; }
    #container { contain-intrinsic-size: 1px; }
    .auto { content-visibility: auto }
    </style>
    <body>
      <div class=spacer></div>
      <div id=container class=auto>
        <div class=spacer></div>
        <div id=target>testing</div>
      </div>
      <div class=spacer></div>
      <div class=spacer></div>
    </body>
  )HTML");

  const String search_text = "testing";
  DisplayLockTestFindInPageClient client;
  client.SetFrame(LocalMainFrame());

  Find(search_text, client);
  EXPECT_EQ(1, client.Count());

  auto* container = GetDocument().getElementById(AtomicString("container"));
  // The container should be unlocked.
  EXPECT_FALSE(container->GetDisplayLockContext()->IsLocked());
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(container->GetDisplayLockContext()->IsLocked());

  if (RuntimeEnabledFeatures::FractionalScrollOffsetsEnabled())
    EXPECT_FLOAT_EQ(GetDocument().scrollingElement()->scrollTop(), 1768.5);
  else
    EXPECT_FLOAT_EQ(GetDocument().scrollingElement()->scrollTop(), 1768);
}

TEST_P(DisplayLockContextTest,
       ActivatableLockedElementTickmarksAreAtLockedRoots) {
  ResizeAndFocus();
  SetHtmlInnerHTML(R"HTML(
    <style>
    body {
      margin: 0;
      padding: 0;
    }
    .small {
      width: 100px;
      height: 100px;
    }
    .medium {
      width: 150px;
      height: 150px;
    }
    .large {
      width: 200px;
      height: 200px;
    }
    </style>
    <body>
      testing
      <div id="container1" class=small>testing</div>
      <div id="container2" class=medium>testing</div>
      <div id="container3" class=large>
        <div id="container4" class=medium>testing</div>
      </div>
      <div id="container5" class=small>testing</div>
    </body>
  )HTML");

  const String search_text = "testing";
  DisplayLockTestFindInPageClient client;
  client.SetFrame(LocalMainFrame());

  auto* container1 = GetDocument().getElementById(AtomicString("container1"));
  auto* container2 = GetDocument().getElementById(AtomicString("container2"));
  auto* container3 = GetDocument().getElementById(AtomicString("container3"));
  auto* container4 = GetDocument().getElementById(AtomicString("container4"));
  auto* container5 = GetDocument().getElementById(AtomicString("container5"));
  LockElement(*container5, false /* activatable */);
  LockElement(*container4, true /* activatable */);
  LockElement(*container3, true /* activatable */);
  LockElement(*container2, true /* activatable */);
  LockElement(*container1, true /* activatable */);

  EXPECT_TRUE(container1->GetDisplayLockContext()->IsLocked());
  EXPECT_TRUE(container2->GetDisplayLockContext()->IsLocked());
  EXPECT_TRUE(container3->GetDisplayLockContext()->IsLocked());
  EXPECT_TRUE(container4->GetDisplayLockContext()->IsLocked());
  EXPECT_TRUE(container5->GetDisplayLockContext()->IsLocked());

  // Do a find-in-page.
  Find(search_text, client);
  // "testing" outside of the container divs, and 3 inside activatable divs.
  EXPECT_EQ(4, client.Count());

  auto tick_rects = GetDocument().Markers().LayoutRectsForTextMatchMarkers();
  ASSERT_EQ(4u, tick_rects.size());

  // Sort the layout rects by y coordinate for deterministic checks below.
  std::sort(
      tick_rects.begin(), tick_rects.end(),
      [](const gfx::Rect& a, const gfx::Rect& b) { return a.y() < b.y(); });

  int y_offset = tick_rects[0].height();

  // The first tick rect will be based on the text itself, so we don't need to
  // check that. The next three should be the small, medium and large rects,
  // since those are the locked roots.
  EXPECT_EQ(gfx::Rect(0, y_offset, 100, 100), tick_rects[1]);
  y_offset += tick_rects[1].height();
  EXPECT_EQ(gfx::Rect(0, y_offset, 150, 150), tick_rects[2]);
  y_offset += tick_rects[2].height();
  EXPECT_EQ(gfx::Rect(0, y_offset, 200, 200), tick_rects[3]);
}

TEST_P(DisplayLockContextTest,
       FindInPageWhileLockedContentChangesDoesNotCrash) {
  ResizeAndFocus();
  SetHtmlInnerHTML(R"HTML(
    <style>
    #container {
      width: 100px;
      height: 100px;
      contain: style layout paint;
    }
    </style>
    <body>testing<div id="container">testing</div></body>
  )HTML");

  const String search_text = "testing";
  DisplayLockTestFindInPageClient client;
  client.SetFrame(LocalMainFrame());

  // Lock the container.
  auto* container = GetDocument().getElementById(AtomicString("container"));
  LockElement(*container, true /* activatable */);
  EXPECT_TRUE(container->GetDisplayLockContext()->IsLocked());

  // Find the first "testing", container still locked since the match is outside
  // the container.
  Find(search_text, client);
  EXPECT_EQ(2, client.Count());
  EXPECT_TRUE(container->GetDisplayLockContext()->IsLocked());

  // Change the inner text, this should not DCHECK.
  container->setInnerHTML("please don't DCHECK");
  UpdateAllLifecyclePhasesForTest();
}

TEST_P(DisplayLockContextTest, FindInPageWithChangedContent) {
  ResizeAndFocus();
  SetHtmlInnerHTML(R"HTML(
    <style>
    #container {
      width: 100px;
      height: 100px;
      contain: style layout paint;
    }
    </style>
    <body><div id="container">testing</div></body>
  )HTML");

  // Check if the result is correct if we update the contents.
  auto* container = GetDocument().getElementById(AtomicString("container"));
  LockElement(*container, true /* activatable */);
  EXPECT_TRUE(container->GetDisplayLockContext()->IsLocked());
  container->setInnerHTML(
      "testing"
      "<div>testing</div>"
      "tes<div style='display:none;'>x</div>ting");

  DisplayLockTestFindInPageClient client;
  client.SetFrame(LocalMainFrame());
  Find("testing", client);
  EXPECT_EQ(3, client.Count());
  EXPECT_FALSE(container->GetDisplayLockContext()->IsLocked());
}

TEST_P(DisplayLockContextTest, FindInPageWithNoMatchesWontUnlock) {
  ResizeAndFocus();
  SetHtmlInnerHTML(R"HTML(
    <style>
    #container {
      width: 100px;
      height: 100px;
      contain: style layout paint;
    }
    </style>
    <body><div id="container">tes<div>ting</div><div style='display:none;'>testing</div></div></body>
  )HTML");

  auto* container = GetDocument().getElementById(AtomicString("container"));
  LockElement(*container, true /* activatable */);
  LockElement(*container, true /* activatable */);
  EXPECT_TRUE(container->GetDisplayLockContext()->IsLocked());

  DisplayLockTestFindInPageClient client;
  client.SetFrame(LocalMainFrame());
  Find("testing", client);
  // No results found, container stays locked.
  EXPECT_EQ(0, client.Count());
  EXPECT_TRUE(container->GetDisplayLockContext()->IsLocked());
}

TEST_P(DisplayLockContextTest,
       NestedActivatableLockedElementIsSearchableViaFindInPage) {
  ResizeAndFocus();
  SetHtmlInnerHTML(R"HTML(
    <body>
      <style>
        div {
          width: 100px;
          height: 100px;
          contain: style layout;
        }
      </style>
      <div id='container'>
        <div>testing1</div>
        <div id='activatable'>
        testing2
          <div id='nestedNonActivatable'>
            testing3
          </div>
        </div>
        <div id='nonActivatable'>testing4</div>
      </div>
    "</body>"
  )HTML");

  auto* container = GetDocument().getElementById(AtomicString("container"));
  auto* activatable = GetDocument().getElementById(AtomicString("activatable"));
  auto* non_activatable =
      GetDocument().getElementById(AtomicString("nonActivatable"));
  auto* nested_non_activatable =
      GetDocument().getElementById(AtomicString("nestedNonActivatable"));

  LockElement(*non_activatable, false /* activatable */);
  LockElement(*nested_non_activatable, false /* activatable */);
  LockElement(*activatable, true /* activatable */);
  LockElement(*container, true /* activatable */);

  EXPECT_TRUE(container->GetDisplayLockContext()->IsLocked());
  EXPECT_TRUE(activatable->GetDisplayLockContext()->IsLocked());
  EXPECT_TRUE(non_activatable->GetDisplayLockContext()->IsLocked());
  EXPECT_TRUE(nested_non_activatable->GetDisplayLockContext()->IsLocked());

  // We can find testing1 and testing2.
  DisplayLockTestFindInPageClient client;
  client.SetFrame(LocalMainFrame());
  Find("testing", client);
  EXPECT_EQ(2, client.Count());
  EXPECT_EQ(1, client.ActiveIndex());

  EXPECT_FALSE(container->GetDisplayLockContext()->IsLocked());
  EXPECT_TRUE(activatable->GetDisplayLockContext()->IsLocked());
  EXPECT_TRUE(non_activatable->GetDisplayLockContext()->IsLocked());
  EXPECT_TRUE(nested_non_activatable->GetDisplayLockContext()->IsLocked());
}

TEST_P(DisplayLockContextTest,
       NestedActivatableLockedElementIsNotUnlockedByFindInPage) {
  ResizeAndFocus();
  SetHtmlInnerHTML(R"HTML(
    <body>
      <style>
        div {
          width: 100px;
          height: 100px;
          contain: style layout;
        }
      </style>
      <div id='container'>
        <div id='child'>testing1</div>
      </div>
  )HTML");
  auto* container = GetDocument().getElementById(AtomicString("container"));
  auto* child = GetDocument().getElementById(AtomicString("child"));
  LockElement(*child, true /* activatable */);
  LockElement(*container, true /* activatable */);

  EXPECT_TRUE(container->GetDisplayLockContext()->IsLocked());
  EXPECT_TRUE(child->GetDisplayLockContext()->IsLocked());
  // We can find testing1 and testing2.
  DisplayLockTestFindInPageClient client;
  client.SetFrame(LocalMainFrame());
  Find("testing", client);
  EXPECT_EQ(1, client.Count());
  EXPECT_EQ(1, client.ActiveIndex());

  EXPECT_FALSE(container->GetDisplayLockContext()->IsLocked());
  EXPECT_FALSE(child->GetDisplayLockContext()->IsLocked());
}

TEST_P(DisplayLockContextTest, CallUpdateStyleAndLayoutAfterChange) {
  ResizeAndFocus();
  SetHtmlInnerHTML(R"HTML(
    <style>
    #container {
      width: 100px;
      height: 100px;
      contain: style layout paint;
    }
    </style>
    <body><div id="container"><b>t</b>esting</div></body>
  )HTML");
  auto* element = GetDocument().getElementById(AtomicString("container"));
  LockElement(*element, false);

  // Sanity checks to ensure the element is locked.
  EXPECT_TRUE(element->GetDisplayLockContext()->IsLocked());
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldStyleChildren());
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldLayoutChildren());
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldPaintChildren());
  EXPECT_EQ(
      GetDocument().GetDisplayLockDocumentState().LockedDisplayLockCount(), 1);
  EXPECT_EQ(GetDocument()
                .GetDisplayLockDocumentState()
                .DisplayLockBlockingAllActivationCount(),
            1);

  EXPECT_FALSE(element->NeedsStyleRecalc());
  EXPECT_FALSE(element->ChildNeedsStyleRecalc());
  EXPECT_FALSE(element->NeedsReattachLayoutTree());
  EXPECT_FALSE(element->ChildNeedsReattachLayoutTree());

  // Testing whitespace reattachment, shouldn't mark for reattachment.
  element->firstChild()->remove();

  EXPECT_FALSE(element->NeedsStyleRecalc());
  EXPECT_FALSE(element->ChildNeedsStyleRecalc());
  EXPECT_FALSE(element->NeedsReattachLayoutTree());
  EXPECT_FALSE(element->ChildNeedsReattachLayoutTree());

  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  EXPECT_FALSE(element->NeedsStyleRecalc());
  EXPECT_FALSE(element->ChildNeedsStyleRecalc());
  EXPECT_FALSE(element->NeedsReattachLayoutTree());
  EXPECT_FALSE(element->ChildNeedsReattachLayoutTree());

  // Testing whitespace reattachment + dirty style.
  element->setInnerHTML("<div>something</div>");

  EXPECT_FALSE(element->NeedsStyleRecalc());
  EXPECT_TRUE(element->ChildNeedsStyleRecalc());
  EXPECT_FALSE(element->NeedsReattachLayoutTree());
  EXPECT_FALSE(element->ChildNeedsReattachLayoutTree());

  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  EXPECT_FALSE(element->NeedsStyleRecalc());
  EXPECT_TRUE(element->ChildNeedsStyleRecalc());
  EXPECT_FALSE(element->NeedsReattachLayoutTree());
  EXPECT_FALSE(element->ChildNeedsReattachLayoutTree());

  // Manually start commit, so that we can verify which dirty bits get
  // propagated.
  CommitElement(*element, false);
  EXPECT_TRUE(element->NeedsStyleRecalc());
  EXPECT_TRUE(element->ChildNeedsStyleRecalc());
  EXPECT_TRUE(GetDocument().body()->ChildNeedsStyleRecalc());
  EXPECT_FALSE(element->NeedsReattachLayoutTree());
  EXPECT_FALSE(element->ChildNeedsReattachLayoutTree());

  // Simulating style recalc happening, will mark for reattachment.
  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);
  GetDocument().GetStyleEngine().RecalcStyle();
  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kStyleClean);

  EXPECT_FALSE(element->ChildNeedsStyleRecalc());
  EXPECT_FALSE(element->NeedsReattachLayoutTree());
  EXPECT_TRUE(element->ChildNeedsReattachLayoutTree());
}

TEST_P(DisplayLockContextTest, CallUpdateStyleAndLayoutAfterChangeCSS) {
  ResizeAndFocus();
  SetHtmlInnerHTML(R"HTML(
    <style>
    #container {
      width: 100px;
      height: 100px;
      contain: style layout paint;
    }
    .bg {
      background: blue;
    }
    .locked {
      content-visibility: hidden;
    }
    </style>
    <body><div class=locked id="container"><b>t</b>esting<div id=inner></div></div></body>
  )HTML");
  auto* element = GetDocument().getElementById(AtomicString("container"));
  auto* inner = GetDocument().getElementById(AtomicString("inner"));

  // Sanity checks to ensure the element is locked.
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldStyleChildren());
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldLayoutChildren());
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldPaintChildren());
  EXPECT_EQ(
      GetDocument().GetDisplayLockDocumentState().LockedDisplayLockCount(), 1);
  EXPECT_EQ(GetDocument()
                .GetDisplayLockDocumentState()
                .DisplayLockBlockingAllActivationCount(),
            1);

  EXPECT_TRUE(ReattachWasBlocked(element->GetDisplayLockContext()));
  // Note that we didn't create a layout object for inner, since the layout tree
  // attachment was blocked.
  EXPECT_FALSE(inner->GetLayoutObject());

  EXPECT_FALSE(element->NeedsStyleRecalc());
  EXPECT_FALSE(element->ChildNeedsStyleRecalc());
  EXPECT_FALSE(element->NeedsReattachLayoutTree());
  EXPECT_FALSE(element->ChildNeedsReattachLayoutTree());

  element->classList().Remove(AtomicString("locked"));

  // Class list changed, so we should need self style change.
  EXPECT_TRUE(element->NeedsStyleRecalc());
  EXPECT_FALSE(element->ChildNeedsStyleRecalc());
  EXPECT_FALSE(element->NeedsReattachLayoutTree());
  EXPECT_FALSE(element->ChildNeedsReattachLayoutTree());

  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(element->NeedsStyleRecalc());
  EXPECT_FALSE(element->ChildNeedsStyleRecalc());
  EXPECT_FALSE(element->NeedsReattachLayoutTree());
  EXPECT_FALSE(element->ChildNeedsReattachLayoutTree());
  // Because we upgraded our style change, we created a layout object for inner.
  EXPECT_TRUE(inner->GetLayoutObject());
}

TEST_P(DisplayLockContextTest, LockedElementAndDescendantsAreNotFocusable) {
  ResizeAndFocus();
  SetHtmlInnerHTML(R"HTML(
    <style>
    #container {
      width: 100px;
      height: 100px;
      contain: style layout paint;
    }
    </style>
    <body>
    <div id="container">
      <input id="textfield", type="text">
    </div>
    </body>
  )HTML");

  // We start off as being focusable.
  ASSERT_TRUE(GetDocument()
                  .getElementById(AtomicString("textfield"))
                  ->IsKeyboardFocusable());
  ASSERT_TRUE(
      GetDocument().getElementById(AtomicString("textfield"))->IsFocusable());
  ASSERT_TRUE(
      GetDocument().getElementById(AtomicString("textfield"))->IsFocusable());
  EXPECT_EQ(
      GetDocument().GetDisplayLockDocumentState().LockedDisplayLockCount(), 0);
  EXPECT_EQ(GetDocument()
                .GetDisplayLockDocumentState()
                .DisplayLockBlockingAllActivationCount(),
            0);

  auto* element = GetDocument().getElementById(AtomicString("container"));
  LockElement(*element, false);

  // Sanity checks to ensure the element is locked.
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldStyleChildren());
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldLayoutChildren());
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldPaintChildren());
  EXPECT_EQ(
      GetDocument().GetDisplayLockDocumentState().LockedDisplayLockCount(), 1);
  EXPECT_EQ(GetDocument()
                .GetDisplayLockDocumentState()
                .DisplayLockBlockingAllActivationCount(),
            1);

  // The input should not be focusable now.
  EXPECT_FALSE(GetDocument()
                   .getElementById(AtomicString("textfield"))
                   ->IsKeyboardFocusable());
  EXPECT_FALSE(
      GetDocument().getElementById(AtomicString("textfield"))->IsFocusable());
  EXPECT_FALSE(
      GetDocument().getElementById(AtomicString("textfield"))->IsFocusable());

  // Calling explicit focus() should also not focus the element.
  GetDocument().getElementById(AtomicString("textfield"))->Focus();
  EXPECT_FALSE(GetDocument().FocusedElement());

  // Now commit the lock and ensure we can focus the input
  CommitElement(*element);

  EXPECT_TRUE(element->GetDisplayLockContext()->ShouldStyleChildren());
  EXPECT_TRUE(element->GetDisplayLockContext()->ShouldLayoutChildren());
  EXPECT_TRUE(element->GetDisplayLockContext()->ShouldPaintChildren());

  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(
      GetDocument().GetDisplayLockDocumentState().LockedDisplayLockCount(), 0);
  EXPECT_EQ(GetDocument()
                .GetDisplayLockDocumentState()
                .DisplayLockBlockingAllActivationCount(),
            0);
  EXPECT_TRUE(GetDocument()
                  .getElementById(AtomicString("textfield"))
                  ->IsKeyboardFocusable());
  EXPECT_TRUE(
      GetDocument().getElementById(AtomicString("textfield"))->IsFocusable());
  EXPECT_TRUE(
      GetDocument().getElementById(AtomicString("textfield"))->IsFocusable());

  // Calling explicit focus() should focus the element
  GetDocument().getElementById(AtomicString("textfield"))->Focus();
  EXPECT_EQ(GetDocument().FocusedElement(),
            GetDocument().getElementById(AtomicString("textfield")));
}

TEST_P(DisplayLockContextTest, DisplayLockPreventsActivation) {
  ResizeAndFocus();
  SetHtmlInnerHTML(R"HTML(
    <body>
    <div id="shadowHost">
      <div id="slotted"></div>
    </div>
    </body>
  )HTML");

  auto* host = GetDocument().getElementById(AtomicString("shadowHost"));
  auto* slotted = GetDocument().getElementById(AtomicString("slotted"));

  ASSERT_FALSE(DisplayLockUtilities::ShouldIgnoreNodeDueToDisplayLock(
      *host, DisplayLockActivationReason::kAny));
  ASSERT_FALSE(DisplayLockUtilities::ShouldIgnoreNodeDueToDisplayLock(
      *slotted, DisplayLockActivationReason::kAny));

  ShadowRoot& shadow_root =
      host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  shadow_root.setInnerHTML(
      "<div id='container' style='contain:style layout "
      "paint;'><slot></slot></div>");
  UpdateAllLifecyclePhasesForTest();

  auto* container = shadow_root.getElementById(AtomicString("container"));
  ASSERT_FALSE(DisplayLockUtilities::ShouldIgnoreNodeDueToDisplayLock(
      *host, DisplayLockActivationReason::kAny));
  ASSERT_FALSE(DisplayLockUtilities::ShouldIgnoreNodeDueToDisplayLock(
      *container, DisplayLockActivationReason::kAny));
  ASSERT_FALSE(DisplayLockUtilities::ShouldIgnoreNodeDueToDisplayLock(
      *slotted, DisplayLockActivationReason::kAny));

  LockElement(*container, false);

  EXPECT_EQ(
      GetDocument().GetDisplayLockDocumentState().LockedDisplayLockCount(), 1);
  EXPECT_EQ(GetDocument()
                .GetDisplayLockDocumentState()
                .DisplayLockBlockingAllActivationCount(),
            1);
  ASSERT_FALSE(DisplayLockUtilities::ShouldIgnoreNodeDueToDisplayLock(
      *host, DisplayLockActivationReason::kAny));
  // The container itself is locked but that doesn't mean it should be ignored.
  ASSERT_FALSE(DisplayLockUtilities::ShouldIgnoreNodeDueToDisplayLock(
      *container, DisplayLockActivationReason::kAny));
  ASSERT_TRUE(DisplayLockUtilities::ShouldIgnoreNodeDueToDisplayLock(
      *slotted, DisplayLockActivationReason::kAny));

  // Ensure that we resolve the acquire callback, thus finishing the acquire
  // step.
  UpdateAllLifecyclePhasesForTest();

  CommitElement(*container);

  EXPECT_EQ(
      GetDocument().GetDisplayLockDocumentState().LockedDisplayLockCount(), 0);
  EXPECT_EQ(GetDocument()
                .GetDisplayLockDocumentState()
                .DisplayLockBlockingAllActivationCount(),
            0);
  ASSERT_FALSE(DisplayLockUtilities::ShouldIgnoreNodeDueToDisplayLock(
      *host, DisplayLockActivationReason::kAny));
  ASSERT_FALSE(DisplayLockUtilities::ShouldIgnoreNodeDueToDisplayLock(
      *container, DisplayLockActivationReason::kAny));
  ASSERT_FALSE(DisplayLockUtilities::ShouldIgnoreNodeDueToDisplayLock(
      *slotted, DisplayLockActivationReason::kAny));

  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(
      GetDocument().GetDisplayLockDocumentState().LockedDisplayLockCount(), 0);
  EXPECT_EQ(GetDocument()
                .GetDisplayLockDocumentState()
                .DisplayLockBlockingAllActivationCount(),
            0);
  ASSERT_FALSE(DisplayLockUtilities::ShouldIgnoreNodeDueToDisplayLock(
      *host, DisplayLockActivationReason::kAny));
  ASSERT_FALSE(DisplayLockUtilities::ShouldIgnoreNodeDueToDisplayLock(
      *container, DisplayLockActivationReason::kAny));
  ASSERT_FALSE(DisplayLockUtilities::ShouldIgnoreNodeDueToDisplayLock(
      *slotted, DisplayLockActivationReason::kAny));

  SetHtmlInnerHTML(R"HTML(
    <body>
    <div id="nonviewport" hidden=until-found>
      <div id="nonviewport-child"></div>
    </div>
    </body>
  )HTML");
  auto* non_viewport =
      GetDocument().getElementById(AtomicString("nonviewport-child"));

  EXPECT_EQ(
      GetDocument().GetDisplayLockDocumentState().LockedDisplayLockCount(), 1);
  EXPECT_EQ(GetDocument()
                .GetDisplayLockDocumentState()
                .DisplayLockBlockingAllActivationCount(),
            0);

  ASSERT_FALSE(DisplayLockUtilities::ShouldIgnoreNodeDueToDisplayLock(
      *non_viewport, DisplayLockActivationReason::kAny));
  ASSERT_FALSE(DisplayLockUtilities::ShouldIgnoreNodeDueToDisplayLock(
      *non_viewport, DisplayLockActivationReason::kFindInPage));
  ASSERT_TRUE(DisplayLockUtilities::ShouldIgnoreNodeDueToDisplayLock(
      *non_viewport, DisplayLockActivationReason::kUserFocus));
}

TEST_P(DisplayLockContextTest,
       LockedElementAndFlatTreeDescendantsAreNotFocusable) {
  ResizeAndFocus();
  SetHtmlInnerHTML(R"HTML(
    <body>
    <div id="shadowHost">
      <input id="textfield" type="text">
    </div>
    </body>
  )HTML");

  auto* host = GetDocument().getElementById(AtomicString("shadowHost"));
  auto* text_field = GetDocument().getElementById(AtomicString("textfield"));
  ShadowRoot& shadow_root =
      host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  shadow_root.setInnerHTML(
      "<div id='container' style='contain:style layout "
      "paint;'><slot></slot></div>");

  UpdateAllLifecyclePhasesForTest();
  ASSERT_TRUE(text_field->IsKeyboardFocusable());
  ASSERT_TRUE(text_field->IsFocusable());

  auto* element = shadow_root.getElementById(AtomicString("container"));
  LockElement(*element, false);

  // Sanity checks to ensure the element is locked.
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldStyleChildren());
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldLayoutChildren());
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldPaintChildren());
  EXPECT_EQ(
      GetDocument().GetDisplayLockDocumentState().LockedDisplayLockCount(), 1);
  EXPECT_EQ(GetDocument()
                .GetDisplayLockDocumentState()
                .DisplayLockBlockingAllActivationCount(),
            1);

  // The input should not be focusable now.
  EXPECT_FALSE(text_field->IsKeyboardFocusable());
  EXPECT_FALSE(text_field->IsFocusable());

  // Calling explicit focus() should also not focus the element.
  text_field->Focus();
  EXPECT_FALSE(GetDocument().FocusedElement());
}

TEST_P(DisplayLockContextTest, LockedCountsWithMultipleLocks) {
  ResizeAndFocus();
  SetHtmlInnerHTML(R"HTML(
    <style>
    .container {
      width: 100px;
      height: 100px;
      contain: style layout paint;
    }
    </style>
    <body>
    <div id="one" class="container">
      <div id="two" class="container"></div>
    </div>
    <div id="three" class="container"></div>
    </body>
  )HTML");

  EXPECT_EQ(
      GetDocument().GetDisplayLockDocumentState().LockedDisplayLockCount(), 0);
  EXPECT_EQ(GetDocument()
                .GetDisplayLockDocumentState()
                .DisplayLockBlockingAllActivationCount(),
            0);

  auto* one = GetDocument().getElementById(AtomicString("one"));
  auto* two = GetDocument().getElementById(AtomicString("two"));
  auto* three = GetDocument().getElementById(AtomicString("three"));

  LockElement(*one, false);

  EXPECT_EQ(
      GetDocument().GetDisplayLockDocumentState().LockedDisplayLockCount(), 1);
  EXPECT_EQ(GetDocument()
                .GetDisplayLockDocumentState()
                .DisplayLockBlockingAllActivationCount(),
            1);

  LockElement(*two, false);

  // Because |two| is nested, the lock counts aren't updated since the lock
  // doesn't actually take effect until style can determine that we should lock.
  EXPECT_EQ(
      GetDocument().GetDisplayLockDocumentState().LockedDisplayLockCount(), 1);
  EXPECT_EQ(GetDocument()
                .GetDisplayLockDocumentState()
                .DisplayLockBlockingAllActivationCount(),
            1);

  LockElement(*three, false);

  EXPECT_EQ(
      GetDocument().GetDisplayLockDocumentState().LockedDisplayLockCount(), 2);
  EXPECT_EQ(GetDocument()
                .GetDisplayLockDocumentState()
                .DisplayLockBlockingAllActivationCount(),
            2);

  // Now commit the outer lock.
  CommitElement(*one);

  // The counts remain the same since now the inner lock is determined to be
  // locked.
  EXPECT_EQ(
      GetDocument().GetDisplayLockDocumentState().LockedDisplayLockCount(), 2);
  EXPECT_EQ(GetDocument()
                .GetDisplayLockDocumentState()
                .DisplayLockBlockingAllActivationCount(),
            2);

  // Commit the inner lock.
  CommitElement(*two);

  // Both inner and outer locks should have committed.
  EXPECT_EQ(
      GetDocument().GetDisplayLockDocumentState().LockedDisplayLockCount(), 1);
  EXPECT_EQ(GetDocument()
                .GetDisplayLockDocumentState()
                .DisplayLockBlockingAllActivationCount(),
            1);

  // Commit the sibling lock.
  CommitElement(*three);

  // Both inner and outer locks should have committed.
  EXPECT_EQ(
      GetDocument().GetDisplayLockDocumentState().LockedDisplayLockCount(), 0);
  EXPECT_EQ(GetDocument()
                .GetDisplayLockDocumentState()
                .DisplayLockBlockingAllActivationCount(),
            0);
}

TEST_P(DisplayLockContextTest, ActivatableNotCountedAsBlocking) {
  ResizeAndFocus();
  SetHtmlInnerHTML(R"HTML(
    <style>
    .container {
      width: 100px;
      height: 100px;
      contain: style layout paint;
    }
    </style>
    <body>
    <div id="activatable" class="container"></div>
    <div id="nonActivatable" class="container"></div>
    </body>
  )HTML");

  EXPECT_EQ(
      GetDocument().GetDisplayLockDocumentState().LockedDisplayLockCount(), 0);
  EXPECT_EQ(GetDocument()
                .GetDisplayLockDocumentState()
                .DisplayLockBlockingAllActivationCount(),
            0);

  auto* activatable = GetDocument().getElementById(AtomicString("activatable"));
  auto* non_activatable =
      GetDocument().getElementById(AtomicString("nonActivatable"));

  // Initial display lock context should be activatable, since nothing skipped
  // activation for it.
  EXPECT_TRUE(activatable->EnsureDisplayLockContext().IsActivatable(
      DisplayLockActivationReason::kAny));

  LockElement(*activatable, true);

  EXPECT_EQ(
      GetDocument().GetDisplayLockDocumentState().LockedDisplayLockCount(), 1);
  EXPECT_EQ(GetDocument()
                .GetDisplayLockDocumentState()
                .DisplayLockBlockingAllActivationCount(),
            0);
  EXPECT_TRUE(activatable->GetDisplayLockContext()->IsActivatable(
      DisplayLockActivationReason::kAny));

  LockElement(*non_activatable, false);

  EXPECT_EQ(
      GetDocument().GetDisplayLockDocumentState().LockedDisplayLockCount(), 2);
  EXPECT_EQ(GetDocument()
                .GetDisplayLockDocumentState()
                .DisplayLockBlockingAllActivationCount(),
            1);
  EXPECT_FALSE(non_activatable->GetDisplayLockContext()->IsActivatable(
      DisplayLockActivationReason::kAny));

  // Now commit the lock for |non_activatable|.
  CommitElement(*non_activatable);

  EXPECT_EQ(
      GetDocument().GetDisplayLockDocumentState().LockedDisplayLockCount(), 1);
  EXPECT_EQ(GetDocument()
                .GetDisplayLockDocumentState()
                .DisplayLockBlockingAllActivationCount(),
            0);

  // Re-acquire the lock for |activatable| again with the activatable flag.
  LockElement(*activatable, true);

  EXPECT_EQ(
      GetDocument().GetDisplayLockDocumentState().LockedDisplayLockCount(), 1);
  EXPECT_EQ(GetDocument()
                .GetDisplayLockDocumentState()
                .DisplayLockBlockingAllActivationCount(),
            0);
  EXPECT_TRUE(activatable->GetDisplayLockContext()->IsActivatable(
      DisplayLockActivationReason::kAny));
}

TEST_P(DisplayLockContextTest, ElementInTemplate) {
  ResizeAndFocus();
  SetHtmlInnerHTML(R"HTML(
    <style>
    #child {
      width: 100px;
      height: 100px;
      contain: style layout paint;
    }
    #grandchild {
      color: blue;
    }
    #container {
      display: none;
    }
    </style>
    <body>
      <template id="template"><div id="child"><div id="grandchild">foo</div></div></template>
      <div id="container"></div>
    </body>
  )HTML");

  EXPECT_EQ(
      GetDocument().GetDisplayLockDocumentState().LockedDisplayLockCount(), 0);
  EXPECT_EQ(GetDocument()
                .GetDisplayLockDocumentState()
                .DisplayLockBlockingAllActivationCount(),
            0);

  auto* template_el = To<HTMLTemplateElement>(
      GetDocument().getElementById(AtomicString("template")));
  auto* child = To<Element>(template_el->content()->firstChild());
  EXPECT_FALSE(child->isConnected());

  // Try to lock an element in a template.
  LockElement(*child, false);

  EXPECT_EQ(
      GetDocument().GetDisplayLockDocumentState().LockedDisplayLockCount(), 0);
  EXPECT_EQ(GetDocument()
                .GetDisplayLockDocumentState()
                .DisplayLockBlockingAllActivationCount(),
            0);
  EXPECT_FALSE(child->GetDisplayLockContext());

  // Commit also works, but does nothing.
  CommitElement(*child);
  EXPECT_FALSE(child->GetDisplayLockContext());

  // Try to lock an element that was moved from a template to a document.
  auto* document_child =
      To<Element>(GetDocument().adoptNode(child, ASSERT_NO_EXCEPTION));
  auto* container = GetDocument().getElementById(AtomicString("container"));
  container->appendChild(document_child);

  LockElement(*document_child, false);

  // These should be 0, since container is display: none, so locking its child
  // is not visible to style.
  EXPECT_EQ(
      GetDocument().GetDisplayLockDocumentState().LockedDisplayLockCount(), 0);
  EXPECT_EQ(GetDocument()
                .GetDisplayLockDocumentState()
                .DisplayLockBlockingAllActivationCount(),
            0);
  ASSERT_FALSE(document_child->GetDisplayLockContext());

  container->setAttribute(html_names::kStyleAttr,
                          AtomicString("display: block;"));
  EXPECT_TRUE(container->NeedsStyleRecalc());
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(
      GetDocument().GetDisplayLockDocumentState().LockedDisplayLockCount(), 1);
  EXPECT_EQ(GetDocument()
                .GetDisplayLockDocumentState()
                .DisplayLockBlockingAllActivationCount(),
            1);
  ASSERT_TRUE(document_child->GetDisplayLockContext());
  EXPECT_TRUE(document_child->GetDisplayLockContext()->IsLocked());

  document_child->setAttribute(
      html_names::kStyleAttr,
      AtomicString("content-visibility: hidden; color: red;"));
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(document_child->NeedsStyleRecalc());

  // Commit will unlock the element and update the style.
  document_child->setAttribute(html_names::kStyleAttr,
                               AtomicString("color: red;"));
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(document_child->GetDisplayLockContext()->IsLocked());
  EXPECT_EQ(
      GetDocument().GetDisplayLockDocumentState().LockedDisplayLockCount(), 0);
  EXPECT_EQ(GetDocument()
                .GetDisplayLockDocumentState()
                .DisplayLockBlockingAllActivationCount(),
            0);

  EXPECT_FALSE(document_child->NeedsStyleRecalc());
  EXPECT_FALSE(document_child->ChildNeedsStyleRecalc());
  ASSERT_TRUE(document_child->GetComputedStyle());
  EXPECT_EQ(document_child->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()),
            Color::FromRGB(255, 0, 0));

  auto* grandchild = GetDocument().getElementById(AtomicString("grandchild"));
  EXPECT_FALSE(grandchild->NeedsStyleRecalc());
  EXPECT_FALSE(grandchild->ChildNeedsStyleRecalc());
  ASSERT_TRUE(grandchild->GetComputedStyle());
  EXPECT_EQ(grandchild->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()),
            Color::FromRGB(0, 0, 255));
}

TEST_P(DisplayLockContextTest, AncestorAllowedTouchAction) {
  SetHtmlInnerHTML(R"HTML(
    <style>
    #locked {
      width: 100px;
      height: 100px;
      contain: style layout paint;
    }
    </style>
    <div id="ancestor">
      <div id="handler">
        <div id="descendant">
          <div id="locked">
            <div id="lockedchild"></div>
          </div>
        </div>
      </div>
    </div>
  )HTML");

  auto* ancestor_element =
      GetDocument().getElementById(AtomicString("ancestor"));
  auto* handler_element = GetDocument().getElementById(AtomicString("handler"));
  auto* descendant_element =
      GetDocument().getElementById(AtomicString("descendant"));
  auto* locked_element = GetDocument().getElementById(AtomicString("locked"));
  auto* lockedchild_element =
      GetDocument().getElementById(AtomicString("lockedchild"));

  LockElement(*locked_element, false);
  EXPECT_TRUE(locked_element->GetDisplayLockContext()->IsLocked());

  auto* ancestor_object = ancestor_element->GetLayoutObject();
  auto* handler_object = handler_element->GetLayoutObject();
  auto* descendant_object = descendant_element->GetLayoutObject();
  auto* locked_object = locked_element->GetLayoutObject();
  auto* lockedchild_object = lockedchild_element->GetLayoutObject();

  EXPECT_FALSE(ancestor_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(handler_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(descendant_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(locked_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(lockedchild_object->EffectiveAllowedTouchActionChanged());

  EXPECT_FALSE(ancestor_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(handler_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(
      descendant_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(locked_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(
      lockedchild_object->DescendantEffectiveAllowedTouchActionChanged());

  EXPECT_FALSE(ancestor_object->InsideBlockingTouchEventHandler());
  EXPECT_FALSE(handler_object->InsideBlockingTouchEventHandler());
  EXPECT_FALSE(descendant_object->InsideBlockingTouchEventHandler());
  EXPECT_FALSE(locked_object->InsideBlockingTouchEventHandler());
  EXPECT_FALSE(lockedchild_object->InsideBlockingTouchEventHandler());

  auto* callback = MakeGarbageCollected<DisplayLockEmptyEventListener>();
  handler_element->addEventListener(event_type_names::kTouchstart, callback);

  EXPECT_FALSE(ancestor_object->EffectiveAllowedTouchActionChanged());
  EXPECT_TRUE(handler_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(descendant_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(locked_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(lockedchild_object->EffectiveAllowedTouchActionChanged());

  EXPECT_TRUE(ancestor_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(handler_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(
      descendant_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(locked_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(
      lockedchild_object->DescendantEffectiveAllowedTouchActionChanged());

  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(ancestor_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(handler_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(descendant_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(locked_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(lockedchild_object->EffectiveAllowedTouchActionChanged());

  EXPECT_FALSE(ancestor_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(handler_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(
      descendant_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(locked_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(
      lockedchild_object->DescendantEffectiveAllowedTouchActionChanged());

  EXPECT_FALSE(ancestor_object->InsideBlockingTouchEventHandler());
  EXPECT_TRUE(handler_object->InsideBlockingTouchEventHandler());
  EXPECT_TRUE(descendant_object->InsideBlockingTouchEventHandler());
  EXPECT_TRUE(locked_object->InsideBlockingTouchEventHandler());
  EXPECT_FALSE(lockedchild_object->InsideBlockingTouchEventHandler());

  // Manually commit the lock so that we can verify which dirty bits get
  // propagated.
  CommitElement(*locked_element, false);
  UnlockImmediate(locked_element->GetDisplayLockContext());

  EXPECT_FALSE(ancestor_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(handler_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(descendant_object->EffectiveAllowedTouchActionChanged());
  EXPECT_TRUE(locked_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(lockedchild_object->EffectiveAllowedTouchActionChanged());

  EXPECT_TRUE(ancestor_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_TRUE(handler_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_TRUE(
      descendant_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(locked_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(
      lockedchild_object->DescendantEffectiveAllowedTouchActionChanged());

  EXPECT_FALSE(ancestor_object->InsideBlockingTouchEventHandler());
  EXPECT_TRUE(handler_object->InsideBlockingTouchEventHandler());
  EXPECT_TRUE(descendant_object->InsideBlockingTouchEventHandler());
  EXPECT_TRUE(locked_object->InsideBlockingTouchEventHandler());
  EXPECT_FALSE(lockedchild_object->InsideBlockingTouchEventHandler());

  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(ancestor_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(handler_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(descendant_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(locked_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(lockedchild_object->EffectiveAllowedTouchActionChanged());

  EXPECT_FALSE(ancestor_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(handler_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(
      descendant_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(locked_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(
      lockedchild_object->DescendantEffectiveAllowedTouchActionChanged());

  EXPECT_FALSE(ancestor_object->InsideBlockingTouchEventHandler());
  EXPECT_TRUE(handler_object->InsideBlockingTouchEventHandler());
  EXPECT_TRUE(descendant_object->InsideBlockingTouchEventHandler());
  EXPECT_TRUE(locked_object->InsideBlockingTouchEventHandler());
  EXPECT_TRUE(lockedchild_object->InsideBlockingTouchEventHandler());
}

TEST_P(DisplayLockContextTest, DescendantAllowedTouchAction) {
  SetHtmlInnerHTML(R"HTML(
    <style>
    #locked {
      width: 100px;
      height: 100px;
      contain: style layout paint;
    }
    </style>
    <div id="ancestor">
      <div id="descendant">
        <div id="locked">
          <div id="handler"></div>
        </div>
      </div>
    </div>
  )HTML");

  auto* ancestor_element =
      GetDocument().getElementById(AtomicString("ancestor"));
  auto* descendant_element =
      GetDocument().getElementById(AtomicString("descendant"));
  auto* locked_element = GetDocument().getElementById(AtomicString("locked"));
  auto* handler_element = GetDocument().getElementById(AtomicString("handler"));

  LockElement(*locked_element, false);
  EXPECT_TRUE(locked_element->GetDisplayLockContext()->IsLocked());

  auto* ancestor_object = ancestor_element->GetLayoutObject();
  auto* descendant_object = descendant_element->GetLayoutObject();
  auto* locked_object = locked_element->GetLayoutObject();
  auto* handler_object = handler_element->GetLayoutObject();

  EXPECT_FALSE(ancestor_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(descendant_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(locked_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(handler_object->EffectiveAllowedTouchActionChanged());

  EXPECT_FALSE(ancestor_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(
      descendant_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(locked_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(handler_object->DescendantEffectiveAllowedTouchActionChanged());

  EXPECT_FALSE(ancestor_object->InsideBlockingTouchEventHandler());
  EXPECT_FALSE(descendant_object->InsideBlockingTouchEventHandler());
  EXPECT_FALSE(locked_object->InsideBlockingTouchEventHandler());
  EXPECT_FALSE(handler_object->InsideBlockingTouchEventHandler());

  auto* callback = MakeGarbageCollected<DisplayLockEmptyEventListener>();
  handler_element->addEventListener(event_type_names::kTouchstart, callback);

  EXPECT_FALSE(ancestor_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(descendant_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(locked_object->EffectiveAllowedTouchActionChanged());
  EXPECT_TRUE(handler_object->EffectiveAllowedTouchActionChanged());

  EXPECT_FALSE(ancestor_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(
      descendant_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_TRUE(locked_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(handler_object->DescendantEffectiveAllowedTouchActionChanged());

  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(ancestor_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(descendant_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(locked_object->EffectiveAllowedTouchActionChanged());
  EXPECT_TRUE(handler_object->EffectiveAllowedTouchActionChanged());

  EXPECT_FALSE(ancestor_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(
      descendant_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_TRUE(locked_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(handler_object->DescendantEffectiveAllowedTouchActionChanged());

  EXPECT_FALSE(ancestor_object->InsideBlockingTouchEventHandler());
  EXPECT_FALSE(descendant_object->InsideBlockingTouchEventHandler());
  EXPECT_FALSE(locked_object->InsideBlockingTouchEventHandler());
  EXPECT_FALSE(handler_object->InsideBlockingTouchEventHandler());

  // Do the same check again. For now, nothing is expected to change. However,
  // when we separate self and child layout, then some flags would be different.
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(ancestor_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(descendant_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(locked_object->EffectiveAllowedTouchActionChanged());
  EXPECT_TRUE(handler_object->EffectiveAllowedTouchActionChanged());

  EXPECT_FALSE(ancestor_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(
      descendant_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_TRUE(locked_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(handler_object->DescendantEffectiveAllowedTouchActionChanged());

  EXPECT_FALSE(ancestor_object->InsideBlockingTouchEventHandler());
  EXPECT_FALSE(descendant_object->InsideBlockingTouchEventHandler());
  EXPECT_FALSE(locked_object->InsideBlockingTouchEventHandler());
  EXPECT_FALSE(handler_object->InsideBlockingTouchEventHandler());

  // Manually commit the lock so that we can verify which dirty bits get
  // propagated.
  CommitElement(*locked_element, false);
  UnlockImmediate(locked_element->GetDisplayLockContext());

  EXPECT_FALSE(ancestor_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(descendant_object->EffectiveAllowedTouchActionChanged());
  EXPECT_TRUE(locked_object->EffectiveAllowedTouchActionChanged());
  EXPECT_TRUE(handler_object->EffectiveAllowedTouchActionChanged());

  EXPECT_TRUE(ancestor_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_TRUE(
      descendant_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_TRUE(locked_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(handler_object->DescendantEffectiveAllowedTouchActionChanged());

  EXPECT_FALSE(ancestor_object->InsideBlockingTouchEventHandler());
  EXPECT_FALSE(descendant_object->InsideBlockingTouchEventHandler());
  EXPECT_FALSE(locked_object->InsideBlockingTouchEventHandler());
  EXPECT_FALSE(handler_object->InsideBlockingTouchEventHandler());

  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(ancestor_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(descendant_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(locked_object->EffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(handler_object->EffectiveAllowedTouchActionChanged());

  EXPECT_FALSE(ancestor_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(
      descendant_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(locked_object->DescendantEffectiveAllowedTouchActionChanged());
  EXPECT_FALSE(handler_object->DescendantEffectiveAllowedTouchActionChanged());

  EXPECT_FALSE(ancestor_object->InsideBlockingTouchEventHandler());
  EXPECT_FALSE(descendant_object->InsideBlockingTouchEventHandler());
  EXPECT_FALSE(locked_object->InsideBlockingTouchEventHandler());
  EXPECT_TRUE(handler_object->InsideBlockingTouchEventHandler());
}

TEST_P(DisplayLockContextTest, AncestorWheelEventHandler) {
  SetHtmlInnerHTML(R"HTML(
    <style>
    #locked {
      width: 100px;
      height: 100px;
      contain: style layout paint;
    }
    </style>
    <div id="ancestor">
      <div id="handler">
        <div id="descendant">
          <div id="locked">
            <div id="lockedchild"></div>
          </div>
        </div>
      </div>
    </div>
  )HTML");

  auto* ancestor_element =
      GetDocument().getElementById(AtomicString("ancestor"));
  auto* handler_element = GetDocument().getElementById(AtomicString("handler"));
  auto* descendant_element =
      GetDocument().getElementById(AtomicString("descendant"));
  auto* locked_element = GetDocument().getElementById(AtomicString("locked"));
  auto* lockedchild_element =
      GetDocument().getElementById(AtomicString("lockedchild"));

  LockElement(*locked_element, false);
  EXPECT_TRUE(locked_element->GetDisplayLockContext()->IsLocked());

  auto* ancestor_object = ancestor_element->GetLayoutObject();
  auto* handler_object = handler_element->GetLayoutObject();
  auto* descendant_object = descendant_element->GetLayoutObject();
  auto* locked_object = locked_element->GetLayoutObject();
  auto* lockedchild_object = lockedchild_element->GetLayoutObject();

  EXPECT_FALSE(ancestor_object->BlockingWheelEventHandlerChanged());
  EXPECT_FALSE(handler_object->BlockingWheelEventHandlerChanged());
  EXPECT_FALSE(descendant_object->BlockingWheelEventHandlerChanged());
  EXPECT_FALSE(locked_object->BlockingWheelEventHandlerChanged());
  EXPECT_FALSE(lockedchild_object->BlockingWheelEventHandlerChanged());

  EXPECT_FALSE(ancestor_object->DescendantBlockingWheelEventHandlerChanged());
  EXPECT_FALSE(handler_object->DescendantBlockingWheelEventHandlerChanged());
  EXPECT_FALSE(descendant_object->DescendantBlockingWheelEventHandlerChanged());
  EXPECT_FALSE(locked_object->DescendantBlockingWheelEventHandlerChanged());
  EXPECT_FALSE(
      lockedchild_object->DescendantBlockingWheelEventHandlerChanged());

  EXPECT_FALSE(ancestor_object->InsideBlockingWheelEventHandler());
  EXPECT_FALSE(handler_object->InsideBlockingWheelEventHandler());
  EXPECT_FALSE(descendant_object->InsideBlockingWheelEventHandler());
  EXPECT_FALSE(locked_object->InsideBlockingWheelEventHandler());
  EXPECT_FALSE(lockedchild_object->InsideBlockingWheelEventHandler());

  auto* callback = MakeGarbageCollected<DisplayLockEmptyEventListener>();
  handler_element->addEventListener(event_type_names::kWheel, callback);

  EXPECT_FALSE(ancestor_object->BlockingWheelEventHandlerChanged());
  EXPECT_TRUE(handler_object->BlockingWheelEventHandlerChanged());
  EXPECT_FALSE(descendant_object->BlockingWheelEventHandlerChanged());
  EXPECT_FALSE(locked_object->BlockingWheelEventHandlerChanged());
  EXPECT_FALSE(lockedchild_object->BlockingWheelEventHandlerChanged());

  EXPECT_TRUE(ancestor_object->DescendantBlockingWheelEventHandlerChanged());
  EXPECT_FALSE(handler_object->DescendantBlockingWheelEventHandlerChanged());
  EXPECT_FALSE(descendant_object->DescendantBlockingWheelEventHandlerChanged());
  EXPECT_FALSE(locked_object->DescendantBlockingWheelEventHandlerChanged());
  EXPECT_FALSE(
      lockedchild_object->DescendantBlockingWheelEventHandlerChanged());

  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(ancestor_object->BlockingWheelEventHandlerChanged());
  EXPECT_FALSE(handler_object->BlockingWheelEventHandlerChanged());
  EXPECT_FALSE(descendant_object->BlockingWheelEventHandlerChanged());
  EXPECT_FALSE(locked_object->BlockingWheelEventHandlerChanged());
  EXPECT_FALSE(lockedchild_object->BlockingWheelEventHandlerChanged());

  EXPECT_FALSE(ancestor_object->DescendantBlockingWheelEventHandlerChanged());
  EXPECT_FALSE(handler_object->DescendantBlockingWheelEventHandlerChanged());
  EXPECT_FALSE(descendant_object->DescendantBlockingWheelEventHandlerChanged());
  EXPECT_FALSE(locked_object->DescendantBlockingWheelEventHandlerChanged());
  EXPECT_FALSE(
      lockedchild_object->DescendantBlockingWheelEventHandlerChanged());

  EXPECT_FALSE(ancestor_object->InsideBlockingWheelEventHandler());
  EXPECT_TRUE(handler_object->InsideBlockingWheelEventHandler());
  EXPECT_TRUE(descendant_object->InsideBlockingWheelEventHandler());
  EXPECT_TRUE(locked_object->InsideBlockingWheelEventHandler());
  EXPECT_FALSE(lockedchild_object->InsideBlockingWheelEventHandler());

  // Manually commit the lock so that we can verify which dirty bits get
  // propagated.
  CommitElement(*locked_element, false);
  UnlockImmediate(locked_element->GetDisplayLockContext());

  EXPECT_FALSE(ancestor_object->BlockingWheelEventHandlerChanged());
  EXPECT_FALSE(handler_object->BlockingWheelEventHandlerChanged());
  EXPECT_FALSE(descendant_object->BlockingWheelEventHandlerChanged());
  EXPECT_TRUE(locked_object->BlockingWheelEventHandlerChanged());
  EXPECT_FALSE(lockedchild_object->BlockingWheelEventHandlerChanged());

  EXPECT_TRUE(ancestor_object->DescendantBlockingWheelEventHandlerChanged());
  EXPECT_TRUE(handler_object->DescendantBlockingWheelEventHandlerChanged());
  EXPECT_TRUE(descendant_object->DescendantBlockingWheelEventHandlerChanged());
  EXPECT_FALSE(locked_object->DescendantBlockingWheelEventHandlerChanged());
  EXPECT_FALSE(
      lockedchild_object->DescendantBlockingWheelEventHandlerChanged());

  EXPECT_FALSE(ancestor_object->InsideBlockingWheelEventHandler());
  EXPECT_TRUE(handler_object->InsideBlockingWheelEventHandler());
  EXPECT_TRUE(descendant_object->InsideBlockingWheelEventHandler());
  EXPECT_TRUE(locked_object->InsideBlockingWheelEventHandler());
  EXPECT_FALSE(lockedchild_object->InsideBlockingWheelEventHandler());

  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(ancestor_object->BlockingWheelEventHandlerChanged());
  EXPECT_FALSE(handler_object->BlockingWheelEventHandlerChanged());
  EXPECT_FALSE(descendant_object->BlockingWheelEventHandlerChanged());
  EXPECT_FALSE(locked_object->BlockingWheelEventHandlerChanged());
  EXPECT_FALSE(lockedchild_object->BlockingWheelEventHandlerChanged());

  EXPECT_FALSE(ancestor_object->DescendantBlockingWheelEventHandlerChanged());
  EXPECT_FALSE(handler_object->DescendantBlockingWheelEventHandlerChanged());
  EXPECT_FALSE(descendant_object->DescendantBlockingWheelEventHandlerChanged());
  EXPECT_FALSE(locked_object->DescendantBlockingWheelEventHandlerChanged());
  EXPECT_FALSE(
      lockedchild_object->DescendantBlockingWheelEventHandlerChanged());

  EXPECT_FALSE(ancestor_object->InsideBlockingWheelEventHandler());
  EXPECT_TRUE(handler_object->InsideBlockingWheelEventHandler());
  EXPECT_TRUE(descendant_object->InsideBlockingWheelEventHandler());
  EXPECT_TRUE(locked_object->InsideBlockingWheelEventHandler());
  EXPECT_TRUE(lockedchild_object->InsideBlockingWheelEventHandler());
}

TEST_P(DisplayLockContextTest, DescendantWheelEventHandler) {
  SetHtmlInnerHTML(R"HTML(
    <style>
    #locked {
      width: 100px;
      height: 100px;
      contain: style layout paint;
    }
    </style>
    <div id="ancestor">
      <div id="descendant">
        <div id="locked">
          <div id="handler"></div>
        </div>
      </div>
    </div>
  )HTML");

  auto* ancestor_element =
      GetDocument().getElementById(AtomicString("ancestor"));
  auto* descendant_element =
      GetDocument().getElementById(AtomicString("descendant"));
  auto* locked_element = GetDocument().getElementById(AtomicString("locked"));
  auto* handler_element = GetDocument().getElementById(AtomicString("handler"));

  LockElement(*locked_element, false);
  EXPECT_TRUE(locked_element->GetDisplayLockContext()->IsLocked());

  auto* ancestor_object = ancestor_element->GetLayoutObject();
  auto* descendant_object = descendant_element->GetLayoutObject();
  auto* locked_object = locked_element->GetLayoutObject();
  auto* handler_object = handler_element->GetLayoutObject();

  EXPECT_FALSE(ancestor_object->BlockingWheelEventHandlerChanged());
  EXPECT_FALSE(descendant_object->BlockingWheelEventHandlerChanged());
  EXPECT_FALSE(locked_object->BlockingWheelEventHandlerChanged());
  EXPECT_FALSE(handler_object->BlockingWheelEventHandlerChanged());

  EXPECT_FALSE(ancestor_object->DescendantBlockingWheelEventHandlerChanged());
  EXPECT_FALSE(descendant_object->DescendantBlockingWheelEventHandlerChanged());
  EXPECT_FALSE(locked_object->DescendantBlockingWheelEventHandlerChanged());
  EXPECT_FALSE(handler_object->DescendantBlockingWheelEventHandlerChanged());

  EXPECT_FALSE(ancestor_object->InsideBlockingWheelEventHandler());
  EXPECT_FALSE(descendant_object->InsideBlockingWheelEventHandler());
  EXPECT_FALSE(locked_object->InsideBlockingWheelEventHandler());
  EXPECT_FALSE(handler_object->InsideBlockingWheelEventHandler());

  auto* callback = MakeGarbageCollected<DisplayLockEmptyEventListener>();
  handler_element->addEventListener(event_type_names::kWheel, callback);

  EXPECT_FALSE(ancestor_object->BlockingWheelEventHandlerChanged());
  EXPECT_FALSE(descendant_object->BlockingWheelEventHandlerChanged());
  EXPECT_FALSE(locked_object->BlockingWheelEventHandlerChanged());
  EXPECT_TRUE(handler_object->BlockingWheelEventHandlerChanged());

  EXPECT_FALSE(ancestor_object->DescendantBlockingWheelEventHandlerChanged());
  EXPECT_FALSE(descendant_object->DescendantBlockingWheelEventHandlerChanged());
  EXPECT_TRUE(locked_object->DescendantBlockingWheelEventHandlerChanged());
  EXPECT_FALSE(handler_object->DescendantBlockingWheelEventHandlerChanged());

  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(ancestor_object->BlockingWheelEventHandlerChanged());
  EXPECT_FALSE(descendant_object->BlockingWheelEventHandlerChanged());
  EXPECT_FALSE(locked_object->BlockingWheelEventHandlerChanged());
  EXPECT_TRUE(handler_object->BlockingWheelEventHandlerChanged());

  EXPECT_FALSE(ancestor_object->DescendantBlockingWheelEventHandlerChanged());
  EXPECT_FALSE(descendant_object->DescendantBlockingWheelEventHandlerChanged());
  EXPECT_TRUE(locked_object->DescendantBlockingWheelEventHandlerChanged());
  EXPECT_FALSE(handler_object->DescendantBlockingWheelEventHandlerChanged());

  EXPECT_FALSE(ancestor_object->InsideBlockingWheelEventHandler());
  EXPECT_FALSE(descendant_object->InsideBlockingWheelEventHandler());
  EXPECT_FALSE(locked_object->InsideBlockingWheelEventHandler());
  EXPECT_FALSE(handler_object->InsideBlockingWheelEventHandler());

  // Do the same check again. For now, nothing is expected to change. However,
  // when we separate self and child layout, then some flags would be different.
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(ancestor_object->BlockingWheelEventHandlerChanged());
  EXPECT_FALSE(descendant_object->BlockingWheelEventHandlerChanged());
  EXPECT_FALSE(locked_object->BlockingWheelEventHandlerChanged());
  EXPECT_TRUE(handler_object->BlockingWheelEventHandlerChanged());

  EXPECT_FALSE(ancestor_object->DescendantBlockingWheelEventHandlerChanged());
  EXPECT_FALSE(descendant_object->DescendantBlockingWheelEventHandlerChanged());
  EXPECT_TRUE(locked_object->DescendantBlockingWheelEventHandlerChanged());
  EXPECT_FALSE(handler_object->DescendantBlockingWheelEventHandlerChanged());

  EXPECT_FALSE(ancestor_object->InsideBlockingWheelEventHandler());
  EXPECT_FALSE(descendant_object->InsideBlockingWheelEventHandler());
  EXPECT_FALSE(locked_object->InsideBlockingWheelEventHandler());
  EXPECT_FALSE(handler_object->InsideBlockingWheelEventHandler());

  // Manually commit the lock so that we can verify which dirty bits get
  // propagated.
  CommitElement(*locked_element, false);
  UnlockImmediate(locked_element->GetDisplayLockContext());

  EXPECT_FALSE(ancestor_object->BlockingWheelEventHandlerChanged());
  EXPECT_FALSE(descendant_object->BlockingWheelEventHandlerChanged());
  EXPECT_TRUE(locked_object->BlockingWheelEventHandlerChanged());
  EXPECT_TRUE(handler_object->BlockingWheelEventHandlerChanged());

  EXPECT_TRUE(ancestor_object->DescendantBlockingWheelEventHandlerChanged());
  EXPECT_TRUE(descendant_object->DescendantBlockingWheelEventHandlerChanged());
  EXPECT_TRUE(locked_object->DescendantBlockingWheelEventHandlerChanged());
  EXPECT_FALSE(handler_object->DescendantBlockingWheelEventHandlerChanged());

  EXPECT_FALSE(ancestor_object->InsideBlockingWheelEventHandler());
  EXPECT_FALSE(descendant_object->InsideBlockingWheelEventHandler());
  EXPECT_FALSE(locked_object->InsideBlockingWheelEventHandler());
  EXPECT_FALSE(handler_object->InsideBlockingWheelEventHandler());

  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(ancestor_object->BlockingWheelEventHandlerChanged());
  EXPECT_FALSE(descendant_object->BlockingWheelEventHandlerChanged());
  EXPECT_FALSE(locked_object->BlockingWheelEventHandlerChanged());
  EXPECT_FALSE(handler_object->BlockingWheelEventHandlerChanged());

  EXPECT_FALSE(ancestor_object->DescendantBlockingWheelEventHandlerChanged());
  EXPECT_FALSE(descendant_object->DescendantBlockingWheelEventHandlerChanged());
  EXPECT_FALSE(locked_object->DescendantBlockingWheelEventHandlerChanged());
  EXPECT_FALSE(handler_object->DescendantBlockingWheelEventHandlerChanged());

  EXPECT_FALSE(ancestor_object->InsideBlockingWheelEventHandler());
  EXPECT_FALSE(descendant_object->InsideBlockingWheelEventHandler());
  EXPECT_FALSE(locked_object->InsideBlockingWheelEventHandler());
  EXPECT_TRUE(handler_object->InsideBlockingWheelEventHandler());
}

TEST_P(DisplayLockContextTest, DescendantNeedsPaintPropertyUpdateBlocked) {
  SetHtmlInnerHTML(R"HTML(
    <style>
    #locked {
      width: 100px;
      height: 100px;
      contain: style layout paint;
    }
    </style>
    <div id="ancestor">
      <div id="descendant">
        <div id="locked">
          <div id="handler"></div>
        </div>
      </div>
    </div>
  )HTML");

  auto* ancestor_element =
      GetDocument().getElementById(AtomicString("ancestor"));
  auto* descendant_element =
      GetDocument().getElementById(AtomicString("descendant"));
  auto* locked_element = GetDocument().getElementById(AtomicString("locked"));
  auto* handler_element = GetDocument().getElementById(AtomicString("handler"));

  LockElement(*locked_element, false);
  EXPECT_TRUE(locked_element->GetDisplayLockContext()->IsLocked());

  auto* ancestor_object = ancestor_element->GetLayoutObject();
  auto* descendant_object = descendant_element->GetLayoutObject();
  auto* locked_object = locked_element->GetLayoutObject();
  auto* handler_object = handler_element->GetLayoutObject();

  EXPECT_FALSE(ancestor_object->NeedsPaintPropertyUpdate());
  EXPECT_FALSE(descendant_object->NeedsPaintPropertyUpdate());
  EXPECT_FALSE(locked_object->NeedsPaintPropertyUpdate());
  EXPECT_FALSE(handler_object->NeedsPaintPropertyUpdate());

  EXPECT_FALSE(ancestor_object->DescendantNeedsPaintPropertyUpdate());
  EXPECT_FALSE(descendant_object->DescendantNeedsPaintPropertyUpdate());
  EXPECT_FALSE(locked_object->DescendantNeedsPaintPropertyUpdate());
  EXPECT_FALSE(handler_object->DescendantNeedsPaintPropertyUpdate());

  handler_object->SetNeedsPaintPropertyUpdate();

  EXPECT_FALSE(ancestor_object->NeedsPaintPropertyUpdate());
  EXPECT_FALSE(descendant_object->NeedsPaintPropertyUpdate());
  EXPECT_FALSE(locked_object->NeedsPaintPropertyUpdate());
  EXPECT_TRUE(handler_object->NeedsPaintPropertyUpdate());

  EXPECT_TRUE(ancestor_object->DescendantNeedsPaintPropertyUpdate());
  EXPECT_TRUE(descendant_object->DescendantNeedsPaintPropertyUpdate());
  EXPECT_TRUE(locked_object->DescendantNeedsPaintPropertyUpdate());
  EXPECT_FALSE(handler_object->DescendantNeedsPaintPropertyUpdate());

  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(ancestor_object->NeedsPaintPropertyUpdate());
  EXPECT_FALSE(descendant_object->NeedsPaintPropertyUpdate());
  EXPECT_FALSE(locked_object->NeedsPaintPropertyUpdate());
  EXPECT_TRUE(handler_object->NeedsPaintPropertyUpdate());

  EXPECT_FALSE(ancestor_object->DescendantNeedsPaintPropertyUpdate());
  EXPECT_FALSE(descendant_object->DescendantNeedsPaintPropertyUpdate());
  EXPECT_TRUE(locked_object->DescendantNeedsPaintPropertyUpdate());
  EXPECT_FALSE(handler_object->DescendantNeedsPaintPropertyUpdate());

  locked_object->SetShouldCheckForPaintInvalidationWithoutLayoutChange();
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(ancestor_object->NeedsPaintPropertyUpdate());
  EXPECT_FALSE(descendant_object->NeedsPaintPropertyUpdate());
  EXPECT_FALSE(locked_object->NeedsPaintPropertyUpdate());
  EXPECT_TRUE(handler_object->NeedsPaintPropertyUpdate());

  EXPECT_FALSE(ancestor_object->DescendantNeedsPaintPropertyUpdate());
  EXPECT_FALSE(descendant_object->DescendantNeedsPaintPropertyUpdate());
  EXPECT_TRUE(locked_object->DescendantNeedsPaintPropertyUpdate());
  EXPECT_FALSE(handler_object->DescendantNeedsPaintPropertyUpdate());

  // Manually commit the lock so that we can verify which dirty bits get
  // propagated.
  CommitElement(*locked_element, false);
  UnlockImmediate(locked_element->GetDisplayLockContext());

  EXPECT_FALSE(ancestor_object->NeedsPaintPropertyUpdate());
  EXPECT_FALSE(descendant_object->NeedsPaintPropertyUpdate());
  EXPECT_TRUE(locked_object->NeedsPaintPropertyUpdate());
  EXPECT_TRUE(handler_object->NeedsPaintPropertyUpdate());

  EXPECT_TRUE(ancestor_object->DescendantNeedsPaintPropertyUpdate());
  EXPECT_TRUE(descendant_object->DescendantNeedsPaintPropertyUpdate());
  EXPECT_TRUE(locked_object->DescendantNeedsPaintPropertyUpdate());
  EXPECT_FALSE(handler_object->DescendantNeedsPaintPropertyUpdate());

  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(ancestor_object->NeedsPaintPropertyUpdate());
  EXPECT_FALSE(descendant_object->NeedsPaintPropertyUpdate());
  EXPECT_FALSE(locked_object->NeedsPaintPropertyUpdate());
  EXPECT_FALSE(handler_object->NeedsPaintPropertyUpdate());

  EXPECT_FALSE(ancestor_object->DescendantNeedsPaintPropertyUpdate());
  EXPECT_FALSE(descendant_object->DescendantNeedsPaintPropertyUpdate());
  EXPECT_FALSE(locked_object->DescendantNeedsPaintPropertyUpdate());
  EXPECT_FALSE(handler_object->DescendantNeedsPaintPropertyUpdate());
}

class DisplayLockContextRenderingTest
    : public RenderingTest,
      public testing::WithParamInterface<bool>,
      private ScopedIntersectionOptimizationForTest {
 public:
  DisplayLockContextRenderingTest()
      : RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()),
        ScopedIntersectionOptimizationForTest(GetParam()) {}

  void SetUp() override {
    EnableCompositing();
    RenderingTest::SetUp();
  }

  bool IsObservingLifecycle(DisplayLockContext* context) const {
    return context->is_registered_for_lifecycle_notifications_;
  }
  bool DescendantDependentFlagUpdateWasBlocked(
      DisplayLockContext* context) const {
    return context->needs_compositing_dependent_flag_update_;
  }
  void LockImmediate(DisplayLockContext* context) {
    context->SetRequestedState(EContentVisibility::kHidden);
  }
  void RunStartOfLifecycleTasks() {
    auto start_of_lifecycle_tasks =
        GetDocument().View()->TakeStartOfLifecycleTasksForTest();
    for (auto& task : start_of_lifecycle_tasks)
      std::move(task).Run();
  }
  DisplayLockUtilities::ScopedForcedUpdate GetScopedForcedUpdate(
      const Node* node,
      DisplayLockContext::ForcedPhase phase,
      bool include_self = false) {
    return DisplayLockUtilities::ScopedForcedUpdate(node, phase, include_self);
  }
};

INSTANTIATE_TEST_SUITE_P(All, DisplayLockContextRenderingTest, testing::Bool());

TEST_P(DisplayLockContextRenderingTest, FrameDocumentRemovedWhileAcquire) {
  SetHtmlInnerHTML(R"HTML(
    <iframe id="frame"></iframe>
  )HTML");
  SetChildFrameHTML(R"HTML(
    <style>
      div {
        contain: style layout;
      }
    </style>
    <div id="target"></target>
  )HTML");

  auto* target = ChildDocument().getElementById(AtomicString("target"));
  GetDocument().getElementById(AtomicString("frame"))->remove();

  LockImmediate(&target->EnsureDisplayLockContext());
}

TEST_P(DisplayLockContextRenderingTest,
       VisualOverflowCalculateOnChildPaintLayer) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      .hidden { content-visibility: hidden }
      .paint_layer { contain: paint }
      .composited { will-change: transform }
    </style>
    <div id=lockable class=paint_layer>
      <div id=parent class=paint_layer>
        <div id=child class=paint_layer>
          <span>content</span>
          <span>content</span>
          <span>content</span>
        </div>
      </div>
    </div>
  )HTML");

  auto* parent = GetDocument().getElementById(AtomicString("parent"));
  auto* parent_box = parent->GetLayoutBoxModelObject();
  ASSERT_TRUE(parent_box);
  EXPECT_TRUE(parent_box->Layer());
  EXPECT_TRUE(parent_box->HasSelfPaintingLayer());

  // Lock the container.
  auto* lockable = GetDocument().getElementById(AtomicString("lockable"));
  lockable->classList().Add(AtomicString("hidden"));
  UpdateAllLifecyclePhasesForTest();

  auto* child_layer = GetPaintLayerByElementId("child");
  child_layer->SetNeedsVisualOverflowRecalc();
  EXPECT_TRUE(child_layer->NeedsVisualOverflowRecalc());

  // The following should not crash/DCHECK.
  UpdateAllLifecyclePhasesForTest();

  // Verify that the display lock knows that the descendant dependent flags
  // update was blocked.
  ASSERT_TRUE(lockable->GetDisplayLockContext());
  EXPECT_TRUE(DescendantDependentFlagUpdateWasBlocked(
      lockable->GetDisplayLockContext()));
  EXPECT_TRUE(child_layer->NeedsVisualOverflowRecalc());

  // After unlocking, we should process the pending visual overflow recalc.
  lockable->classList().Remove(AtomicString("hidden"));
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(child_layer->NeedsVisualOverflowRecalc());
}

TEST_P(DisplayLockContextRenderingTest, FloatChildLocked) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      .hidden { content-visibility: hidden }
      #floating { float: left; width: 100px; height: 100px }
    </style>
    <div id=lockable style="width: 200px; height: 50px; position: absolute">
      <div id=floating></div>
    </div>
  )HTML");

  auto* lockable = GetDocument().getElementById(AtomicString("lockable"));
  auto* lockable_box = lockable->GetLayoutBox();
  auto* floating = GetDocument().getElementById(AtomicString("floating"));
  EXPECT_EQ(PhysicalRect(0, 0, 200, 100), lockable_box->VisualOverflowRect());
  EXPECT_EQ(PhysicalRect(0, 0, 200, 100),
            lockable_box->ScrollableOverflowRect());

  lockable->classList().Add(AtomicString("hidden"));
  UpdateAllLifecyclePhasesForTest();

  // Verify that the display lock knows that the descendant dependent flags
  // update was blocked.
  ASSERT_TRUE(lockable->GetDisplayLockContext());
  EXPECT_TRUE(DescendantDependentFlagUpdateWasBlocked(
      lockable->GetDisplayLockContext()));
  EXPECT_EQ(PhysicalRect(0, 0, 200, 50), lockable_box->VisualOverflowRect());
  EXPECT_EQ(PhysicalRect(0, 0, 200, 50),
            lockable_box->ScrollableOverflowRect());

  floating->setAttribute(html_names::kStyleAttr, AtomicString("height: 200px"));
  // The following should not crash/DCHECK.
  UpdateAllLifecyclePhasesForTest();

  ASSERT_TRUE(lockable->GetDisplayLockContext());
  EXPECT_TRUE(DescendantDependentFlagUpdateWasBlocked(
      lockable->GetDisplayLockContext()));
  EXPECT_EQ(PhysicalRect(0, 0, 200, 50), lockable_box->VisualOverflowRect());
  EXPECT_EQ(PhysicalRect(0, 0, 200, 50),
            lockable_box->ScrollableOverflowRect());

  // After unlocking, we should process the pending visual overflow recalc.
  lockable->classList().Remove(AtomicString("hidden"));
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(PhysicalRect(0, 0, 200, 200), lockable_box->VisualOverflowRect());
  EXPECT_EQ(PhysicalRect(0, 0, 200, 200),
            lockable_box->ScrollableOverflowRect());
}

TEST_P(DisplayLockContextRenderingTest,
       VisualOverflowCalculateOnChildPaintLayerInForcedLock) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      .hidden { content-visibility: hidden }
      .paint_layer { contain: paint }
      .composited { will-change: transform }
    </style>
    <div id=lockable class=paint_layer>
      <div id=parent class=paint_layer>
        <div id=child class=paint_layer>
          <span>content</span>
          <span>content</span>
          <span>content</span>
        </div>
      </div>
    </div>
  )HTML");

  auto* parent = GetDocument().getElementById(AtomicString("parent"));
  auto* parent_box = parent->GetLayoutBoxModelObject();
  ASSERT_TRUE(parent_box);
  EXPECT_TRUE(parent_box->Layer());
  EXPECT_TRUE(parent_box->HasSelfPaintingLayer());

  // Lock the container.
  auto* lockable = GetDocument().getElementById(AtomicString("lockable"));
  lockable->classList().Add(AtomicString("hidden"));
  UpdateAllLifecyclePhasesForTest();

  auto* child_layer = GetPaintLayerByElementId("child");
  child_layer->SetNeedsVisualOverflowRecalc();
  EXPECT_TRUE(child_layer->NeedsVisualOverflowRecalc());

  ASSERT_TRUE(lockable->GetDisplayLockContext());
  {
    auto scope = GetScopedForcedUpdate(
        lockable, DisplayLockContext::ForcedPhase::kPrePaint,
        true /* include self */);

    // The following should not crash/DCHECK.
    UpdateAllLifecyclePhasesForTest();
  }

  // Verify that the display lock doesn't keep extra state since the update was
  // processed.
  EXPECT_FALSE(DescendantDependentFlagUpdateWasBlocked(
      lockable->GetDisplayLockContext()));
  EXPECT_FALSE(child_layer->NeedsVisualOverflowRecalc());

  // After unlocking, we should not need to do any extra work.
  lockable->classList().Remove(AtomicString("hidden"));
  EXPECT_FALSE(child_layer->NeedsVisualOverflowRecalc());

  UpdateAllLifecyclePhasesForTest();
}
TEST_P(DisplayLockContextRenderingTest,
       SelectionOnAnonymousColumnSpannerDoesNotCrash) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      #columns {
        column-count: 5;
      }
      #spanner {
        column-span: all;
      }
    </style>
    <div id="columns">
      <div id="spanner"></div>
    </div>
  )HTML");

  auto* columns_object =
      GetDocument().getElementById(AtomicString("columns"))->GetLayoutObject();
  LayoutObject* spanner_placeholder_object = nullptr;
  for (auto* candidate = columns_object->SlowFirstChild(); candidate;
       candidate = candidate->NextSibling()) {
    if (candidate->IsLayoutMultiColumnSpannerPlaceholder()) {
      spanner_placeholder_object = candidate;
      break;
    }
  }

  ASSERT_TRUE(spanner_placeholder_object);
  EXPECT_FALSE(spanner_placeholder_object->CanBeSelectionLeaf());
}

TEST_P(DisplayLockContextRenderingTest, ObjectsNeedingLayoutConsidersLocks) {
  SetHtmlInnerHTML(R"HTML(
    <div id=a>
      <div id=b>
        <div id=c></div>
        <div id=d></div>
      </div>
      <div id=e>
        <div id=f></div>
        <div id=g></div>
      </div>
    </div>
  )HTML");

  // Dirty all of the leaf nodes.
  auto dirty_all = [this]() {
    GetDocument()
        .getElementById(AtomicString("c"))
        ->GetLayoutObject()
        ->SetNeedsLayout("test");
    GetDocument()
        .getElementById(AtomicString("d"))
        ->GetLayoutObject()
        ->SetNeedsLayout("test");
    GetDocument()
        .getElementById(AtomicString("f"))
        ->GetLayoutObject()
        ->SetNeedsLayout("test");
    GetDocument()
        .getElementById(AtomicString("g"))
        ->GetLayoutObject()
        ->SetNeedsLayout("test");
  };

  unsigned dirty_count = 0;
  unsigned total_count = 0;
  bool is_subtree = false;

  dirty_all();
  GetDocument().View()->CountObjectsNeedingLayout(dirty_count, total_count,
                                                  is_subtree);
  // 7 divs + body + html + layout view
  EXPECT_EQ(dirty_count, 10u);
  EXPECT_EQ(total_count, 10u);

  GetDocument()
      .getElementById(AtomicString("e"))
      ->setAttribute(html_names::kStyleAttr,
                     AtomicString("content-visibility: hidden"));
  UpdateAllLifecyclePhasesForTest();

  // Note that the dirty_all call propagate the dirty bit from the unlocked
  // subtree all the way up to the layout view, so everything on the way up is
  // dirtied.
  dirty_all();
  GetDocument().View()->CountObjectsNeedingLayout(dirty_count, total_count,
                                                  is_subtree);
  // Element with 2 children is locked, and it itself isn't dirty (just the
  // children are). So, 10 - 3 = 7
  EXPECT_EQ(dirty_count, 7u);
  // We still see the locked element, so the total is 8.
  EXPECT_EQ(total_count, 8u);

  GetDocument()
      .getElementById(AtomicString("a"))
      ->setAttribute(html_names::kStyleAttr,
                     AtomicString("content-visibility: hidden"));
  UpdateAllLifecyclePhasesForTest();

  // Note that this dirty_all call is now not propagating the dirty bits at all,
  // since they are stopped at the top level div.
  dirty_all();
  GetDocument().View()->CountObjectsNeedingLayout(dirty_count, total_count,
                                                  is_subtree);
  // Top level element is locked and the dirty bits were not propagated, so we
  // expect 0 dirty elements. The total should be 4 ('a' + body + html + layout
  // view);
  EXPECT_EQ(dirty_count, 0u);
  EXPECT_EQ(total_count, 4u);
}

TEST_P(DisplayLockContextRenderingTest,
       PaintDirtyBitsNotPropagatedAcrossBoundary) {
  SetHtmlInnerHTML(R"HTML(
    <style>
    .locked { content-visibility: hidden; }
    div { contain: paint; }
    </style>
    <div id=parent>
      <div id=lockable>
        <div id=child>
          <div id=grandchild></div>
        </div>
      </div>
    </div>
  )HTML");

  auto* parent = GetDocument().getElementById(AtomicString("parent"));
  auto* lockable = GetDocument().getElementById(AtomicString("lockable"));
  auto* child = GetDocument().getElementById(AtomicString("child"));
  auto* grandchild = GetDocument().getElementById(AtomicString("grandchild"));

  auto* parent_box = parent->GetLayoutBoxModelObject();
  auto* lockable_box = lockable->GetLayoutBoxModelObject();
  auto* child_box = child->GetLayoutBoxModelObject();
  auto* grandchild_box = grandchild->GetLayoutBoxModelObject();

  ASSERT_TRUE(parent_box);
  ASSERT_TRUE(lockable_box);
  ASSERT_TRUE(child_box);
  ASSERT_TRUE(grandchild_box);

  ASSERT_TRUE(parent_box->HasSelfPaintingLayer());
  ASSERT_TRUE(lockable_box->HasSelfPaintingLayer());
  ASSERT_TRUE(child_box->HasSelfPaintingLayer());
  ASSERT_TRUE(grandchild_box->HasSelfPaintingLayer());

  auto* parent_layer = parent_box->Layer();
  auto* lockable_layer = lockable_box->Layer();
  auto* child_layer = child_box->Layer();
  auto* grandchild_layer = grandchild_box->Layer();

  EXPECT_FALSE(parent_layer->SelfOrDescendantNeedsRepaint());
  EXPECT_FALSE(lockable_layer->SelfOrDescendantNeedsRepaint());
  EXPECT_FALSE(child_layer->SelfOrDescendantNeedsRepaint());
  EXPECT_FALSE(grandchild_layer->SelfOrDescendantNeedsRepaint());

  lockable->classList().Add(AtomicString("locked"));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  // Lockable layer needs repainting after locking.
  EXPECT_FALSE(parent_layer->SelfNeedsRepaint());
  EXPECT_TRUE(lockable_layer->SelfNeedsRepaint());
  EXPECT_FALSE(child_layer->SelfNeedsRepaint());
  EXPECT_FALSE(grandchild_layer->SelfNeedsRepaint());

  // Breadcrumbs are set from the lockable layer.
  EXPECT_TRUE(parent_layer->DescendantNeedsRepaint());
  EXPECT_FALSE(lockable_layer->DescendantNeedsRepaint());
  EXPECT_FALSE(child_layer->DescendantNeedsRepaint());
  EXPECT_FALSE(grandchild_layer->DescendantNeedsRepaint());

  UpdateAllLifecyclePhasesForTest();

  // Everything is clean.
  EXPECT_FALSE(parent_layer->SelfNeedsRepaint());
  EXPECT_FALSE(lockable_layer->SelfNeedsRepaint());
  EXPECT_FALSE(child_layer->SelfNeedsRepaint());
  EXPECT_FALSE(grandchild_layer->SelfNeedsRepaint());

  // Breadcrumbs are clean as well.
  EXPECT_FALSE(parent_layer->DescendantNeedsRepaint());
  EXPECT_FALSE(lockable_layer->DescendantNeedsRepaint());
  EXPECT_FALSE(child_layer->DescendantNeedsRepaint());
  EXPECT_FALSE(grandchild_layer->DescendantNeedsRepaint());

  grandchild_layer->SetNeedsRepaint();

  // Grandchild needs repaint, so everything else should be clean.
  EXPECT_FALSE(parent_layer->SelfNeedsRepaint());
  EXPECT_FALSE(lockable_layer->SelfNeedsRepaint());
  EXPECT_FALSE(child_layer->SelfNeedsRepaint());
  EXPECT_TRUE(grandchild_layer->SelfNeedsRepaint());

  // Breadcrumbs are set from the lockable layer but are stopped at the locked
  // boundary.
  EXPECT_FALSE(parent_layer->DescendantNeedsRepaint());
  EXPECT_TRUE(lockable_layer->DescendantNeedsRepaint());
  EXPECT_TRUE(child_layer->DescendantNeedsRepaint());
  EXPECT_FALSE(grandchild_layer->DescendantNeedsRepaint());

  // Updating the lifecycle does not clean the dirty bits.
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(parent_layer->SelfNeedsRepaint());
  EXPECT_FALSE(lockable_layer->SelfNeedsRepaint());
  EXPECT_FALSE(child_layer->SelfNeedsRepaint());
  EXPECT_TRUE(grandchild_layer->SelfNeedsRepaint());

  EXPECT_FALSE(parent_layer->DescendantNeedsRepaint());
  EXPECT_TRUE(lockable_layer->DescendantNeedsRepaint());
  EXPECT_TRUE(child_layer->DescendantNeedsRepaint());
  EXPECT_FALSE(grandchild_layer->DescendantNeedsRepaint());

  // Unlocking causes lockable to repaint itself.
  lockable->classList().Remove(AtomicString("locked"));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  EXPECT_FALSE(parent_layer->SelfNeedsRepaint());
  EXPECT_TRUE(lockable_layer->SelfNeedsRepaint());
  EXPECT_FALSE(child_layer->SelfNeedsRepaint());
  EXPECT_TRUE(grandchild_layer->SelfNeedsRepaint());

  EXPECT_TRUE(parent_layer->DescendantNeedsRepaint());
  EXPECT_TRUE(lockable_layer->DescendantNeedsRepaint());
  EXPECT_TRUE(child_layer->DescendantNeedsRepaint());
  EXPECT_FALSE(grandchild_layer->DescendantNeedsRepaint());

  UpdateAllLifecyclePhasesForTest();

  // Everything should be clean.
  EXPECT_FALSE(parent_layer->SelfNeedsRepaint());
  EXPECT_FALSE(lockable_layer->SelfNeedsRepaint());
  EXPECT_FALSE(child_layer->SelfNeedsRepaint());
  EXPECT_FALSE(grandchild_layer->SelfNeedsRepaint());

  // Breadcrumbs are clean as well.
  EXPECT_FALSE(parent_layer->DescendantNeedsRepaint());
  EXPECT_FALSE(lockable_layer->DescendantNeedsRepaint());
  EXPECT_FALSE(child_layer->DescendantNeedsRepaint());
  EXPECT_FALSE(grandchild_layer->DescendantNeedsRepaint());
}

TEST_P(DisplayLockContextRenderingTest,
       NestedLockDoesNotInvalidateOnHideOrShow) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      .auto { content-visibility: auto; }
      .hidden { content-visibility: hidden; }
      .item { height: 10px; }
      /* this is important to not invalidate layout when we hide the element! */
      #outer { contain: style layout; }
    </style>
    <div id=outer>
      <div id=unrelated>
        <div id=inner class=auto>Content</div>
      </div>
    </div>
  )HTML");

  auto* inner_element = GetDocument().getElementById(AtomicString("inner"));
  auto* unrelated_element =
      GetDocument().getElementById(AtomicString("unrelated"));
  auto* outer_element = GetDocument().getElementById(AtomicString("outer"));

  // Ensure that the visibility switch happens. This would also clear the
  // layout.
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(outer_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(outer_element->GetLayoutObject()->SelfNeedsFullLayout());
  EXPECT_FALSE(unrelated_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(unrelated_element->GetLayoutObject()->SelfNeedsFullLayout());
  EXPECT_FALSE(inner_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(inner_element->GetLayoutObject()->SelfNeedsFullLayout());

  // Verify lock state.
  auto* inner_context = inner_element->GetDisplayLockContext();
  ASSERT_TRUE(inner_context);
  EXPECT_FALSE(inner_context->IsLocked());

  // Lock outer.
  outer_element->setAttribute(html_names::kClassAttr, AtomicString("hidden"));
  // Ensure the lock processes (but don't run intersection observation tasks
  // yet).
  UpdateAllLifecyclePhasesForTest();

  // Verify the lock exists.
  auto* outer_context = outer_element->GetDisplayLockContext();
  ASSERT_TRUE(outer_context);
  EXPECT_TRUE(outer_context->IsLocked());

  // Everything should be layout clean.
  EXPECT_FALSE(outer_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(outer_element->GetLayoutObject()->SelfNeedsFullLayout());
  EXPECT_FALSE(unrelated_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(unrelated_element->GetLayoutObject()->SelfNeedsFullLayout());
  EXPECT_FALSE(inner_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(inner_element->GetLayoutObject()->SelfNeedsFullLayout());

  // Inner context should not be observing the lifecycle.
  EXPECT_FALSE(IsObservingLifecycle(inner_context));

  // Process any visibility changes.
  RunStartOfLifecycleTasks();

  // Run the following checks a few times since we should be observing
  // lifecycle.
  for (int i = 0; i < 3; ++i) {
    // It shouldn't change the fact that we're layout clean.
    EXPECT_FALSE(outer_element->GetLayoutObject()->NeedsLayout());
    EXPECT_FALSE(outer_element->GetLayoutObject()->SelfNeedsFullLayout());
    EXPECT_FALSE(unrelated_element->GetLayoutObject()->NeedsLayout());
    EXPECT_FALSE(unrelated_element->GetLayoutObject()->SelfNeedsFullLayout());
    EXPECT_FALSE(inner_element->GetLayoutObject()->NeedsLayout());
    EXPECT_FALSE(inner_element->GetLayoutObject()->SelfNeedsFullLayout());

    // Because we skipped hiding the element, inner_context should be observing
    // lifecycle.
    EXPECT_TRUE(IsObservingLifecycle(inner_context));

    UpdateAllLifecyclePhasesForTest();
  }

  // Unlock outer.
  outer_element->setAttribute(html_names::kClassAttr, g_empty_atom);
  // Ensure the lock processes (but don't run intersection observation tasks
  // yet).
  UpdateAllLifecyclePhasesForTest();

  // Note that although we're not nested, we're still observing the lifecycle
  // because we don't yet know whether we should or should not hide and we only
  // make this decision _before_ the lifecycle actually unlocked outer.
  EXPECT_TRUE(IsObservingLifecycle(inner_context));

  // Verify the lock is gone.
  EXPECT_FALSE(outer_context->IsLocked());

  // Everything should be layout clean.
  EXPECT_FALSE(outer_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(outer_element->GetLayoutObject()->SelfNeedsFullLayout());
  EXPECT_FALSE(unrelated_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(unrelated_element->GetLayoutObject()->SelfNeedsFullLayout());
  EXPECT_FALSE(inner_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(inner_element->GetLayoutObject()->SelfNeedsFullLayout());

  // Process visibility changes.
  RunStartOfLifecycleTasks();

  // We now should know we're visible and so we're not observing the lifecycle.
  EXPECT_FALSE(IsObservingLifecycle(inner_context));

  // Also we should still be activated and unlocked.
  EXPECT_FALSE(inner_context->IsLocked());

  // Everything should be layout clean.
  EXPECT_FALSE(outer_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(outer_element->GetLayoutObject()->SelfNeedsFullLayout());
  EXPECT_FALSE(unrelated_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(unrelated_element->GetLayoutObject()->SelfNeedsFullLayout());
  EXPECT_FALSE(inner_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(inner_element->GetLayoutObject()->SelfNeedsFullLayout());
}

TEST_P(DisplayLockContextRenderingTest, NestedLockDoesHideWhenItIsOffscreen) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      .auto { content-visibility: auto; }
      .hidden { content-visibility: hidden; }
      .item { height: 10px; }
      /* this is important to not invalidate layout when we hide the element! */
      #outer { contain: style layout; }
      .spacer { height: 10000px; }
    </style>
    <div id=future_spacer></div>
    <div id=outer>
      <div id=unrelated>
        <div id=inner class=auto>Content</div>
      </div>
    </div>
  )HTML");

  auto* inner_element = GetDocument().getElementById(AtomicString("inner"));
  auto* unrelated_element =
      GetDocument().getElementById(AtomicString("unrelated"));
  auto* outer_element = GetDocument().getElementById(AtomicString("outer"));

  // Ensure that the visibility switch happens. This would also clear the
  // layout.
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(outer_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(outer_element->GetLayoutObject()->SelfNeedsFullLayout());
  EXPECT_FALSE(unrelated_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(unrelated_element->GetLayoutObject()->SelfNeedsFullLayout());
  EXPECT_FALSE(inner_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(inner_element->GetLayoutObject()->SelfNeedsFullLayout());

  // Verify lock state.
  auto* inner_context = inner_element->GetDisplayLockContext();
  ASSERT_TRUE(inner_context);
  EXPECT_FALSE(inner_context->IsLocked());

  // Lock outer.
  outer_element->setAttribute(html_names::kClassAttr, AtomicString("hidden"));
  // Ensure the lock processes (but don't run intersection observation tasks
  // yet).
  UpdateAllLifecyclePhasesForTest();

  // Verify the lock exists.
  auto* outer_context = outer_element->GetDisplayLockContext();
  ASSERT_TRUE(outer_context);
  EXPECT_TRUE(outer_context->IsLocked());

  // Everything should be layout clean.
  EXPECT_FALSE(outer_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(outer_element->GetLayoutObject()->SelfNeedsFullLayout());
  EXPECT_FALSE(unrelated_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(unrelated_element->GetLayoutObject()->SelfNeedsFullLayout());
  EXPECT_FALSE(inner_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(inner_element->GetLayoutObject()->SelfNeedsFullLayout());

  // Inner context should not be observing the lifecycle.
  EXPECT_FALSE(IsObservingLifecycle(inner_context));

  // Process any visibility changes.
  RunStartOfLifecycleTasks();

  // It shouldn't change the fact that we're layout clean.
  EXPECT_FALSE(outer_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(outer_element->GetLayoutObject()->SelfNeedsFullLayout());
  EXPECT_FALSE(unrelated_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(unrelated_element->GetLayoutObject()->SelfNeedsFullLayout());
  EXPECT_FALSE(inner_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(inner_element->GetLayoutObject()->SelfNeedsFullLayout());

  // Let future spacer become a real spacer!
  GetDocument()
      .getElementById(AtomicString("future_spacer"))
      ->setAttribute(html_names::kClassAttr, AtomicString("spacer"));

  UpdateAllLifecyclePhasesForTest();

  // Because we skipped hiding the element, inner_context should be observing
  // lifecycle.
  EXPECT_TRUE(IsObservingLifecycle(inner_context));

  // Unlock outer.
  outer_element->setAttribute(html_names::kClassAttr, g_empty_atom);
  // Ensure the lock processes (but don't run intersection observation tasks
  // yet).
  UpdateAllLifecyclePhasesForTest();

  // Note that although we're not nested, we're still observing the lifecycle
  // because we don't yet know whether we should or should not hide and we only
  // make this decision _before_ the lifecycle actually unlocked outer.
  EXPECT_TRUE(IsObservingLifecycle(inner_context));

  // Verify the lock is gone.
  EXPECT_FALSE(outer_context->IsLocked());

  // Everything should be layout clean.
  EXPECT_FALSE(outer_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(outer_element->GetLayoutObject()->SelfNeedsFullLayout());
  EXPECT_FALSE(unrelated_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(unrelated_element->GetLayoutObject()->SelfNeedsFullLayout());
  EXPECT_FALSE(inner_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(inner_element->GetLayoutObject()->SelfNeedsFullLayout());

  // Process any visibility changes.
  RunStartOfLifecycleTasks();

  // We're still invisible, and we don't know that we're not nested so we're
  // still observing the lifecycle.
  EXPECT_TRUE(IsObservingLifecycle(inner_context));

  // We're unlocked for now.
  EXPECT_FALSE(inner_context->IsLocked());

  // Everything should be layout clean.
  EXPECT_FALSE(outer_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(outer_element->GetLayoutObject()->SelfNeedsFullLayout());
  EXPECT_FALSE(unrelated_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(unrelated_element->GetLayoutObject()->SelfNeedsFullLayout());
  EXPECT_FALSE(inner_element->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(inner_element->GetLayoutObject()->SelfNeedsFullLayout());

  UpdateAllLifecyclePhasesForTest();

  // We figured out that we're actually invisible so no need to observe the
  // lifecycle.
  EXPECT_FALSE(IsObservingLifecycle(inner_context));

  // We're locked.
  EXPECT_TRUE(inner_context->IsLocked());
}

TEST_P(DisplayLockContextRenderingTest,
       LockedCanvasWithFallbackHasFocusableStyle) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      .auto { content-visibility: auto; }
      .spacer { height: 3000px; }
    </style>
    <div class=spacer></div>
    <div class=auto>
      <canvas>
        <div id=target tabindex=0></div>
      </canvas>
    </div>
  )HTML");

  auto* target = GetDocument().getElementById(AtomicString("target"));
  EXPECT_TRUE(target->IsFocusable());
}

TEST_P(DisplayLockContextRenderingTest, ForcedUnlockBookkeeping) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      .hidden { content-visibility: hidden; }
      .inline { display: inline; }
    </style>
    <div id=target class=hidden></div>
  )HTML");

  auto* target = GetDocument().getElementById(AtomicString("target"));
  auto* context = target->GetDisplayLockContext();

  ASSERT_TRUE(context);
  EXPECT_TRUE(context->IsLocked());
  EXPECT_EQ(
      GetDocument().GetDisplayLockDocumentState().LockedDisplayLockCount(), 1);

  target->classList().Add(AtomicString("inline"));
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(context->IsLocked());
  EXPECT_EQ(
      GetDocument().GetDisplayLockDocumentState().LockedDisplayLockCount(), 0);
}

TEST_P(DisplayLockContextRenderingTest, LayoutRootIsSkippedIfLocked) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      .hidden { content-visibility: hidden; }
      .contained { contain: strict; }
      .positioned { position: absolute; top: 0; left: 0; }
    </style>
    <div id=hide>
      <div class=contained>
        <div id=new_parent class="contained positioned">
          <div>
            <div id=target></div>
          </div>
        </div>
      </div>
    </div>
  )HTML");

  // Lock an ancestor.
  auto* hide = GetDocument().getElementById(AtomicString("hide"));
  hide->classList().Add(AtomicString("hidden"));
  UpdateAllLifecyclePhasesForTest();

  auto* target = GetDocument().getElementById(AtomicString("target"));
  auto* new_parent = GetDocument().getElementById(AtomicString("new_parent"));

  // Reparent elements which will invalidate layout without needing to process
  // style (which is blocked by the display-lock).
  new_parent->appendChild(target);

  // Note that we don't check target here, since it doesn't have a layout object
  // after being re-parented.
  EXPECT_TRUE(new_parent->GetLayoutObject()->NeedsLayout());

  // Updating the lifecycle should not update new_parent, since it is in a
  // locked subtree.
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(new_parent->GetLayoutObject()->NeedsLayout());

  // Unlocking and updating should update everything.
  hide->classList().Remove(AtomicString("hidden"));
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(hide->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(target->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(new_parent->GetLayoutObject()->NeedsLayout());
}

TEST_P(DisplayLockContextRenderingTest,
       LayoutRootIsProcessedIfLockedAndForced) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      .hidden { content-visibility: hidden; }
      .contained { contain: strict; }
      .positioned { position: absolute; top: 0; left: 0; }
    </style>
    <div id=hide>
      <div class=contained>
        <div id=new_parent class="contained positioned">
          <div>
            <div id=target></div>
          </div>
        </div>
      </div>
    </div>
  )HTML");

  // Lock an ancestor.
  auto* hide = GetDocument().getElementById(AtomicString("hide"));
  hide->classList().Add(AtomicString("hidden"));
  UpdateAllLifecyclePhasesForTest();

  auto* target = GetDocument().getElementById(AtomicString("target"));
  auto* new_parent = GetDocument().getElementById(AtomicString("new_parent"));

  // Reparent elements which will invalidate layout without needing to process
  // style (which is blocked by the display-lock).
  new_parent->appendChild(target);

  // Note that we don't check target here, since it doesn't have a layout object
  // after being re-parented.
  EXPECT_TRUE(new_parent->GetLayoutObject()->NeedsLayout());

  {
    auto scope =
        GetScopedForcedUpdate(hide, DisplayLockContext::ForcedPhase::kLayout,
                              true /* include self */);

    // Updating the lifecycle should update target and new_parent, since it is
    // in a locked but forced subtree.
    UpdateAllLifecyclePhasesForTest();
    EXPECT_FALSE(target->GetLayoutObject()->NeedsLayout());
    EXPECT_FALSE(new_parent->GetLayoutObject()->NeedsLayout());
  }

  // Unlocking and updating should update everything.
  hide->classList().Remove(AtomicString("hidden"));
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(hide->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(target->GetLayoutObject()->NeedsLayout());
  EXPECT_FALSE(new_parent->GetLayoutObject()->NeedsLayout());
}

TEST_P(DisplayLockContextRenderingTest, ContainStrictChild) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      .hidden { content-visibility: hidden; }
      .contained { contain: strict; }
      #target { backface-visibility: hidden; }
    </style>
    <div id=hide>
      <div id=container class=contained>
        <div id=target></div>
      </div>
    </div>
  )HTML");

  // Lock an ancestor.
  auto* hide = GetDocument().getElementById(AtomicString("hide"));
  hide->classList().Add(AtomicString("hidden"));

  // This should not DCHECK.
  UpdateAllLifecyclePhasesForTest();

  hide->classList().Remove(AtomicString("hidden"));
  UpdateAllLifecyclePhasesForTest();
}

TEST_P(DisplayLockContextRenderingTest, UseCounter) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      .auto { content-visibility: auto; }
      .hidden { content-visibility: hidden; }
    </style>
    <div id=e1></div>
    <div id=e2></div>
  )HTML");

  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kContentVisibilityAuto));
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kContentVisibilityHidden));

  GetDocument()
      .getElementById(AtomicString("e1"))
      ->classList()
      .Add(AtomicString("auto"));
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kContentVisibilityAuto));
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kContentVisibilityHidden));

  GetDocument()
      .getElementById(AtomicString("e2"))
      ->classList()
      .Add(AtomicString("hidden"));
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kContentVisibilityAuto));
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kContentVisibilityHidden));
}

TEST_P(DisplayLockContextRenderingTest,
       NeedsLayoutTreeUpdateForNodeRespectsForcedLocks) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      .hidden { content-visibility: hidden; }
      .contained { contain: strict; }
      .backface_hidden { backface-visibility: hidden; }
    </style>
    <div id=hide>
      <div id=container class=contained>
        <div id=target></div>
      </div>
    </div>
  )HTML");

  // Lock an ancestor.
  auto* hide = GetDocument().getElementById(AtomicString("hide"));
  hide->classList().Add(AtomicString("hidden"));
  UpdateAllLifecyclePhasesForTest();

  auto* target = GetDocument().getElementById(AtomicString("target"));
  target->classList().Add(AtomicString("backface_hidden"));

  auto scope =
      GetScopedForcedUpdate(hide, DisplayLockContext::ForcedPhase::kPrePaint,
                            true /* include self */);
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdateForNode(*target));
}

TEST_P(DisplayLockContextRenderingTest, InnerScrollerAutoVisibilityMargin) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      .auto { content-visibility: auto; }
      #scroller { height: 300px; overflow: scroll }
      #target { height: 10px; width: 10px; }
      .spacer { height: 3000px }
    </style>
    <div id=scroller>
      <div class=spacer></div>
      <div id=target class=auto></div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  auto* target = GetDocument().getElementById(AtomicString("target"));
  ASSERT_TRUE(target->GetDisplayLockContext());
  EXPECT_TRUE(target->GetDisplayLockContext()->IsLocked());

  auto* scroller = GetDocument().getElementById(AtomicString("scroller"));
  // 2600 is spacer (3000) minus scroller height (300) minus 100 for some extra
  // padding.
  scroller->setScrollTop(2600);
  UpdateAllLifecyclePhasesForTest();

  // Since the intersection observation is delivered on the next frame, run
  // another lifecycle.
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(target->GetDisplayLockContext()->IsLocked());
}

TEST_P(DisplayLockContextRenderingTest,
       AutoReachesStableStateOnContentSmallerThanLockedSize) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      .spacer { height: 20000px; }
      .auto {
        content-visibility: auto;
        contain-intrinsic-size: 1px 20000px;
      }
      .auto > div {
        height: 3000px;
      }
    </style>

    <div class=spacer></div>
    <div id=e1 class=auto><div>content</div></div>
    <div id=e2 class=auto><div>content</div></div>
    <div class=spacer></div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  GetDocument().scrollingElement()->setScrollTop(29000);

  Element* element = GetDocument().getElementById(AtomicString("e1"));

  // Note that this test also unlock/relocks #e2 but we only care about #e1
  // settling into a steady state.

  // Initially we start with locked in the viewport.
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(element->GetDisplayLockContext()->IsLocked());
  EXPECT_EQ(GetDocument().scrollingElement()->scrollTop(), 29000.);

  // It gets unlocked because it's in the viewport.
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(element->GetDisplayLockContext()->IsLocked());
  EXPECT_EQ(GetDocument().scrollingElement()->scrollTop(), 29000.);

  // By unlocking it, it shrinks so next time it gets relocked.
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(element->GetDisplayLockContext()->IsLocked());
  EXPECT_EQ(GetDocument().scrollingElement()->scrollTop(), 29000.);

  // However, because c-v auto implies c-i-s auto when relocking it doesn't
  // grow anymore and this is a stable state.
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(element->GetDisplayLockContext()->IsLocked());
  EXPECT_EQ(GetDocument().scrollingElement()->scrollTop(), 29000.);
}

TEST_P(DisplayLockContextRenderingTest,
       AutoReachesStableStateOnContentSmallerThanLockedSizeInLtr) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      body { writing-mode: vertical-lr }
      .spacer { block-size: 20000px; }
      .auto {
        content-visibility: auto;
        contain-intrinsic-size: 20000px 1px;
      }
      .auto > div {
        block-size: 3000px;
      }
    </style>

    <div class=spacer></div>
    <div id=e1 class=auto><div>content</div></div>
    <div id=e2 class=auto><div>content</div></div>
    <div class=spacer></div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  GetDocument().scrollingElement()->setScrollLeft(29000);

  Element* element = GetDocument().getElementById(AtomicString("e1"));

  // Note that this test also unlock/relocks #e2 but we only care about #e1
  // settling into a steady state.

  // Initially we start with locked in the viewport.
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(element->GetDisplayLockContext()->IsLocked());
  EXPECT_EQ(GetDocument().scrollingElement()->scrollLeft(), 29000.);

  // It gets unlocked because it's in the viewport.
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(element->GetDisplayLockContext()->IsLocked());
  EXPECT_EQ(GetDocument().scrollingElement()->scrollLeft(), 29000.);

  // By unlocking it, it shrinks so next time it gets relocked.
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(element->GetDisplayLockContext()->IsLocked());
  EXPECT_EQ(GetDocument().scrollingElement()->scrollLeft(), 29000.);

  // Because c-v auto implies c-i-s auto, the element doesn't grow again so this
  // is a stable state.
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(element->GetDisplayLockContext()->IsLocked());
  EXPECT_EQ(GetDocument().scrollingElement()->scrollLeft(), 29000.);
}

TEST_P(DisplayLockContextRenderingTest,
       AutoReachesStableStateOnContentSmallerThanLockedSizeInRtl) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      body { writing-mode: vertical-rl }
      .spacer { block-size: 20000px; }
      .auto {
        content-visibility: auto;
        contain-intrinsic-size: 20000px 1px;
      }
      .auto > div {
        block-size: 3000px;
      }
    </style>

    <div class=spacer></div>
    <div id=e1 class=auto><div>content</div></div>
    <div id=e2 class=auto><div>content</div></div>
    <div class=spacer></div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  GetDocument().scrollingElement()->setScrollLeft(-29000);

  Element* element = GetDocument().getElementById(AtomicString("e1"));

  // Note that this test also unlock/relocks #e2 but we only care about #e1
  // settling into a steady state.

  // Initially we start with locked in the viewport.
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(element->GetDisplayLockContext()->IsLocked());
  EXPECT_EQ(GetDocument().scrollingElement()->scrollLeft(), -29000.);

  // It gets unlocked because it's in the viewport.
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(element->GetDisplayLockContext()->IsLocked());
  EXPECT_EQ(GetDocument().scrollingElement()->scrollLeft(), -29000.);

  // By unlocking it, it shrinks so next time it gets relocked.
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(element->GetDisplayLockContext()->IsLocked());
  EXPECT_EQ(GetDocument().scrollingElement()->scrollLeft(), -29000.);

  // Because c-v auto implies c-i-s auto, this is a stable state.
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(element->GetDisplayLockContext()->IsLocked());
  EXPECT_EQ(GetDocument().scrollingElement()->scrollLeft(), -29000.);
}

TEST_P(DisplayLockContextRenderingTest, FirstAutoFramePaintsInViewport) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      .spacer { height: 10000px }
      .auto {
        content-visibility: auto;
        contain-intrinsic-size: 1px 200px;
      }
      .auto > div { height: 100px }
    </style>

    <div id=visible><div>content</div></div>
    <div class=spacer></div>
    <div id=hidden><div>content</div></div>
  )HTML");

  auto* visible = GetDocument().getElementById(AtomicString("visible"));
  auto* hidden = GetDocument().getElementById(AtomicString("hidden"));

  visible->classList().Add(AtomicString("auto"));
  hidden->classList().Add(AtomicString("auto"));

  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(visible->GetDisplayLockContext()->IsLocked());
  EXPECT_TRUE(hidden->GetDisplayLockContext()->IsLocked());

  EXPECT_FALSE(visible->GetLayoutObject()->SelfNeedsFullLayout());
  EXPECT_FALSE(hidden->GetLayoutObject()->SelfNeedsFullLayout());

  auto* visible_rect = visible->GetBoundingClientRect();
  auto* hidden_rect = hidden->GetBoundingClientRect();

  EXPECT_FLOAT_EQ(visible_rect->height(), 100);
  EXPECT_FLOAT_EQ(hidden_rect->height(), 200);
}

TEST_P(DisplayLockContextRenderingTest,
       HadIntersectionNotificationsResetsWhenConnected) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      .auto { content-visibility: auto; }
    </style>
    <div id=target class=auto></div>
  )HTML");

  auto* element = GetDocument().getElementById(AtomicString("target"));
  auto* context = element->GetDisplayLockContext();
  ASSERT_TRUE(context);
  test::RunPendingTasks();

  EXPECT_TRUE(context->HadAnyViewportIntersectionNotifications());

  element->remove();
  GetDocument().body()->AppendChild(element);

  EXPECT_FALSE(context->HadAnyViewportIntersectionNotifications());

  UpdateAllLifecyclePhasesForTest();
  test::RunPendingTasks();

  EXPECT_TRUE(context->HadAnyViewportIntersectionNotifications());
}

TEST_P(DisplayLockContextTest, PrintingUnlocksAutoLocks) {
  ResizeAndFocus();

  SetHtmlInnerHTML(R"HTML(
    <style>
    .spacer { height: 30000px; }
    .auto { content-visibility: auto; }
    </style>
    <div class=spacer></div>
    <div id=target class=auto>
      <div id=nested class=auto></div>
    </div>
  )HTML");

  auto* target = GetDocument().getElementById(AtomicString("target"));
  auto* nested = GetDocument().getElementById(AtomicString("nested"));
  ASSERT_TRUE(target->GetDisplayLockContext());
  EXPECT_TRUE(target->GetDisplayLockContext()->IsLocked());
  // Nested should not have a display lock since we would have skipped style.
  EXPECT_FALSE(nested->GetDisplayLockContext());

  {
    // Create a paint preview scope.
    Document::PaintPreviewScope scope(GetDocument(),
                                      Document::kPaintingPreview);
    UpdateAllLifecyclePhasesForTest();

    EXPECT_FALSE(target->GetDisplayLockContext()->IsLocked());
    // Nested should have created a context...
    ASSERT_TRUE(nested->GetDisplayLockContext());
    // ... but it should be unlocked.
    EXPECT_FALSE(nested->GetDisplayLockContext()->IsLocked());
  }

  EXPECT_TRUE(target->GetDisplayLockContext()->IsLocked());
  EXPECT_TRUE(nested->GetDisplayLockContext()->IsLocked());
}

TEST_P(DisplayLockContextTest, CullRectUpdate) {
  ResizeAndFocus();
  SetHtmlInnerHTML(R"HTML(
    <style>
    #clip {
      width: 100px;
      height: 100px;
      overflow: hidden;
    }
    #container {
      width: 300px;
      height: 300px;
      contain: paint layout;
    }
    .locked {
      content-visibility: hidden;
    }
    </style>
    <div id="clip">
      <div id="container"
           style="width: 300px; height: 300px; contain: paint layout">
        <div id="target" style="position: relative"></div>
      </div>
    </div>
  )HTML");

  // Check if the result is correct if we update the contents.
  auto* container = GetDocument().getElementById(AtomicString("container"));
  auto* target =
      GetDocument().getElementById(AtomicString("target"))->GetLayoutBox();
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100),
            target->FirstFragment().GetCullRect().Rect());

  container->classList().Add(AtomicString("locked"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100),
            target->FirstFragment().GetCullRect().Rect());

  GetDocument()
      .getElementById(AtomicString("clip"))
      ->setAttribute(html_names::kStyleAttr, AtomicString("width: 200px"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100),
            target->FirstFragment().GetCullRect().Rect());

  container->classList().Remove(AtomicString("locked"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(gfx::Rect(0, 0, 200, 100),
            target->FirstFragment().GetCullRect().Rect());
}

TEST_P(DisplayLockContextTest, DisconnectedElementIsUnlocked) {
  ResizeAndFocus();
  SetHtmlInnerHTML(R"HTML(
    <style>
    .locked { content-visibility: hidden; }
    </style>
    <div id="container" class="locked"></div>
  )HTML");

  // Check if the result is correct if we update the contents.
  auto* container = GetDocument().getElementById(AtomicString("container"));
  auto* context = container->GetDisplayLockContext();
  ASSERT_TRUE(context);
  EXPECT_TRUE(context->IsLocked());
  EXPECT_EQ(context->GetState(), EContentVisibility::kHidden);

  container->remove();

  EXPECT_FALSE(container->GetComputedStyle());
  EXPECT_FALSE(context->IsLocked());
  EXPECT_EQ(context->GetState(), EContentVisibility::kVisible);
}

TEST_P(DisplayLockContextTest, ConnectedElementDefersSubtreeChecks) {
  ResizeAndFocus();
  SetHtmlInnerHTML(R"HTML(
    <style>
    .spacer { height: 3000px; }
    .locked { content-visibility: auto; }
    </style>
    <div id="s1" class="spacer">first spacer</div>
    <div id="s2" class="spacer">second spacer</div>
    <div id="locked" class="locked">locked container</div>
  )HTML");

  auto* locked = GetDocument().getElementById(AtomicString("locked"));
  auto* context = locked->GetDisplayLockContext();
  ASSERT_TRUE(context);
  EXPECT_TRUE(context->IsLocked());

  auto* range = GetDocument().createRange();
  range->setStart(
      GetDocument().getElementById(AtomicString("s1"))->firstChild(), 0);
  range->setEnd(GetDocument().getElementById(AtomicString("s2"))->firstChild(),
                5);

  Selection().SetSelection(SelectionInDOMTree::Builder()
                               .SetBaseAndExtent(EphemeralRange(range))
                               .Build(),
                           SetSelectionOptions());

  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(HasSelection(context));

  GetDocument().body()->insertBefore(
      locked, GetDocument().getElementById(AtomicString("s2")));

  EXPECT_FALSE(HasSelection(context));

  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(HasSelection(context));
}

TEST_P(DisplayLockContextTest, BlockedReattachOfSlotted) {
  GetDocument().body()->setHTMLUnsafe(R"HTML(
    <div id="host">
      <template shadowrootmode="open">
        <style>
          slot { display: block; }
          .locked {
            content-visibility: hidden;
          }
        </style>
        <slot id="slot"></slot>
      </template>
      <span id="slotted"></span>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* host = GetDocument().getElementById(AtomicString("host"));
  auto* slotted = GetDocument().getElementById(AtomicString("slotted"));
  auto* slot = host->GetShadowRoot()->getElementById(AtomicString("slot"));

  EXPECT_TRUE(slot->GetLayoutObject());

  slot->classList().Add(AtomicString("locked"));
  GetDocument().documentElement()->SetForceReattachLayoutTree();
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(slotted->GetLayoutObject());
}

TEST_P(DisplayLockContextTest, BlockedReattachOfShadowTree) {
  GetDocument().body()->setHTMLUnsafe(R"HTML(
    <style>
      .locked { content-visibility: hidden; }
    </style>
    <div id="host">
      <template shadowrootmode="open">
        <span id="span"></span>
      </template>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* host = GetDocument().getElementById(AtomicString("host"));
  auto* span = host->GetShadowRoot()->getElementById(AtomicString("span"));

  ASSERT_TRUE(host->GetLayoutObject());
  EXPECT_TRUE(span->GetLayoutObject());

  host->classList().Add(AtomicString("locked"));
  GetDocument().documentElement()->SetForceReattachLayoutTree();
  UpdateAllLifecyclePhasesForTest();

  ASSERT_TRUE(host->GetLayoutObject());
  EXPECT_FALSE(span->GetLayoutObject());
}

TEST_P(DisplayLockContextTest, BlockedReattachOfPseudoElements) {
  GetDocument().documentElement()->setInnerHTML(R"HTML(
    <style>
      #locked::before { content: "X"; }
      .locked { content-visibility: hidden; }
    </style>
    <div id="locked"></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* locked = GetDocument().getElementById(AtomicString("locked"));

  ASSERT_TRUE(locked->GetLayoutObject());
  ASSERT_TRUE(locked->GetPseudoElement(kPseudoIdBefore));
  EXPECT_TRUE(locked->GetPseudoElement(kPseudoIdBefore)->GetLayoutObject());

  locked->classList().Add(AtomicString("locked"));
  GetDocument().documentElement()->SetForceReattachLayoutTree();
  UpdateAllLifecyclePhasesForTest();

  ASSERT_TRUE(locked->GetLayoutObject());
  ASSERT_TRUE(locked->GetPseudoElement(kPseudoIdBefore));
  EXPECT_FALSE(locked->GetPseudoElement(kPseudoIdBefore)->GetLayoutObject());
}

TEST_P(DisplayLockContextTest, BlockedReattachWhitespaceSibling) {
  GetDocument().documentElement()->setInnerHTML(R"HTML(
    <style>
      #locked { display: inline-block; }
      .locked { content-visibility: hidden; }
    </style>
    <span id="locked"><span>X</span></span> <span>X</span>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* locked = GetDocument().getElementById(AtomicString("locked"));

  EXPECT_TRUE(locked->GetLayoutObject());
  EXPECT_TRUE(locked->firstChild()->GetLayoutObject());
  EXPECT_TRUE(locked->firstChild()->firstChild()->GetLayoutObject());
  EXPECT_TRUE(locked->nextSibling()->GetLayoutObject());
  EXPECT_TRUE(locked->nextSibling()->nextSibling()->GetLayoutObject());

  locked->classList().Add(AtomicString("locked"));
  GetDocument().documentElement()->SetForceReattachLayoutTree();
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(locked->GetLayoutObject());
  EXPECT_FALSE(locked->firstChild()->GetLayoutObject());
  EXPECT_FALSE(locked->firstChild()->firstChild()->GetLayoutObject());
  EXPECT_TRUE(locked->nextSibling()->GetLayoutObject());
  EXPECT_TRUE(locked->nextSibling()->nextSibling()->GetLayoutObject());
}

TEST_P(DisplayLockContextTest, ReattachPropagationBlockedByDisplayLock) {
  GetDocument().documentElement()->setInnerHTML(R"HTML(
    <style>
      #locked { content-visibility: hidden; }
    </style>
    <div id=parent>
      <div id=locked>
        <div id=child>
          <div id=grandchild></div>
        </div>
      </div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  auto* locked = GetDocument().getElementById(AtomicString("locked"));
  auto* grandchild = GetDocument().getElementById(AtomicString("grandchild"));
  auto* parent = GetDocument().getElementById(AtomicString("parent"));

  // Force update all layout objects
  grandchild->GetBoundingClientRect();

  ASSERT_TRUE(locked->GetLayoutObject());
  ASSERT_TRUE(grandchild->GetLayoutObject());
  ASSERT_TRUE(parent->GetLayoutObject());

  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);
  grandchild->SetNeedsReattachLayoutTree();
  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kStyleClean);

  EXPECT_TRUE(locked->ChildNeedsReattachLayoutTree());
  EXPECT_TRUE(grandchild->NeedsReattachLayoutTree());
  EXPECT_FALSE(parent->ChildNeedsReattachLayoutTree());

  EXPECT_FALSE(GetDocument().GetStyleEngine().NeedsLayoutTreeRebuild());

  auto scope = GetScopedForcedUpdate(
      grandchild, DisplayLockContext::ForcedPhase::kStyleAndLayoutTree);
  // Pretend we styled the children.
  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);
  locked->GetDisplayLockContext()->DidStyleChildren();
  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kStyleClean);

  EXPECT_TRUE(locked->ChildNeedsReattachLayoutTree());
  EXPECT_TRUE(grandchild->NeedsReattachLayoutTree());
  EXPECT_TRUE(parent->ChildNeedsReattachLayoutTree());

  EXPECT_TRUE(GetDocument().GetStyleEngine().NeedsLayoutTreeRebuild());
}

TEST_P(DisplayLockContextTest, NoUpdatesInDisplayNone) {
  GetDocument().documentElement()->setInnerHTML(R"HTML(
    <div id=displaynone style="display:none">
      <div id=displaylocked style="content-visibility:hidden">
        <div id=child>hello</div>
      </div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  auto* displaylocked =
      GetDocument().getElementById(AtomicString("displaylocked"));
  auto* child = GetDocument().getElementById(AtomicString("child"));

  EXPECT_FALSE(displaylocked->GetComputedStyle());
  EXPECT_FALSE(displaylocked->GetLayoutObject());
  EXPECT_FALSE(child->GetComputedStyle());
  EXPECT_FALSE(child->GetLayoutObject());

  // EnsureComputedStyle shouldn't lock elements in a display:none subtree, and
  // certainly shouldn't run layout.
  displaylocked->EnsureComputedStyle();
  child->EnsureComputedStyle();
  EXPECT_FALSE(displaylocked->GetDisplayLockContext());
  EXPECT_FALSE(displaylocked->GetLayoutObject());
  EXPECT_FALSE(child->GetLayoutObject());
}

TEST_P(DisplayLockContextTest, ElementActivateDisplayLockIfNeeded) {
  SetHtmlInnerHTML(R"HTML(
    <div style="height: 10000px"></div>
    <div style="content-visibility: hidden" hidden="until-found"></div>
    <div style="content-visibility: auto"><div id="target"></div></div>
  )HTML");

  auto* target = GetDocument().getElementById(AtomicString("target"));
  // Non-ancestor c-v:hidden should not prevent the activation.
  EXPECT_TRUE(target->ActivateDisplayLockIfNeeded(
      DisplayLockActivationReason::kScrollIntoView));
}

TEST_P(DisplayLockContextTest, ShouldForceUnlockObjectWithFallbackContent) {
  SetHtmlInnerHTML(R"HTML(
    <div style="height: 10000px"></div>
    <object style="content-visibility: auto" id="target">foo bar</object>
  )HTML");

  // The <object> should should be lockable after the initial layout.
  UpdateAllLifecyclePhasesForTest();
  auto* target = To<HTMLPlugInElement>(
      GetDocument().getElementById(AtomicString("target")));
  EXPECT_TRUE(target->GetDisplayLockContext());
  EXPECT_TRUE(target->GetDisplayLockContext()->IsLocked());

  // UpdatePlugin() makes the <object> UseFallbackContent() state, and
  // invalidates its style.
  ASSERT_TRUE(target->NeedsPluginUpdate());
  target->UpdatePlugin();
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(target->GetDisplayLockContext()->IsLocked());
}

}  // namespace blink
