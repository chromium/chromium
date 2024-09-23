// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/json/json_reader.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/shell/common/shell_switches.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-test-utils.h"
#include "third_party/blink/public/mojom/scroll/scroll_into_view_params.mojom.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/events/event_constants.h"
#include "url/gurl.h"

#if defined(USE_AURA)
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#endif

#define EXPECT_TRUE_OR_FAIL(condition) \
  EXPECT_TRUE(condition);              \
  if (!condition)                      \
    return false;

namespace content {

namespace {

// Test variants

// kLocalFrame will force all remote frames in a test to be local.
enum TestFrameType { kLocalFrame, kRemoteFrame };

// Tests run with both Left-to-Right and Right-to-Left writing modes.
enum TestWritingMode { kLTR, kRTL };

// What kind of scroll into view to invoke, via JavaScript binding
// (element.scrollIntoView), using the InputHandler
// ScrollFocusedEditableNodeIntoView method, or via setting an OSK inset.
enum TestInvokeMethod { kJavaScript, kInputHandler, kAuraOnScreenKeyboard };

[[maybe_unused]] std::string DescribeFrameType(
    const testing::TestParamInfo<TestFrameType>& info) {
  std::string frame_type;
  switch (info.param) {
    case kLocalFrame: {
      frame_type = "LocalFrame";
    } break;
    case kRemoteFrame: {
      frame_type = "RemoteFrame";
    } break;
  }
  return frame_type;
}

blink::mojom::FrameWidgetInputHandler* GetInputHandler(FrameTreeNode* node) {
  return node->current_frame_host()
      ->GetRenderWidgetHost()
      ->GetFrameWidgetInputHandler();
}

// Will block from the destructor until a ScrollFocusedEditableNodeIntoView has
// completed. This must be called with the root frame tree node since that's
// where the ScrollIntoView and PageScaleAnimation will bubble to.
class ScopedFocusScrollWaiter {
 public:
  explicit ScopedFocusScrollWaiter(FrameTreeNode* node) {
    DCHECK(node->IsOutermostMainFrame());
    GetInputHandler(node)->WaitForPageScaleAnimationForTesting(
        run_loop_.QuitClosure());
  }

  ~ScopedFocusScrollWaiter() { run_loop_.Run(); }

 private:
  base::RunLoop run_loop_;
};

// While this is in scope, causes the TextInputManager of the given WebContents
// to always return nullptr. This effectively blocks the IME from receiving any
// events from the renderer. Note: RenderWidgetHostViewBase caches this value
// so for this to work it must be constructed before the target page is
// constructed.
class ScopedSuppressImeEvents {
 public:
  explicit ScopedSuppressImeEvents(WebContentsImpl* web_contents)
      : web_contents_(web_contents->GetWeakPtr()) {
    web_contents->set_suppress_ime_events_for_testing(true);
  }

  ~ScopedSuppressImeEvents() {
    if (!web_contents_)
      return;

    static_cast<WebContentsImpl*>(web_contents_.get())
        ->set_suppress_ime_events_for_testing(false);
  }

  base::WeakPtr<WebContents> web_contents_;
};

// Interceptor that can be used to verify calls to
// ScrollRectToVisibleInParentFrame on the LocalFrameHost interface.
class ScrollRectToVisibleInParentFrameInterceptor
    : public blink::mojom::LocalFrameHostInterceptorForTesting {
 public:
  ScrollRectToVisibleInParentFrameInterceptor() = default;
  ~ScrollRectToVisibleInParentFrameInterceptor() override = default;

  void Init(RenderFrameHostImpl* render_frame_host) {
    render_frame_host_ = render_frame_host;
    std::ignore = render_frame_host_->local_frame_host_receiver_for_testing()
                      .SwapImplForTesting(this);
  }

  blink::mojom::LocalFrameHost* GetForwardingInterface() override {
    return render_frame_host_;
  }

  void ScrollRectToVisibleInParentFrame(
      const gfx::RectF& rect_to_scroll,
      blink::mojom::ScrollIntoViewParamsPtr params) override {
    has_called_method_ = true;
  }

  bool HasCalledScrollRectToVisibleInParentFrame() const {
    return has_called_method_;
  }

 private:
  raw_ptr<RenderFrameHostImpl> render_frame_host_;
  bool has_called_method_ = false;
};

// Test harness for ScrollIntoView related browser tests. These tests are
// mainly concerned with behavior of scroll into view related functionality
// across remote frames. This harness depends on
// cross_site_scroll_into_view_factory.html, which is based on
// cross_site_iframe_factory.html.
//
// cross_site_scroll_into_view_factory.html builds a frame tree from its given
// argument, allowing only a single child frame in each frame. The inner most
// frame adds an <input> element which can be used to call
// ScrollFocusedEditableNodeIntoView.
//
// Each test starts by performing a non-scrolling focus on the <input> element.
// It then performs a scroll into view (either via JavaScript bindings or
// content API) and ensures the caret is within a vertically centered band of
// the viewport.
class ScrollIntoViewBrowserTestBase : public ContentBrowserTest {
 public:
  ScrollIntoViewBrowserTestBase() = default;
  ~ScrollIntoViewBrowserTestBase() override = default;

