// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/message_loop/message_loop_current.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/renderer_host/dip_util.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/common/frame_messages.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/did_commit_navigation_interceptor.h"
#include "net/base/filename_util.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/layout.h"
#include "ui/display/display_switches.h"
#include "ui/gfx/geometry/size_conversions.h"

#if defined(OS_ANDROID)
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "ui/android/delegated_frame_host_android.h"
#endif

namespace content {

namespace {

// Convenience macro: Short-circuit a pass for the tests where platform support
// for forced-compositing mode (or disabled-compositing mode) is lacking.
#define SET_UP_SURFACE_OR_PASS_TEST(wait_message)  \
  if (!SetUpSourceSurface(wait_message)) {  \
    LOG(WARNING)  \
        << ("Blindly passing this test: This platform does not support "  \
            "forced compositing (or forced-disabled compositing) mode.");  \
    return;  \
  }

// Common base class for browser tests.  This is subclassed three times: Once to
// test the browser in forced-compositing mode; once to test with compositing
// mode disabled; once with no surface creation for non-visual tests.
class RenderWidgetHostViewBrowserTest : public ContentBrowserTest {
 public:
  RenderWidgetHostViewBrowserTest()
      : frame_size_(400, 300),
        callback_invoke_count_(0),
        frames_captured_(0) {}

  void SetUpOnMainThread() override {
    ASSERT_TRUE(base::PathService::Get(DIR_TEST_DATA, &test_dir_));
  }

  // Attempts to set up the source surface.  Returns false if unsupported on the
  // current platform.
  virtual bool SetUpSourceSurface(const char* wait_message) = 0;

  int callback_invoke_count() const {
    return callback_invoke_count_;
  }

  int frames_captured() const {
    return frames_captured_;
  }

  const gfx::Size& frame_size() const {
    return frame_size_;
  }

  const base::FilePath& test_dir() const {
    return test_dir_;
  }

  RenderViewHost* GetRenderViewHost() const {
    RenderViewHost* const rvh = shell()->web_contents()->GetRenderViewHost();
    CHECK(rvh);
    return rvh;
  }

  RenderWidgetHostImpl* GetRenderWidgetHost() const {
    RenderWidgetHostImpl* const rwh = RenderWidgetHostImpl::From(
        shell()->web_contents()->GetRenderWidgetHostView()->
            GetRenderWidgetHost());
    CHECK(rwh);
    return rwh;
  }

  RenderWidgetHostViewBase* GetRenderWidgetHostView() const {
    return static_cast<RenderWidgetHostViewBase*>(
        GetRenderViewHost()->GetWidget()->GetView());
  }

  // Callback when using CopyFromSurface() API.
  void FinishCopyFromSurface(const base::Closure& quit_closure,
                             const SkBitmap& bitmap) {
    ++callback_invoke_count_;
    if (!bitmap.drawsNothing())
      ++frames_captured_;
    if (!quit_closure.is_null())
      quit_closure.Run();
  }

 protected:
  // Waits until the source is available for copying.
  void WaitForCopySourceReady() {
    while (!GetRenderWidgetHostView()->IsSurfaceAvailableForCopy())
      GiveItSomeTime();
  }

  // Run the current message loop for a short time without unwinding the current
  // call stack.
  static void GiveItSomeTime() {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(),
        base::TimeDelta::FromMilliseconds(250));
    run_loop.Run();
  }

 private:
  const gfx::Size frame_size_;
  base::FilePath test_dir_;
  int callback_invoke_count_;
  int frames_captured_;
};

// Helps to ensure that a navigation is committed after a compositor frame was
// submitted by the renderer, but before corresponding ACK is sent back.
class CommitBeforeSwapAckSentHelper : public DidCommitNavigationInterceptor {
 public:
  explicit CommitBeforeSwapAckSentHelper(
      WebContents* web_contents,
      RenderFrameSubmissionObserver* frame_observer)
      : DidCommitNavigationInterceptor(web_contents),
        frame_observer_(frame_observer) {}

 private:
  // DidCommitNavigationInterceptor:
  bool WillProcessDidCommitNavigation(
      RenderFrameHost* render_frame_host,
      NavigationRequest* navigation_request,
      ::FrameHostMsg_DidCommitProvisionalLoad_Params* params,
      mojom::DidCommitProvisionalLoadInterfaceParamsPtr* interface_params)
      override {
    frame_observer_->WaitForAnyFrameSubmission();
    return true;
  }

  // Not owned.
  RenderFrameSubmissionObserver* const frame_observer_;

