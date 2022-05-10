// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/json/json_reader.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
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
// ScrollFocusedEditableNodeIntoRect method, or via setting an OSK inset.
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

// Will block from the destructor until a ScrollFocusedEditableNodeIntoRect has
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

// Test harness for ScrollIntoView related browser tests. These tests are
// mainly concerned with behavior of scroll into view related functionality
// across remote frames. This harness depends on
// cross_site_scroll_into_view_factory.html, which is based on
// cross_site_iframe_factory.html.
//
// cross_site_scroll_into_view_factory.html builds a frame tree from its given
// argument, allowing only a single child frame in each frame. The inner most
// frame adds an <input> element which can be used to call
// ScrollFocusedEditableNodeIntoRect.
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

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());

    suppress_ime_ = std::make_unique<ScopedSuppressImeEvents>(web_contents());
  }

  void TearDownOnMainThread() override {
    suppress_ime_.reset();

    ContentBrowserTest::TearDownOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    IsolateAllSitesForTesting(command_line);

    // Need this to control page scale factor via script.
    command_line->AppendSwitch(switches::kExposeInternalsForTesting);
  }

  WebContentsImpl* web_contents() {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  FrameTreeNode* InnerMostFrameTreeNode() {
    FrameTreeNode* node = web_contents()->GetPrimaryFrameTree().root();
    while (node->child_count()) {
      // These tests never have multiple child frames.
      CHECK_EQ(node->child_count(), 1ul);

      node = node->child_at(0);
    }
    return node;
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
    absl::optional<base::Value> value =
        base::JSONReader::Read(result.ExtractString());
    CHECK(value.has_value());
    CHECK(value->is_dict());

    absl::optional<double> x = value->FindDoubleKey("x");
    absl::optional<double> y = value->FindDoubleKey("y");
    absl::optional<double> width = value->FindDoubleKey("width");
    absl::optional<double> height = value->FindDoubleKey("height");

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

    absl::optional<base::Value> value =
        base::JSONReader::Read(result.ExtractString());
    CHECK(value.has_value());
    CHECK(value->is_dict());

    absl::optional<double> offset_left = value->FindDoubleKey("offsetLeft");
    absl::optional<double> offset_top = value->FindDoubleKey("offsetTop");
    absl::optional<double> width = value->FindDoubleKey("width");
    absl::optional<double> height = value->FindDoubleKey("height");
    absl::optional<double> scale = value->FindDoubleKey("scale");
    absl::optional<double> page_left = value->FindDoubleKey("pageLeft");
    absl::optional<double> page_top = value->FindDoubleKey("pageTop");

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
    FrameTreeNode* frame = FrameTreeNode::From(node->parent());
    while (frame) {
      gfx::RectF parent_rect = GetClientRect(frame, "iframe");
      rect.Offset(parent_rect.OffsetFromOrigin());

      rect = gfx::IntersectRects(parent_rect, rect);

      frame = FrameTreeNode::From(frame->parent());
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

    return embedded_test_server()->GetURL(
        "a.com", base::StrCat({"/cross_site_scroll_into_view_factory.html?",
                               frame_tree_string}));
  }

  // Simualte a keyboard coming up, insetting the viewport by its height.
  void SetAuraOnScreenKeyboardInset(int keyboard_height) {
#if defined(USE_AURA)
    RenderWidgetHostViewChildFrame* child_render_widget_host_view_child_frame =
        static_cast<RenderWidgetHostViewChildFrame*>(InnerMostFrameTreeNode()
                                                         ->current_frame_host()
                                                         ->GetRenderWidgetHost()
                                                         ->GetView());

    RenderWidgetHostViewAura* parent_render_widget_host_aura =
        static_cast<RenderWidgetHostViewAura*>(
            child_render_widget_host_view_child_frame->GetRootView());

    // Set the pointer type to simulate the keyboard appearing as a result of
    // the user tapping on an editable element.
    parent_render_widget_host_aura->SetLastPointerType(
        ui::EventPointerType::kTouch);
    parent_render_widget_host_aura->SetInsets(
        gfx::Insets::TLBR(0, 0, keyboard_height, 0));
#else
    NOTREACHED();
#endif
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

    VisualViewport viewport = GetVisualViewport();
    double page_scale_factor_before = viewport.scale;
    double page_left_before = viewport.page_left;
    double page_top_before = viewport.page_top;

    if (GetInvokeMethod() == kInputHandler ||
        GetInvokeMethod() == kAuraOnScreenKeyboard) {
      // Focus the input for tests that rely on scrolling to a focused element
      // (i.e. via ScrollFocusedEditableNodeIntoRect).  Use `preventScroll` to
      // avoid affecting the test via the automatic scrolling caused by focus.
      //
      // Note: normally, an IME (i.e. On-Screen Keyboard) can also attempt to
      // scroll into view (in fact, using ScrollFocusedEditableNodeIntoRect
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

        // The gfx::Rect() param is used only to debounce repeated calls, see
        // the TODO at https://bit.ly/3vIdTsD
        GetInputHandler(InnerMostFrameTreeNode())
            ->ScrollFocusedEditableNodeIntoRect(gfx::Rect());
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
// element.scrollIntoView/InputHandler.ScrollFocusedEditableNodeIntoRect. The
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
        invoke_method = "ScrollFocusedEditableNodeIntoRect";
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

IN_PROC_BROWSER_TEST_P(ScrollIntoViewBrowserTest, EditableInSingleNestedFrame) {
  ASSERT_TRUE(SetupTest("siteA(siteB)"));
  RunTest();
}

IN_PROC_BROWSER_TEST_P(ScrollIntoViewBrowserTest, EditableInLocalRoot) {
// TODO(crbug.com/1323876) Flaky on Mac.
#if BUILDFLAG(IS_MAC)
  if (!IsForceLocalFrames())
    return;
#endif
  ASSERT_TRUE(SetupTest("siteA(siteB(siteA))"));
  RunTest();
}

IN_PROC_BROWSER_TEST_P(ScrollIntoViewBrowserTest, EditableInDoublyNestedFrame) {
// TODO(crbug.com/1323876) Flaky on Mac.
#if BUILDFLAG(IS_MAC)
  if (!IsForceLocalFrames())
    return;
#endif
  ASSERT_TRUE(SetupTest("siteA(siteB(siteC))"));
  RunTest();
}

IN_PROC_BROWSER_TEST_P(
    ScrollIntoViewBrowserTest,
    CrossesEditableInDoublyNestedFrameLocalAndRemoteBoundaries) {
// TODO(crbug.com/1323876) Flaky on Mac.
#if BUILDFLAG(IS_MAC)
  if (!IsForceLocalFrames())
    return;
#endif
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
// TODO(bokan): Failing flakily on Windows. https://crbug.com/1323876.
#if BUILDFLAG(IS_WIN)
#define MAYBE_InsetsCauseScrollToFocusedEditable \
  DISABLED_InsetsCauseScrollToFocusedEditable
#else
#define MAYBE_InsetsCauseScrollToFocusedEditable \
  InsetsCauseScrollToFocusedEditable
#endif
IN_PROC_BROWSER_TEST_P(InsetScrollIntoViewBrowserTest,
                       MAYBE_InsetsCauseScrollToFocusedEditable) {
  ASSERT_TRUE(SetupTest("siteA(siteB(siteC))"));

  // Allow some fuzziness due to scrollbar.
  const int kScrollbarApprox = 20;
  ASSERT_NEAR(600, GetVisualViewport().height, kScrollbarApprox);
  ASSERT_NEAR(600, GetLayoutViewportRect().height(), kScrollbarApprox);
  ASSERT_EQ(1.f, GetVisualViewport().scale);

  RunTest();

  // The visualViewport should have been insetted but not the root frame.
  ASSERT_NEAR(200, GetVisualViewport().height, kScrollbarApprox);
  ASSERT_NEAR(600, GetLayoutViewportRect().height(), kScrollbarApprox);
  ASSERT_EQ(1.f, GetVisualViewport().scale);

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

// Tests zooming behaviors for ScrollFocusedEditableNodeIntoRect. These tests
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
                       ViewportMetaDisablesZoom) {
  ASSERT_TRUE(SetupTest("siteA{ViewportMeta}(siteB)"));

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

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         ZoomScrollIntoViewBrowserTest,
                         testing::Values(kLocalFrame, kRemoteFrame),
                         DescribeFrameType);
#endif

}  // namespace

}  // namespace content