  virtual bool IsForceLocalFrames() const = 0;
  virtual bool IsWritingModeLTR() const = 0;
  virtual TestInvokeMethod GetInvokeMethod() const = 0;
  virtual net::EmbeddedTestServer* server() { return embedded_test_server(); }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(server()->Start());

    suppress_ime_ = std::make_unique<ScopedSuppressImeEvents>(web_contents());
  }

  void TearDownOnMainThread() override {
    suppress_ime_.reset();

    ContentBrowserTest::TearDownOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolateAllSitesForTesting(command_line);

    // Need this to control page scale factor via script or check for root
    // scroller.
    command_line->AppendSwitch(switches::kExposeInternalsForTesting);
  }

  WebContentsImpl* web_contents() {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  FrameTreeNode* InnerMostFrameTreeNode() {
    FrameTreeNode* inner_most_node = nullptr;
    ForEachFrameFromRootToInnerMost(
        [&inner_most_node](FrameTreeNode* node) { inner_most_node = node; });
    return inner_most_node;
  }

  FrameTreeNode* RootFrameTreeNode() {
    return web_contents()->GetPrimaryFrameTree().root();
  }

  // Gets the bounding client rect from the element returned via the given query
  // string (i.e. as found via document.querySelector).
  gfx::RectF GetClientRect(FrameTreeNode* node, std::string query) {
    auto result = EvalJs(node, JsReplace(R"JS(
      JSON.stringify(document.querySelector($1).getBoundingClientRect());
    )JS",
                                         query));
    std::optional<base::Value> value =
        base::JSONReader::Read(result.ExtractString());
    CHECK(value.has_value());
    CHECK(value->is_dict());

    const base::Value::Dict& dict = value->GetDict();
    std::optional<double> x = dict.FindDouble("x");
    std::optional<double> y = dict.FindDouble("y");
    std::optional<double> width = dict.FindDouble("width");
    std::optional<double> height = dict.FindDouble("height");

    CHECK(x);
    CHECK(y);
    CHECK(width);
    CHECK(height);

    return gfx::RectF(*x, *y, *width, *height);
  }

  gfx::RectF GetLayoutViewportRect() {
    return GetClientRect(RootFrameTreeNode(), ".layoutViewport");
  }

  struct VisualViewport {
    // These are the values coming from the window.visualViewport object. Note:
    // the width/height are _relative to the root frame_, meaning they decrease
    // as `scale` increases.
    double offset_left;
    double offset_top;
    double width;
    double height;
    double scale;
    double page_left;
    double page_top;

    // This is the _unscaled_ rect computed from values above.
    gfx::RectF rect;
  };

  VisualViewport GetVisualViewport() {
    auto result = EvalJs(RootFrameTreeNode(), R"JS(
      JSON.stringify({
        offsetLeft: visualViewport.offsetLeft,
        offsetTop: visualViewport.offsetTop,
        width: visualViewport.width,
        height: visualViewport.height,
        scale: visualViewport.scale,
        pageLeft: visualViewport.pageLeft,
        pageTop: visualViewport.pageTop});
    )JS");

    std::optional<base::Value> value =
        base::JSONReader::Read(result.ExtractString());
    CHECK(value.has_value());
    CHECK(value->is_dict());

    const base::Value::Dict& dict = value->GetDict();
    std::optional<double> offset_left = dict.FindDouble("offsetLeft");
    std::optional<double> offset_top = dict.FindDouble("offsetTop");
    std::optional<double> width = dict.FindDouble("width");
    std::optional<double> height = dict.FindDouble("height");
    std::optional<double> scale = dict.FindDouble("scale");
    std::optional<double> page_left = dict.FindDouble("pageLeft");
    std::optional<double> page_top = dict.FindDouble("pageTop");

    CHECK(offset_left);
    CHECK(offset_top);
    CHECK(width);
    CHECK(height);
    CHECK(scale);
    CHECK(page_left);
    CHECK(page_top);

    VisualViewport values;
    values.offset_left = *offset_left;
    values.offset_top = *offset_top;
    values.width = *width;
    values.height = *height;
    values.scale = *scale;
    values.page_left = *page_left;
    values.page_top = *page_top;

    values.rect = gfx::RectF(
        gfx::PointF(),
        gfx::ScaleSize(gfx::SizeF(values.width, values.height), values.scale));

    return values;
  }