  DISALLOW_COPY_AND_ASSIGN(CommitBeforeSwapAckSentHelper);
};

class RenderWidgetHostViewBrowserTestBase : public ContentBrowserTest {
 public:
  ~RenderWidgetHostViewBrowserTestBase() override {}

 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
  }
};

// Base class for testing a RenderWidgetHostViewBase where visual output is not
// relevant. This class does not setup surfaces for compositing.
class NoCompositingRenderWidgetHostViewBrowserTest
    : public RenderWidgetHostViewBrowserTest {
 public:
  NoCompositingRenderWidgetHostViewBrowserTest() {}
  ~NoCompositingRenderWidgetHostViewBrowserTest() override {}

  bool SetUpSourceSurface(const char* wait_message) override {
    NOTIMPLEMENTED();
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NoCompositingRenderWidgetHostViewBrowserTest);
};

// When creating the first RenderWidgetHostViewBase, the CompositorFrameSink can
// change. When this occurs we need to evict the current frame, and recreate
// surfaces. This tests that when frame eviction occurs while the
// RenderWidgetHostViewBase is visible, that we generate a new LocalSurfaceId.
// Simply invalidating can lead to displaying blank screens.
// (https://crbug.com/909903)
IN_PROC_BROWSER_TEST_F(NoCompositingRenderWidgetHostViewBrowserTest,
                       ValidLocalSurfaceIdAllocationAfterInitialNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Creates the initial RenderWidgetHostViewBase, and connects to a
  // CompositorFrameSink. This will trigger frame eviction.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/page_with_animation.html")));
  RenderWidgetHostViewBase* rwhvb = GetRenderWidgetHostView();
  // Eviction normally invalidates the LocalSurfaceId, however if the
  // RenderWidgetHostViewBase is visible, a new id must be allocated. Otherwise
  // blank content is shown.
  EXPECT_TRUE(rwhvb);
  // Mac does not initialize RenderWidgetHostViewBase as visible.
#if !defined(OS_MACOSX)
  EXPECT_TRUE(rwhvb->IsShowing());
#endif
  EXPECT_TRUE(rwhvb->GetLocalSurfaceIdAllocation().IsValid());
  // TODO(jonross): Unify FrameEvictor into RenderWidgetHostViewBase so that we
  // can generically test all eviction paths. However this should only be for
  // top level renderers. Currently the FrameEvict implementations are platform
  // dependent so we can't have a single generic test.
}

// TODO(jonross): Update Mac to also invalidate its viz::LocalSurfaceIds when
// performing navigations while hidden. https://crbug.com/935364
#if !defined(OS_MACOSX)
// When a navigation occurs while the RenderWidgetHostViewBase is hidden, it
// should invalidate it's viz::LocalSurfaceId. When subsequently being shown,
// a new surface should be generated with a new viz::LocalSurfaceId
IN_PROC_BROWSER_TEST_F(NoCompositingRenderWidgetHostViewBrowserTest,
                       ValidLocalSurfaceIdAllocationAfterHiddenNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Creates the initial RenderWidgetHostViewBase, and connects to a
  // CompositorFrameSink.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/page_with_animation.html")));
  RenderWidgetHostViewBase* rwhvb = GetRenderWidgetHostView();
  EXPECT_TRUE(rwhvb);
  viz::LocalSurfaceId rwhvb_local_surface_id =
      rwhvb->GetLocalSurfaceIdAllocation().local_surface_id();
  EXPECT_TRUE(rwhvb_local_surface_id.is_valid());

  // Hide the view before performing the next navigation.
  shell()->web_contents()->WasHidden();
#if defined(OS_ANDROID)
  // On Android we want to ensure that we maintain the currently embedded
  // surface. So that there is something to display when returning to the tab.
  RenderWidgetHostViewAndroid* rwhva =
      static_cast<RenderWidgetHostViewAndroid*>(rwhvb);
  ui::DelegatedFrameHostAndroid* dfh =
      rwhva->delegated_frame_host_for_testing();
  EXPECT_TRUE(dfh->HasPrimarySurface());
  EXPECT_FALSE(dfh->IsPrimarySurfaceEvicted());
  viz::LocalSurfaceId initial_local_surface_id =
      dfh->SurfaceId().local_surface_id();
  EXPECT_TRUE(initial_local_surface_id.is_valid());
#endif

  // Perform a navigation to the same content source. This will reuse the
  // existing RenderWidgetHostViewBase.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/page_with_animation.html")));
  EXPECT_FALSE(rwhvb->GetLocalSurfaceIdAllocation().IsValid());

#if defined(OS_ANDROID)
  // Navigating while hidden should not generate a new surface. As the old one
  // is maintained as the fallback.
  EXPECT_TRUE(dfh->HasPrimarySurface());
  EXPECT_FALSE(dfh->IsPrimarySurfaceEvicted());
  EXPECT_EQ(initial_local_surface_id, dfh->SurfaceId().local_surface_id());
#endif

  // Showing the view should lead to a new surface being embedded.
  shell()->web_contents()->WasShown();
  viz::LocalSurfaceId new_rwhvb_local_surface_id =
      rwhvb->GetLocalSurfaceIdAllocation().local_surface_id();
  EXPECT_TRUE(new_rwhvb_local_surface_id.is_valid());
  EXPECT_NE(rwhvb_local_surface_id, new_rwhvb_local_surface_id);
#if defined(OS_ANDROID)
  EXPECT_TRUE(dfh->HasPrimarySurface());
  EXPECT_FALSE(dfh->IsPrimarySurfaceEvicted());
  viz::LocalSurfaceId new_local_surface_id =
      dfh->SurfaceId().local_surface_id();
  EXPECT_TRUE(new_local_surface_id.is_valid());
  EXPECT_NE(initial_local_surface_id, new_local_surface_id);
#endif
}
#endif  // !defined(OS_MACOSX)

IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewBrowserTestBase,
                       CompositorWorksWhenReusingRenderer) {
  ASSERT_TRUE(embedded_test_server()->Start());
  auto* web_contents = shell()->web_contents();
  // Load a page that draws new frames infinitely.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/page_with_animation.html")));
  std::unique_ptr<RenderFrameSubmissionObserver> frame_observer(
      std::make_unique<RenderFrameSubmissionObserver>(web_contents));

  // Open a new page in the same renderer to keep it alive.
  WebContents::CreateParams new_contents_params(
      web_contents->GetBrowserContext(), web_contents->GetSiteInstance());
  std::unique_ptr<WebContents> new_web_contents(
      WebContents::Create(new_contents_params));

  new_web_contents->GetController().LoadURLWithParams(
      NavigationController::LoadURLParams(GURL(url::kAboutBlankURL)));
  EXPECT_TRUE(WaitForLoadStop(new_web_contents.get()));

  // Start a cross-process navigation.
  shell()->LoadURL(embedded_test_server()->GetURL("foo.com", "/title1.html"));

  // When the navigation is about to commit, wait for the next frame to be
  // submitted by the renderer before proceeding with page load.
  {
    CommitBeforeSwapAckSentHelper commit_helper(web_contents,
                                                frame_observer.get());
    EXPECT_TRUE(WaitForLoadStop(web_contents));
    EXPECT_NE(web_contents->GetMainFrame()->GetProcess(),
              new_web_contents->GetMainFrame()->GetProcess());
  }

  // Go back and verify that the renderer continues to draw new frames.
  shell()->GoBackOrForward(-1);
  // Stop observing before we destroy |web_contents| in WaitForLoadStop.
  frame_observer.reset();
  EXPECT_TRUE(WaitForLoadStop(web_contents));
  EXPECT_EQ(web_contents->GetMainFrame()->GetProcess(),
            new_web_contents->GetMainFrame()->GetProcess());
  MainThreadFrameObserver observer(
      web_contents->GetRenderViewHost()->GetWidget());
  for (int i = 0; i < 5; ++i)
    observer.Wait();
}

enum CompositingMode {
  GL_COMPOSITING,
  SOFTWARE_COMPOSITING,
};

class CompositingRenderWidgetHostViewBrowserTest
    : public RenderWidgetHostViewBrowserTest,
      public testing::WithParamInterface<CompositingMode> {
 public:
  CompositingRenderWidgetHostViewBrowserTest()
      : compositing_mode_(GetParam()) {}

  void SetUp() override {
    if (compositing_mode_ == SOFTWARE_COMPOSITING)
      UseSoftwareCompositing();
    EnablePixelOutput();
    RenderWidgetHostViewBrowserTest::SetUp();
  }

  virtual GURL TestUrl() {
    return net::FilePathToFileURL(
        test_dir().AppendASCII("rwhv_compositing_animation.html"));
  }

  bool SetUpSourceSurface(const char* wait_message) override {
    content::DOMMessageQueue message_queue;
    EXPECT_TRUE(NavigateToURL(shell(), TestUrl()));
    if (wait_message != nullptr) {
      std::string result(wait_message);
      if (!message_queue.WaitForMessage(&result)) {
        EXPECT_TRUE(false) << "WaitForMessage " << result << " failed.";
        return false;
      }
    }

    // A frame might not be available yet. So, wait for it.
    WaitForCopySourceReady();
    return true;
  }

 private:
  const CompositingMode compositing_mode_;

  DISALLOW_COPY_AND_ASSIGN(CompositingRenderWidgetHostViewBrowserTest);
};