  // Gets the bounding rect of the caret (taken from the <input> element in the
  // inner-most frame) as it appears in the root most viewport.
  //
  // This accounts for clipping in each intervening frame.
  //
  // WARNING: This doesn't take transforms on the frames into account. It also
  // makes a guess on where the caret is, based on the writing-mode of the
  // document.
  gfx::RectF GetCaretRectInViewport() {
    FrameTreeNode* node = InnerMostFrameTreeNode();
    gfx::RectF rect = GetClientRect(node, "input");

    // Take either the left-most or right-most portion of the input box as an
    // estimate of the caret; based on the writing-mode of the page.
    constexpr float kCaretBoxWidth = 30.f;
    if (IsWritingModeLTR()) {
      rect.Inset(gfx::InsetsF::TLBR(0, 0, 0, rect.width() - kCaretBoxWidth));
    } else {
      rect.Inset(gfx::InsetsF::TLBR(0, rect.width() - kCaretBoxWidth, 0, 0));
    }

    EXPECT_EQ("", EvalJs(node, "document.querySelector('input').value"))
        << "Caret location is assumed based on empty <input> value";

    // If `node` is a child frame, we'll convert rect up the ancestor frame
    // chain, clipping to each frame rect.
    FrameTreeNode* frame =
        FrameTreeNode::From(node->GetParentOrOuterDocument());
    while (frame) {
      gfx::RectF parent_rect = GetClientRect(frame, "#childframe");
      rect.Offset(parent_rect.OffsetFromOrigin());

      rect = gfx::IntersectRects(parent_rect, rect);

      frame = FrameTreeNode::From(frame->GetParentOrOuterDocument());
    }

    gfx::RectF root_frame_rect = GetLayoutViewportRect();
    root_frame_rect.set_origin(gfx::PointF());

    rect = gfx::IntersectRects(root_frame_rect, rect);

    VisualViewport visual_viewport = GetVisualViewport();
    rect.Offset(-visual_viewport.offset_left, -visual_viewport.offset_top);
    rect.Scale(visual_viewport.scale);

    rect = gfx::IntersectRects(visual_viewport.rect, rect);

    return rect;
  }

  // Returns the rect within the visual viewport where, if the caret ends up in
  // after a scroll into view, we'll consider it a success.
  gfx::RectF GetAcceptableCaretRect() {
    gfx::RectF caret_in_viewport = GetCaretRectInViewport();
    VisualViewport visual_viewport = GetVisualViewport();

    gfx::RectF rect = visual_viewport.rect;

    // Vertically, the caret should be roughly centered (40px of wiggleroom,
    // e.g. for scrollbars, in either direction) in the viewport.
    const float kVerticalInset =
        ((rect.height() - caret_in_viewport.height()) / 2.f) - 40.f;

    // Horizontally, we're less picky, as long as the caret is in the viewport.
    // TODO(bokan): The constants used in
    // WebViewImpl::ComputeScaleAndScrollForEditableElementRects are somewhat
    // inscrutible and dimension dependent (which is a problem when this test
    // runs on Android and the width depends on the device). Ideally we'd be
    // able to ensure the caret appears in the right region of the viewport.
    const float kHorizontalInset = 0.f;

    rect.Inset(gfx::InsetsF::VH(kVerticalInset, kHorizontalInset));
    return rect;
  }

  // Modifies the frame tree string as needed for different test parameters.
  GURL GetMainURLForFrameTree(std::string frame_tree_string) {
    // To make things simple, remove any whitespace or empty attribute lists.
    re2::RE2::GlobalReplace(&frame_tree_string, "\\s*", "");
    re2::RE2::GlobalReplace(&frame_tree_string, "{}", "");

    // If we're in a local frame test variant, replace all site strings with
    // "siteA".
    if (IsForceLocalFrames()) {
      re2::RE2::GlobalReplace(&frame_tree_string, "site[A-Z]", "siteA");
    }

    // For RTL tests, add {RTL} attribute on each frame.
    if (!IsWritingModeLTR()) {
      // Prepend RTL to any existing attribute lists.
      re2::RE2::GlobalReplace(&frame_tree_string, "{(.*?)}", "{RTL,\\1}");

      // Add an attribute list with RTL to sites without an existing list.
      {
        std::string regex =
            // Match any site name (store in capture group 1).
            "(site[A-Z])"

            // That's followed by a non-{ character or line-end (store in
            // capture group 2).
            "([^{]|$)";

        re2::RE2::GlobalReplace(&frame_tree_string, regex, "\\1{RTL}\\2");
      }
    }

    return server()->GetURL(
        "a.test", base::StrCat({"/cross_site_scroll_into_view_factory.html?",
                                frame_tree_string}));
  }

  // Simualte a keyboard coming up, insetting the viewport by its height.
  void SetAuraOnScreenKeyboardInset(int keyboard_height) {
#if defined(USE_AURA)
    RenderWidgetHostViewBase* inner_most_view = InnerMostFrameTreeNode()
                                                    ->current_frame_host()
                                                    ->GetRenderWidgetHost()
                                                    ->GetView();

    RenderWidgetHostViewBase* root_view = inner_most_view->GetRootView();

    // Set the pointer type to simulate the keyboard appearing as a result of
    // the user tapping on an editable element.
    root_view->SetLastPointerType(ui::EventPointerType::kTouch);
    root_view->SetInsets(gfx::Insets::TLBR(0, 0, keyboard_height, 0));
#else
    NOTREACHED_IN_MIGRATION();
#endif
  }

  // Calls `func` with each FrameTreeNode in the page, starting from the root
  // and descending into the inner most frame, traversing frame tree boundaries
  // such as fenced frames.
  template <typename Function>
  void ForEachFrameFromRootToInnerMost(const Function& func) {
    FrameTreeNode* node = web_contents()->GetPrimaryFrameTree().root();
    while (node) {
      bool is_proxy_for_inner_frame_tree =
          !node->current_frame_host()
               ->inner_tree_main_frame_tree_node_id()
               .is_null();
      // The functor isn't called for the placeholder FrameTreeNode, it'll be
      // called on the inner tree's root.
      if (!is_proxy_for_inner_frame_tree)
        func(node);

      if (node->child_count()) {
        CHECK(node->current_frame_host()
                  ->inner_tree_main_frame_tree_node_id()
                  .is_null());
        // These tests never have multiple child frames.
        CHECK_EQ(node->child_count(), 1ul);
        node = node->child_at(0);
      } else if (is_proxy_for_inner_frame_tree) {
        CHECK_EQ(node->child_count(), 0ul);
        node = FrameTreeNode::GloballyFindByID(
            node->current_frame_host()->inner_tree_main_frame_tree_node_id());
      } else {
        node = nullptr;
      }
    }
  }