// Disable tests for Android as it has an incomplete implementation.
#if !defined(OS_ANDROID)

// The CopyFromSurface() API should work on all platforms when compositing is
// enabled.
IN_PROC_BROWSER_TEST_P(CompositingRenderWidgetHostViewBrowserTest,
                       CopyFromSurface) {
  SET_UP_SURFACE_OR_PASS_TEST(nullptr);

  // Repeatedly call CopyFromSurface() since, on some platforms (e.g., Windows),
  // the operation will fail until the first "present" has been made.
  int count_attempts = 0;
  while (true) {
    ++count_attempts;
    base::RunLoop run_loop;
    GetRenderWidgetHostView()->CopyFromSurface(
        gfx::Rect(), frame_size(),
        base::BindOnce(&RenderWidgetHostViewBrowserTest::FinishCopyFromSurface,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    if (frames_captured())
      break;
    else
      GiveItSomeTime();
  }

  EXPECT_EQ(count_attempts, callback_invoke_count());
  EXPECT_EQ(1, frames_captured());
}

// Tests that the callback passed to CopyFromSurface is always called, even
// when the RenderWidgetHostView is deleting in the middle of an async copy.
//
// TODO(miu): On some bots (e.g., ChromeOS and Cast Shell), this test fails
// because the RunLoop quits before its QuitClosure() is run. This is because
// the call to WebContents::Close() leads to something that makes the current
// thread's RunLoop::Delegate constantly report "should quit." We'll need to
// find a better way of testing this functionality.
IN_PROC_BROWSER_TEST_P(CompositingRenderWidgetHostViewBrowserTest,
                       DISABLED_CopyFromSurface_CallbackDespiteDelete) {
  SET_UP_SURFACE_OR_PASS_TEST(nullptr);

  base::RunLoop run_loop;
  GetRenderWidgetHostView()->CopyFromSurface(
      gfx::Rect(), frame_size(),
      base::BindOnce(&RenderWidgetHostViewBrowserTest::FinishCopyFromSurface,
                     base::Unretained(this), run_loop.QuitClosure()));
  shell()->web_contents()->Close();
  run_loop.Run();

  EXPECT_EQ(1, callback_invoke_count());
}

class CompositingRenderWidgetHostViewBrowserTestTabCapture
    : public CompositingRenderWidgetHostViewBrowserTest {
 public:
  CompositingRenderWidgetHostViewBrowserTestTabCapture()
      : readback_result_(READBACK_NO_RESPONSE),
        allowable_error_(0),
        test_url_("data:text/html,<!doctype html>") {}

  void VerifyResult(base::OnceClosure quit_callback, const SkBitmap& bitmap) {
    if (bitmap.drawsNothing()) {
      readback_result_ = READBACK_FAILED;
      std::move(quit_callback).Run();
      return;
    }
    readback_result_ = READBACK_SUCCESS;

    // Check that the |bitmap| contains cyan and/or yellow pixels.  This is
    // needed because the compositor will read back "blank" frames until the
    // first frame from the renderer is composited.  See comments in
    // PerformTestWithLeftRightRects() for more details about eliminating test
    // flakiness.
    bool contains_a_test_color = false;
    for (int i = 0; i < bitmap.width(); ++i) {
      for (int j = 0; j < bitmap.height(); ++j) {
        if (!exclude_rect_.IsEmpty() && exclude_rect_.Contains(i, j))
          continue;

        const unsigned high_threshold = 0xff - allowable_error_;
        const unsigned low_threshold = 0x00 + allowable_error_;
        const SkColor color = bitmap.getColor(i, j);
        const bool is_cyan = SkColorGetR(color) <= low_threshold &&
                             SkColorGetG(color) >= high_threshold &&
                             SkColorGetB(color) >= high_threshold;
        const bool is_yellow = SkColorGetR(color) >= high_threshold &&
                               SkColorGetG(color) >= high_threshold &&
                               SkColorGetB(color) <= low_threshold;
        if (is_cyan || is_yellow) {
          contains_a_test_color = true;
          break;
        }
      }
    }
    if (!contains_a_test_color) {
      readback_result_ = READBACK_NO_TEST_COLORS;
      std::move(quit_callback).Run();
      return;
    }

    // Compare the readback |bitmap| to the |expected_bitmap|, pixel-by-pixel.
    const SkBitmap& expected_bitmap =
        expected_copy_from_compositing_surface_bitmap_;
    EXPECT_EQ(expected_bitmap.width(), bitmap.width());
    EXPECT_EQ(expected_bitmap.height(), bitmap.height());
    if (expected_bitmap.width() != bitmap.width() ||
        expected_bitmap.height() != bitmap.height()) {
      readback_result_ = READBACK_INCORRECT_RESULT_SIZE;
      std::move(quit_callback).Run();
      return;
    }
    EXPECT_EQ(expected_bitmap.colorType(), bitmap.colorType());
    int fails = 0;
    // Note: The outermost 2 pixels are ignored because the scaling tests pick
    // up a little bleed-in from the surrounding content.
    for (int i = 2; i < bitmap.width() - 4 && fails < 10; ++i) {
      for (int j = 2; j < bitmap.height() - 4 && fails < 10; ++j) {
        if (!exclude_rect_.IsEmpty() && exclude_rect_.Contains(i, j))
          continue;

        SkColor expected_color = expected_bitmap.getColor(i, j);
        SkColor color = bitmap.getColor(i, j);
        int expected_alpha = SkColorGetA(expected_color);
        int alpha = SkColorGetA(color);
        int expected_red = SkColorGetR(expected_color);
        int red = SkColorGetR(color);
        int expected_green = SkColorGetG(expected_color);
        int green = SkColorGetG(color);
        int expected_blue = SkColorGetB(expected_color);
        int blue = SkColorGetB(color);
        EXPECT_NEAR(expected_alpha, alpha, allowable_error_)
            << "expected_color: " << std::hex << expected_color
            << " color: " <<  color
            << " Failed at " << std::dec << i << ", " << j
            << " Failure " << ++fails;
        EXPECT_NEAR(expected_red, red, allowable_error_)
            << "expected_color: " << std::hex << expected_color
            << " color: " <<  color
            << " Failed at " << std::dec << i << ", " << j
            << " Failure " << ++fails;
        EXPECT_NEAR(expected_green, green, allowable_error_)
            << "expected_color: " << std::hex << expected_color
            << " color: " <<  color
            << " Failed at " << std::dec << i << ", " << j
            << " Failure " << ++fails;
        EXPECT_NEAR(expected_blue, blue, allowable_error_)
            << "expected_color: " << std::hex << expected_color
            << " color: " <<  color
            << " Failed at " << std::dec << i << ", " << j
            << " Failure " << ++fails;
      }
    }
    EXPECT_LT(fails, 10);

    std::move(quit_callback).Run();
  }

  void SetAllowableError(int amount) { allowable_error_ = amount; }
  void SetExcludeRect(gfx::Rect exclude) { exclude_rect_ = exclude; }

  GURL TestUrl() override { return GURL(test_url_); }

  void SetTestUrl(const std::string& url) { test_url_ = url; }

  // Loads a page two boxes side-by-side, each half the width of
  // |html_rect_size|, and with different background colors. The test then
  // copies from |copy_rect| region of the page into a bitmap of size
  // |output_size|, and examines the resulting bitmap.
  // Note that |output_size| may not have the same size as |copy_rect| (e.g.
  // when the output is scaled).
  void PerformTestWithLeftRightRects(const gfx::Size& html_rect_size,
                                     const gfx::Rect& copy_rect,
                                     const gfx::Size& output_size) {
    const gfx::Size box_size(html_rect_size.width() / 2,
                             html_rect_size.height());
    SetTestUrl(base::StringPrintf(
        "data:text/html,<!doctype html>"
        "<div class='left'>"
        "  <div class='right'></div>"
        "</div>"
        "<style>"
        "body { padding: 0; margin: 0; }"
        ".left { position: absolute;"
        "        background: %%230ff;"
        "        width: %dpx;"
        "        height: %dpx;"
        "}"
        ".right { position: absolute;"
        "         left: %dpx;"
        "         background: %%23ff0;"
        "         width: %dpx;"
        "         height: %dpx;"
        "}"
        "</style>"
        "<script>"
        "  domAutomationController.send(\"DONE\");"
        "</script>",
        box_size.width(),
        box_size.height(),
        box_size.width(),
        box_size.width(),
        box_size.height()));

    SET_UP_SURFACE_OR_PASS_TEST("\"DONE\"");
    if (!ShouldContinueAfterTestURLLoad())
      return;

    RenderWidgetHostViewBase* rwhv = GetRenderWidgetHostView();

    SetupLeftRightBitmap(output_size,
                         &expected_copy_from_compositing_surface_bitmap_);

    // The page is loaded in the renderer.  Request frames from the renderer
    // until readback succeeds.  When readback succeeds, the resulting
    // SkBitmap is examined to ensure it matches the expected result.
    // This loop is needed because:
    //   1. Painting/Compositing is not synchronous with the Javascript engine,
    //      and so the "DONE" signal above could be received before the renderer
    //      provides a frame with the expected content.  http://crbug.com/405282
    //   2. Avoiding test flakiness: On some platforms, the readback operation
    //      is allowed to transiently fail.  The purpose of these tests is to
    //      confirm correct cropping/scaling behavior; and not that every
    //      readback must succeed.  http://crbug.com/444237
    int attempt_count = 0;
    do {
      // Wait a little before retrying again. This gives the most up-to-date
      // frame a chance to propagate from the renderer to the compositor.
      if (attempt_count > 0)
        GiveItSomeTime();
      ++attempt_count;

      // Request readback.  The callbacks will examine the pixels in the
      // SkBitmap result if readback was successful.
      readback_result_ = READBACK_NO_RESPONSE;
      SetAllowableError(2);
      // Scaling can cause blur/fuzz between color boundaries, particularly in
      // the middle columns for these tests.
      SetExcludeRect(
          gfx::Rect(output_size.width() / 2 - 1, 0, 2, output_size.height()));

      base::RunLoop run_loop;
      rwhv->CopyFromSurface(
          copy_rect, output_size,
          base::BindOnce(&CompositingRenderWidgetHostViewBrowserTestTabCapture::
                             VerifyResult,
                         base::Unretained(this), run_loop.QuitClosure()));
      run_loop.Run();

      // If the readback operation did not provide a frame, log the reason
      // to aid in future debugging.  This information will also help determine
      // whether the implementation is broken, or a test bot is in a bad state.
      // clang-format off
      switch (readback_result_) {
        case READBACK_SUCCESS:
          break;
        #define CASE_LOG_READBACK_WARNING(enum_value)                    \
          case enum_value:                                               \
            LOG(WARNING) << "Readback attempt failed (attempt #"         \
                         << attempt_count << ").  Reason: " #enum_value; \
            break
        CASE_LOG_READBACK_WARNING(READBACK_FAILED);
        CASE_LOG_READBACK_WARNING(READBACK_NO_TEST_COLORS);
        CASE_LOG_READBACK_WARNING(READBACK_INCORRECT_RESULT_SIZE);
        default:
          LOG(ERROR)
              << "Invalid readback response value: " << readback_result_;
          NOTREACHED();
      }
      // clang-format on
    } while (readback_result_ != READBACK_SUCCESS &&
             !testing::Test::HasFailure());
  }

  // Sets up |bitmap| to have size |copy_size|. It floods the left half with
  // #0ff and the right half with #ff0.
  void SetupLeftRightBitmap(const gfx::Size& copy_size, SkBitmap* bitmap) {
    bitmap->allocN32Pixels(copy_size.width(), copy_size.height());
    // Left half is #0ff.
    bitmap->eraseARGB(255, 0, 255, 255);
    // Right half is #ff0.
    for (int i = 0; i < copy_size.width() / 2; ++i) {
      for (int j = 0; j < copy_size.height(); ++j) {
        *(bitmap->getAddr32(copy_size.width() / 2 + i, j)) =
            SkColorSetARGB(255, 255, 255, 0);
      }
    }
  }

 protected:
  // An enum to distinguish between reasons for result verify failures.
  enum ReadbackResult {
    READBACK_NO_RESPONSE,
    READBACK_SUCCESS,
    READBACK_FAILED,
    READBACK_NO_TEST_COLORS,
    READBACK_INCORRECT_RESULT_SIZE,
  };

  virtual bool ShouldContinueAfterTestURLLoad() {
    return true;
  }

 private:
  ReadbackResult readback_result_;
  SkBitmap expected_copy_from_compositing_surface_bitmap_;
  int allowable_error_;
  gfx::Rect exclude_rect_;
  std::string test_url_;
};

IN_PROC_BROWSER_TEST_P(CompositingRenderWidgetHostViewBrowserTestTabCapture,
                       CopyFromSurface_Origin_Unscaled) {
  gfx::Rect copy_rect(400, 300);
  gfx::Size output_size = copy_rect.size();
  gfx::Size html_rect_size(400, 300);
  PerformTestWithLeftRightRects(html_rect_size, copy_rect, output_size);
}

IN_PROC_BROWSER_TEST_P(CompositingRenderWidgetHostViewBrowserTestTabCapture,
                       CopyFromSurface_Origin_Scaled) {
  gfx::Rect copy_rect(400, 300);
  gfx::Size output_size(200, 100);
  gfx::Size html_rect_size(400, 300);
  PerformTestWithLeftRightRects(html_rect_size, copy_rect, output_size);
}

IN_PROC_BROWSER_TEST_P(CompositingRenderWidgetHostViewBrowserTestTabCapture,
                       CopyFromSurface_Cropped_Unscaled) {
  // Grab 60x60 pixels from the center of the tab contents.
  gfx::Rect copy_rect(400, 300);
  copy_rect = gfx::Rect(copy_rect.CenterPoint() - gfx::Vector2d(30, 30),
                        gfx::Size(60, 60));
  gfx::Size output_size = copy_rect.size();
  gfx::Size html_rect_size(400, 300);
  PerformTestWithLeftRightRects(html_rect_size, copy_rect, output_size);
}

IN_PROC_BROWSER_TEST_P(CompositingRenderWidgetHostViewBrowserTestTabCapture,
                       CopyFromSurface_Cropped_Scaled) {
  // Grab 60x60 pixels from the center of the tab contents.
  gfx::Rect copy_rect(400, 300);
  copy_rect = gfx::Rect(copy_rect.CenterPoint() - gfx::Vector2d(30, 30),
                        gfx::Size(60, 60));
  gfx::Size output_size(20, 10);
  gfx::Size html_rect_size(400, 300);
  PerformTestWithLeftRightRects(html_rect_size, copy_rect, output_size);
}

class CompositingRenderWidgetHostViewBrowserTestTabCaptureHighDPI
    : public CompositingRenderWidgetHostViewBrowserTestTabCapture {
 public:
  CompositingRenderWidgetHostViewBrowserTestTabCaptureHighDPI() {}

 protected:
  void SetUpCommandLine(base::CommandLine* cmd) override {
    CompositingRenderWidgetHostViewBrowserTestTabCapture::SetUpCommandLine(cmd);
    cmd->AppendSwitchASCII(switches::kForceDeviceScaleFactor,
                           base::StringPrintf("%f", scale()));
  }

  bool ShouldContinueAfterTestURLLoad() override {
    // Short-circuit a pass for platforms where setting up high-DPI fails.
    const float actual_scale_factor =
        GetScaleFactorForView(GetRenderWidgetHostView());
    if (actual_scale_factor != scale()) {
      LOG(WARNING) << "Blindly passing this test; unable to force device scale "
                   << "factor: seems to be " << actual_scale_factor
                   << " but expected " << scale();
      return false;
    }
    VLOG(1) << ("Successfully forced device scale factor.  Moving forward with "
                "this test!  :-)");
    return true;
  }

  static float scale() { return 2.0f; }

 private:
  DISALLOW_COPY_AND_ASSIGN(
      CompositingRenderWidgetHostViewBrowserTestTabCaptureHighDPI);
};

// NineImagePainter implementation crashes the process on Windows when this
// content_browsertest forces a device scale factor.  http://crbug.com/399349
#if defined(OS_WIN)
#define MAYBE_CopyToBitmap_EntireRegion DISABLED_CopyToBitmap_EntireRegion
#define MAYBE_CopyToBitmap_CenterRegion DISABLED_CopyToBitmap_CenterRegion
#define MAYBE_CopyToBitmap_ScaledResult DISABLED_CopyToBitmap_ScaledResult
#else
#define MAYBE_CopyToBitmap_EntireRegion CopyToBitmap_EntireRegion
#define MAYBE_CopyToBitmap_CenterRegion CopyToBitmap_CenterRegion
#define MAYBE_CopyToBitmap_ScaledResult CopyToBitmap_ScaledResult
#endif

IN_PROC_BROWSER_TEST_P(
    CompositingRenderWidgetHostViewBrowserTestTabCaptureHighDPI,
    MAYBE_CopyToBitmap_EntireRegion) {
  gfx::Size html_rect_size(200, 150);
  gfx::Rect copy_rect(200, 150);
  // Scale the output size so that, internally, scaling is not occurring.
  gfx::Size output_size = gfx::ScaleToRoundedSize(copy_rect.size(), scale());
  PerformTestWithLeftRightRects(html_rect_size, copy_rect, output_size);
}

IN_PROC_BROWSER_TEST_P(
    CompositingRenderWidgetHostViewBrowserTestTabCaptureHighDPI,
    MAYBE_CopyToBitmap_CenterRegion) {
  gfx::Size html_rect_size(200, 150);
  // Grab 90x60 pixels from the center of the tab contents.
  gfx::Rect copy_rect =
      gfx::Rect(gfx::Rect(html_rect_size).CenterPoint() - gfx::Vector2d(45, 30),
                gfx::Size(90, 60));
  // Scale the output size so that, internally, scaling is not occurring.
  gfx::Size output_size = gfx::ScaleToRoundedSize(copy_rect.size(), scale());
  PerformTestWithLeftRightRects(html_rect_size, copy_rect, output_size);
}

IN_PROC_BROWSER_TEST_P(
    CompositingRenderWidgetHostViewBrowserTestTabCaptureHighDPI,
    MAYBE_CopyToBitmap_ScaledResult) {
  gfx::Size html_rect_size(200, 100);
  gfx::Rect copy_rect(200, 100);
  // Output is being down-scaled since output_size is in phyiscal pixels.
  gfx::Size output_size(200, 100);
  PerformTestWithLeftRightRects(html_rect_size, copy_rect, output_size);
}

class CompositingRenderWidgetHostViewBrowserTestHiDPI
    : public CompositingRenderWidgetHostViewBrowserTest {
 public:
  CompositingRenderWidgetHostViewBrowserTestHiDPI() {}

 protected:
  void SetUpCommandLine(base::CommandLine* cmd) override {
    CompositingRenderWidgetHostViewBrowserTest::SetUpCommandLine(cmd);
    cmd->AppendSwitchASCII(switches::kForceDeviceScaleFactor,
                           base::StringPrintf("%f", scale()));
  }

  GURL TestUrl() override { return GURL(test_url_); }

  void SetTestUrl(const std::string& url) { test_url_ = url; }

  bool ShouldContinueAfterTestURLLoad() {
    // Short-circuit a pass for platforms where setting up high-DPI fails.
    const float actual_scale_factor =
        GetScaleFactorForView(GetRenderWidgetHostView());
    if (actual_scale_factor != scale()) {
      LOG(WARNING) << "Blindly passing this test; unable to force device scale "
                   << "factor: seems to be " << actual_scale_factor
                   << " but expected " << scale();
      return false;
    }
    VLOG(1)
        << ("Successfully forced device scale factor.  Moving forward with "
            "this test!  :-)");
    return true;
  }

  static float scale() { return 2.0f; }

 private:
  std::string test_url_;

  DISALLOW_COPY_AND_ASSIGN(CompositingRenderWidgetHostViewBrowserTestHiDPI);
};

IN_PROC_BROWSER_TEST_P(CompositingRenderWidgetHostViewBrowserTestHiDPI,
                       ScrollOffset) {
  const int kContentHeight = 2000;
  const int kScrollAmount = 100;

  SetTestUrl(
      base::StringPrintf("data:text/html,<!doctype html>"
                         "<div class='box'></div>"
                         "<style>"
                         "body { padding: 0; margin: 0; }"
                         ".box { position: absolute;"
                         "        background: %%230ff;"
                         "        width: 100%%;"
                         "        height: %dpx;"
                         "}"
                         "</style>"
                         "<script>"
                         "  addEventListener(\"scroll\", function() {"
                         "      domAutomationController.send(\"DONE\"); });"
                         "  window.scrollTo(0, %d);"
                         "</script>",
                         kContentHeight, kScrollAmount));

  SET_UP_SURFACE_OR_PASS_TEST("\"DONE\"");
  RenderFrameSubmissionObserver observer_(
      GetRenderWidgetHost()->render_frame_metadata_provider());
  observer_.WaitForScrollOffsetAtTop(false);

  if (!ShouldContinueAfterTestURLLoad())
    return;

  EXPECT_FALSE(GetRenderWidgetHostView()->IsScrollOffsetAtTop());
}

#if defined(OS_CHROMEOS)
// On ChromeOS there is no software compositing.
static const auto kTestCompositingModes = testing::Values(GL_COMPOSITING);
#else
static const auto kTestCompositingModes =
    testing::Values(GL_COMPOSITING, SOFTWARE_COMPOSITING);
#endif

INSTANTIATE_TEST_SUITE_P(GLAndSoftwareCompositing,
                         CompositingRenderWidgetHostViewBrowserTest,
                         kTestCompositingModes);
INSTANTIATE_TEST_SUITE_P(GLAndSoftwareCompositing,
                         CompositingRenderWidgetHostViewBrowserTestTabCapture,
                         kTestCompositingModes);
INSTANTIATE_TEST_SUITE_P(
    GLAndSoftwareCompositing,
    CompositingRenderWidgetHostViewBrowserTestTabCaptureHighDPI,
    kTestCompositingModes);
INSTANTIATE_TEST_SUITE_P(GLAndSoftwareCompositing,
                         CompositingRenderWidgetHostViewBrowserTestHiDPI,
                         kTestCompositingModes);

#endif  // !defined(OS_ANDROID)

}  // namespace
}  // namespace content