  // Cross origin frames may throttle their lifecycle when not visible.
  // This method ensure each frame is brought into view and a frame produced to
  // ensure up-to-date layout.
  void EnsureAllFramesCompletedLifecycle() {
    // Wait until each frame presents a CompositorFrame and then scroll its
    // child frame (if it has one) into view, so that it is unthrottled and
    // able to generate and present CompositorFrames.
    ForEachFrameFromRootToInnerMost([](FrameTreeNode* node) {
      base::RunLoop loop;
      node->current_frame_host()->InsertVisualStateCallback(
          base::BindLambdaForTesting(
              [&loop](bool visual_state_updated) { loop.Quit(); }));
      loop.Run();

      EXPECT_TRUE(ExecJs(node, R"JS(
              if (document.getElementById('childframe'))
                document.getElementById('childframe').scrollIntoView()
          )JS"));
    });

    // Now that each frame has been in view and produced a frame, reset each
    // scroll offset.
    ForEachFrameFromRootToInnerMost([](FrameTreeNode* node) {
      EXPECT_TRUE(ExecJs(node, "window.scrollTo(0, 0)"));
    });
  }

  // For frame_tree syntax see tree_parser_util.js.
  // These tests place two additional restrictions to make some simplifying
  // assumptions:
  //
  //  * All site names must start with "site" and be followed by [A-Z].
  //  * Allow only one or zero children. That is, siteA(siteB) is valid but
  //    siteA(siteB, siteB) is not.
  //
  // For valid arguments, see comments in
  // cross_site_scroll_into_view_factory.html
  bool SetupTest(std::string frame_tree) {
    const GURL kMainUrl(GetMainURLForFrameTree(frame_tree));

    if (!NavigateToURL(shell(), kMainUrl))
      return false;

    EnsureAllFramesCompletedLifecycle();

    VisualViewport viewport = GetVisualViewport();
    double page_scale_factor_before = viewport.scale;
    double page_left_before = viewport.page_left;
    double page_top_before = viewport.page_top;

    if (GetInvokeMethod() == kInputHandler ||
        GetInvokeMethod() == kAuraOnScreenKeyboard) {
      // Focus the input for tests that rely on scrolling to a focused element
      // (i.e. via ScrollFocusedEditableNodeIntoView).  Use `preventScroll` to
      // avoid affecting the test via the automatic scrolling caused by focus.
      //
      // Note: normally, an IME (i.e. On-Screen Keyboard) can also attempt to
      // scroll into view (in fact, using ScrollFocusedEditableNodeIntoView
      // which we're trying to test). However, in order to reliably test this
      // across platforms this test harness suppresses IME events so that the
      // on-screen keyboard on a platform that uses one will not activate in
      // response to this. See ScopedSuppressImeEvents above.
      EXPECT_TRUE_OR_FAIL(ExecJs(InnerMostFrameTreeNode(), R"JS(
        document.querySelector('input').focus({preventScroll: true});
      )JS"));
    }

    // The test should start with fresh scroll and scale.
    viewport = GetVisualViewport();
    CHECK_EQ(viewport.scale, page_scale_factor_before);
    CHECK_EQ(viewport.page_left, page_left_before);
    CHECK_EQ(viewport.page_top, page_top_before);

    return true;
  }

  void RunTest() {
    switch (GetInvokeMethod()) {
      case kInputHandler: {
        ScopedFocusScrollWaiter wait_for_scroll_done(RootFrameTreeNode());

        GetInputHandler(InnerMostFrameTreeNode())
            ->ScrollFocusedEditableNodeIntoView();
      } break;
      case kAuraOnScreenKeyboard: {
        ScopedFocusScrollWaiter wait_for_scroll_done(RootFrameTreeNode());
        SetAuraOnScreenKeyboardInset(/*keyboard_height=*/400);
      } break;
      case kJavaScript: {
        RenderFrameSubmissionObserver frame_observer(web_contents());
        EXPECT_TRUE(ExecJs(InnerMostFrameTreeNode(), R"JS(
          document.querySelector('input').scrollIntoView({
            behavior: 'instant',
            block: 'center',
            inline: 'center'
          })
        )JS"));
        frame_observer.WaitForScrollOffsetAtTop(
            /*expected_scroll_offset_at_top=*/false);
      } break;
    }

    gfx::RectF caret_in_viewport = GetCaretRectInViewport();
    gfx::RectF acceptable_rect = GetAcceptableCaretRect();

    EXPECT_TRUE(acceptable_rect.Contains(caret_in_viewport))
        << "Expected caret to within [" << acceptable_rect.ToString()
        << "] but caret is [" << caret_in_viewport.ToString() << "]";
  }

 private:
  std::unique_ptr<ScopedSuppressImeEvents> suppress_ime_;
};

// Runs tests in all combinations of Local/Remote frames,
// left-to-right/right-to-left writing modes, and scrollIntoView via
// element.scrollIntoView/InputHandler.ScrollFocusedEditableNodeIntoView. The
// kAuraOnScreenKeyboard is intentionally omitted as it is expected to be
// functionally equivalent to kInputHandler.
class ScrollIntoViewBrowserTest
    : public ScrollIntoViewBrowserTestBase,
      public ::testing::WithParamInterface<
          std::tuple<TestFrameType, TestWritingMode, TestInvokeMethod>> {
 public:
  bool IsForceLocalFrames() const override {
    return std::get<0>(GetParam()) == kLocalFrame;
  }

  bool IsWritingModeLTR() const override {
    return std::get<1>(GetParam()) == kLTR;
  }

  TestInvokeMethod GetInvokeMethod() const override {
    return std::get<2>(GetParam());
  }

  static std::string DescribeParams(
      const testing::TestParamInfo<ParamType>& info) {
    auto [frame_type_param, writing_mode_param, invoke_method_param] =
        info.param;

    std::string frame_type;
    switch (frame_type_param) {
      case kLocalFrame: {
        frame_type = "LocalFrame";
      } break;
      case kRemoteFrame: {
        frame_type = "RemoteFrame";
      } break;
    }

    std::string writing_mode;
    switch (writing_mode_param) {
      case kLTR: {
        writing_mode = "LTR";
      } break;
      case kRTL: {
        writing_mode = "RTL";
      } break;
    }

    std::string invoke_method;
    switch (invoke_method_param) {
      case kJavaScript: {
        invoke_method = "JavaScript";
      } break;
      case kInputHandler: {
        invoke_method = "ScrollFocusedEditableNodeIntoView";
      } break;
      case kAuraOnScreenKeyboard: {
        invoke_method = "AuraOnScreenKeyboard";
      } break;
    }

    return base::StringPrintf("%s_%s_%s", frame_type.c_str(),
                              writing_mode.c_str(), invoke_method.c_str());
  }
};

// See comment in SetupTest for frame tree syntax.

// ScrollIntoViewBrowserTest runs with all combinations of multiple parameters
// to test the basic scroll into view machinery so each test instantiates 8
// cases. To avoid an explosion of tests, prefer to add new tests to a more
// specific suite unless the functionality it's testing is likely to differ
// across the various parameters and isn't already covered.

IN_PROC_BROWSER_TEST_P(ScrollIntoViewBrowserTest, EditableInSingleNestedFrame) {
  ASSERT_TRUE(SetupTest("siteA(siteB)"));
  RunTest();
}

IN_PROC_BROWSER_TEST_P(ScrollIntoViewBrowserTest, EditableInLocalRoot) {
  ASSERT_TRUE(SetupTest("siteA(siteB(siteA))"));
  RunTest();
}

IN_PROC_BROWSER_TEST_P(ScrollIntoViewBrowserTest, EditableInDoublyNestedFrame) {
  ASSERT_TRUE(SetupTest("siteA(siteB(siteC))"));
  RunTest();
}

IN_PROC_BROWSER_TEST_P(
    ScrollIntoViewBrowserTest,
    CrossesEditableInDoublyNestedFrameLocalAndRemoteBoundaries) {
  ASSERT_TRUE(SetupTest("siteA(siteA(siteB(siteB)))"));
  RunTest();
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    ScrollIntoViewBrowserTest,
    testing::Combine(testing::Values(kLocalFrame, kRemoteFrame),
                     testing::Values(kLTR, kRTL),
                     // kAuraOnScreenKeyboard is intentionally omitted as it is
                     // expected to be functionally equivalent to
                     // kInputHandler.
                     testing::Values(kJavaScript, kInputHandler)),
    ScrollIntoViewBrowserTest::DescribeParams);

#if defined(USE_AURA)

// Tests viewport insetting as a result of keyboard insets. Insetting is only
// used on Aura platforms. The OSK on Android resizes the entire view.
class InsetScrollIntoViewBrowserTest
    : public ScrollIntoViewBrowserTestBase,
      public ::testing::WithParamInterface<TestFrameType> {
 public:
  bool IsForceLocalFrames() const override { return GetParam() == kLocalFrame; }
  bool IsWritingModeLTR() const override { return true; }
  TestInvokeMethod GetInvokeMethod() const override {
    return kAuraOnScreenKeyboard;
  }
};

// Ensure that insetting the viewport causes the visual viewport to be resized
// and focused editable scrolled into view. (https://crbug.com/927483)
IN_PROC_BROWSER_TEST_P(InsetScrollIntoViewBrowserTest,
                       InsetsCauseScrollToFocusedEditable) {
  ASSERT_TRUE(SetupTest("siteA(siteB(siteC))"));

  int contents_height = web_contents()->GetViewBounds().height();

  // Ensure the window height is large enough to accommodate the inset and leave
  // some space for a caret. Note: we can't just assume 800x600 because some
  // Windows 7 bots have less than 600px of workspace area available which
  // results in a smaller window.
  ASSERT_GT(contents_height, 450);

  int visual_viewport_height_before = GetVisualViewport().height;
  int layout_viewport_height_before = GetLayoutViewportRect().height();

  // We expect the viewport height to match the WebContents but allow some
  // fuzziness due to differing scrollbars and window decorations on different
  // platforms.
  const int kEpsilon = 30;
  EXPECT_NEAR(visual_viewport_height_before, contents_height, kEpsilon);
  EXPECT_NEAR(layout_viewport_height_before, contents_height, kEpsilon);
  EXPECT_EQ(1.f, GetVisualViewport().scale);

  RunTest();

  // The visualViewport should have been insetted by 400px but not the root
  // frame.
  EXPECT_EQ(visual_viewport_height_before - GetVisualViewport().height, 400);
  EXPECT_EQ(layout_viewport_height_before, GetLayoutViewportRect().height());
  EXPECT_EQ(1.f, GetVisualViewport().scale);

  // The rect where we expect the caret to appear must not not be below the
  // inset region.
  ASSERT_LT(GetAcceptableCaretRect().bottom(), 200);
}

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         InsetScrollIntoViewBrowserTest,
                         testing::Values(kLocalFrame, kRemoteFrame),
                         DescribeFrameType);

#endif

// Only Chrome Android performs a zoom when focusing an editable.
#if BUILDFLAG(IS_ANDROID)

constexpr double kMobileMinimumScale = 0.25;

// Tests zooming behaviors for ScrollFocusedEditableNodeIntoView. These tests
// runs only on Android since that's the only platorm that uses this behavior.
class ZoomScrollIntoViewBrowserTest
    : public ScrollIntoViewBrowserTestBase,
      public ::testing::WithParamInterface<TestFrameType> {
 public:
  bool IsForceLocalFrames() const override { return GetParam() == kLocalFrame; }

  bool IsWritingModeLTR() const override { return true; }

  TestInvokeMethod GetInvokeMethod() const override { return kInputHandler; }
};

// A regular "desktop" site (i.e. no viewport <meta> tag) on Chrome Android
// should zoom in on a focused editable so that it's legible.
IN_PROC_BROWSER_TEST_P(ZoomScrollIntoViewBrowserTest, DesktopViewportMustZoom) {
  ASSERT_TRUE(SetupTest("siteA(siteB)"));

  EXPECT_EQ(kMobileMinimumScale, GetVisualViewport().scale);

  RunTest();

  // Without a viewport tag, the page is considered a "desktop" page so we
  // should enable zooming to a legible scale.
  EXPECT_NEAR(1, GetVisualViewport().scale, 0.05);
}

// Ensure that adding a `width=device-width` viewport <meta> tag disables the
// zooming behavior so that "mobile-friendly" pages do not zoom in on input
// boxes.
IN_PROC_BROWSER_TEST_P(ZoomScrollIntoViewBrowserTest,
                       MobileViewportDisablesZoom) {
  ASSERT_TRUE(SetupTest("siteA{MobileViewport}(siteB)"));

  EXPECT_EQ(kMobileMinimumScale, GetVisualViewport().scale);

  RunTest();

  // width=device-width must prevent the zooming behavior.
  EXPECT_EQ(kMobileMinimumScale, GetVisualViewport().scale);
}

// Similar to above, an input in a touch-action region that disables pinch-zoom
// shouldn't cause zoom since it may trap the user at that zoom level.
IN_PROC_BROWSER_TEST_P(ZoomScrollIntoViewBrowserTest,
                       TouchActionNoneDisablesZoom) {
  ASSERT_TRUE(SetupTest("siteA(siteB{TouchActionNone})"));

  EXPECT_EQ(kMobileMinimumScale, GetVisualViewport().scale);

  RunTest();

  // touch-action: none must prevent the zooming behavior since the user may
  // not be able to zoom back out.
  EXPECT_EQ(kMobileMinimumScale, GetVisualViewport().scale);
}

class RootScrollerScrollIntoViewBrowserTest
    : public ScrollIntoViewBrowserTestBase {
 public:
  bool IsForceLocalFrames() const override { return false; }
  bool IsWritingModeLTR() const override { return true; }
  TestInvokeMethod GetInvokeMethod() const override { return kInputHandler; }
};

IN_PROC_BROWSER_TEST_F(RootScrollerScrollIntoViewBrowserTest,
                       FocusInRootScroller) {
  ASSERT_TRUE(SetupTest("siteA{RootScroller,MobileViewportNoZoom}"));

  // Root scroller is recomputed after a Blink lifecycle so ensure a frame is
  // produced to make sure the renderer has had time to evaluate the root
  // scroller.
  {
    base::RunLoop loop;
    shell()->web_contents()->GetPrimaryMainFrame()->InsertVisualStateCallback(
        base::BindLambdaForTesting(
            [&loop](bool visual_state_updated) { loop.Quit(); }));
    loop.Run();
  }

  ASSERT_EQ(1.0, GetVisualViewport().scale);
  ASSERT_EQ(
      true,
      EvalJs(
          InnerMostFrameTreeNode(),
          "window.internals.effectiveRootScroller(document).tagName == 'DIV'"));

  RunTest();
}

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         ZoomScrollIntoViewBrowserTest,
                         testing::Values(kLocalFrame, kRemoteFrame),
                         DescribeFrameType);
#endif

// Tests scrollIntoView behaviors related to a fenced frame.
class ScrollIntoViewFencedFrameBrowserTest
    : public ScrollIntoViewBrowserTestBase {
 public:
  ScrollIntoViewFencedFrameBrowserTest() {
    feature_list_.InitWithFeatures({blink::features::kFencedFrames,
                                    features::kPrivacySandboxAdsAPIsOverride,
                                    blink::features::kFencedFramesAPIChanges,
                                    blink::features::kFencedFramesDefaultMode},
                                   {/* disabled_features */});
  }
  bool IsForceLocalFrames() const override { return false; }
  bool IsWritingModeLTR() const override { return true; }
  TestInvokeMethod GetInvokeMethod() const override { return kInputHandler; }
  net::EmbeddedTestServer* server() override { return &https_server_; }

  void SetUpOnMainThread() override {
    https_server_.ServeFilesFromSourceDirectory(GetTestDataFilePath());
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    ScrollIntoViewBrowserTestBase::SetUpOnMainThread();
  }

 private:
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ScrollIntoViewFencedFrameBrowserTest,
                       SingleFencedFrame) {
  ASSERT_TRUE(SetupTest("siteA{FencedFrame}(siteB)"));
  RunTest();
}

IN_PROC_BROWSER_TEST_F(ScrollIntoViewFencedFrameBrowserTest,
                       NestedFencedFrames) {
  ASSERT_TRUE(SetupTest("siteA{FencedFrame}(siteB{FencedFrame}(siteC))"));
  RunTest();
}

IN_PROC_BROWSER_TEST_F(ScrollIntoViewFencedFrameBrowserTest,
                       LocalFrameInFencedFrame) {
  ASSERT_TRUE(SetupTest("siteA{FencedFrame}(siteB(siteB))"));
  RunTest();
}

IN_PROC_BROWSER_TEST_F(ScrollIntoViewFencedFrameBrowserTest,
                       RemoteFrameInFencedFrame) {
  ASSERT_TRUE(SetupTest("siteA{FencedFrame}(siteB(siteC))"));

  // TODO(bokan): This is required due to a race in how page-level focus is
  // transferred. If the race is won by the page level focus notification then
  // it'll clobber the <input> focus and reset it to the main frame. In this
  // case, trying again will work because the fenced frame tree already has
  // page focus now so focusing it doesn't change page focus. See
  // https://crbug.com/1327439.
  {
    VisualViewport viewport = GetVisualViewport();
    double page_scale_factor_before = viewport.scale;

    EXPECT_TRUE(ExecJs(InnerMostFrameTreeNode(), R"JS(
      document.querySelector('input').focus({preventScroll: true});
    )JS"));

    // The test should start with fresh scroll and scale.
    ASSERT_EQ(viewport.scale, page_scale_factor_before);
    ASSERT_EQ(viewport.page_left, 0);
    ASSERT_EQ(viewport.page_top, 0);
  }

  RunTest();
}

IN_PROC_BROWSER_TEST_F(ScrollIntoViewFencedFrameBrowserTest,
                       FencedFrameInRemoteFrame) {
  ASSERT_TRUE(SetupTest("siteA(siteB{FencedFrame}(siteC))"));
  RunTest();
}

IN_PROC_BROWSER_TEST_F(ScrollIntoViewFencedFrameBrowserTest,
                       ProgrammaticScrollIntoViewDoesntCrossFencedFrame) {
  ASSERT_TRUE(SetupTest("siteA{FencedFrame}(siteB)"));

  ScrollRectToVisibleInParentFrameInterceptor interceptor;
  interceptor.Init(InnerMostFrameTreeNode()->current_frame_host());

  ASSERT_EQ(0, EvalJs(InnerMostFrameTreeNode(), "window.scrollX"));
  ASSERT_EQ(0, EvalJs(InnerMostFrameTreeNode(), "window.scrollY"));
  ASSERT_TRUE(ExecJs(InnerMostFrameTreeNode(), R"JS(
    document.querySelector('input').scrollIntoView({
      behavior: 'instant',
      block: 'center',
      inline: 'center'
    })
  )JS"));
  ASSERT_LT(0, EvalJs(InnerMostFrameTreeNode(), "window.scrollX"));
  ASSERT_LT(0, EvalJs(InnerMostFrameTreeNode(), "window.scrollY"));

  // Since bubbling to a parent frame happens synchronously in scrollIntoView,
  // once the fenced frame has visible scroll we can guarantee that, if it
  // tried bubbling the scroll to the parent the message must have been sent to
  // the browser by now.
  InnerMostFrameTreeNode()
      ->current_frame_host()
      ->local_frame_host_receiver_for_testing()
      .FlushForTesting();
  EXPECT_FALSE(interceptor.HasCalledScrollRectToVisibleInParentFrame());
}

}  // namespace

}  // namespace content
