// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>
#include "build/build_config.h"

#include "base/path_service.h"
#include "content/browser/media/media_browsertest.h"
#include "content/browser/media/media_web_contents_observer.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"

namespace content {

namespace {

enum class FullscreenTestEvent {
  kEnterFullscreen,           // Some element in the tab goes fullscreen
  kLeaveFullscreen,           // Tab's element stopped being fullscreen
  kEffectivelyFullscreen,     // <video> gets effective fullscreen flag set.
  kNotEffectivelyFullscreen,  // <video> gets effective fullscreen flag unset.
  kInvalidEvent = 0xBADF00D
};

class FullscreenEventsRecorder : public WebContentsObserver {
 public:
  explicit FullscreenEventsRecorder(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  FullscreenEventsRecorder(const FullscreenEventsRecorder&) = delete;
  FullscreenEventsRecorder& operator=(const FullscreenEventsRecorder&) = delete;

  void MediaEffectivelyFullscreenChanged(bool value) override {
    AddEvent(value ? FullscreenTestEvent::kEffectivelyFullscreen
                   : FullscreenTestEvent::kNotEffectivelyFullscreen);
  }

  void DidToggleFullscreenModeForTab(bool entered_fullscreen,
                                     bool will_cause_resize) override {
    AddEvent(entered_fullscreen ? FullscreenTestEvent::kEnterFullscreen
                                : FullscreenTestEvent::kLeaveFullscreen);
  }

  void Wait(size_t events_count) {
    if (events_.size() >= events_count)
      return;
    expected_event_count_ = events_count;
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  FullscreenTestEvent event(size_t index) {
    if (index >= events_.size())
      return FullscreenTestEvent::kInvalidEvent;
    return events_[index];
  }

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
  size_t expected_event_count_ = 0;
  FullscreenTestEvent last_event_ = FullscreenTestEvent::kInvalidEvent;
  std::vector<FullscreenTestEvent> events_;

  void AddEvent(FullscreenTestEvent e) {
    if (last_event_ == e)
      return;
    events_.push_back(e);
    last_event_ = e;
    if (events_.size() == expected_event_count_ && run_loop_)
      run_loop_->Quit();
  }
};

}  // namespace

class FullscreenDetectionTest : public ContentBrowserTest {
 public:
  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    base::FilePath test_data_dir;
    ASSERT_TRUE(base::PathService::Get(content::DIR_TEST_DATA, &test_data_dir));
    embedded_test_server()->ServeFilesFromDirectory(test_data_dir);
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

// The most basic test possible and most obvious case of fullscreeness.
// A <video> itself goes fullscreen, and it's marked as effectively-fullscreen.
// Once the <video> exits fullscreen mode, it's not effectively-fullscreen
// any more.
IN_PROC_BROWSER_TEST_F(FullscreenDetectionTest, RegularVideoTagFullscreen) {
  auto* web_contents = shell()->web_contents();
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/media/fullscreen.html")));

  FullscreenEventsRecorder recorder(web_contents);
  ASSERT_TRUE(content::ExecJs(web_contents, "makeFullscreen('small_video')"));
  recorder.Wait(2);
  EXPECT_EQ(recorder.event(0), FullscreenTestEvent::kEnterFullscreen);
  EXPECT_EQ(recorder.event(1), FullscreenTestEvent::kEffectivelyFullscreen);

  EXPECT_TRUE(content::ExecJs(web_contents, "exitFullscreen()"));
  recorder.Wait(4);
  EXPECT_EQ(recorder.event(2), FullscreenTestEvent::kLeaveFullscreen);
  EXPECT_EQ(recorder.event(3), FullscreenTestEvent::kNotEffectivelyFullscreen);
}

// A <div>, containing a big <video>, goes fullscreen.
// This div effectively become the viewport.
// Since the <video> occupies most of the <div>, and thus most of the screen,
// it's marked as effectively-fullscreen.
// Once the <div> exits fullscreen mode, <video> is not effectively-fullscreen
// any more.
IN_PROC_BROWSER_TEST_F(FullscreenDetectionTest, EncompassingDivFullscreen) {
  auto* web_contents = shell()->web_contents();
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/media/fullscreen.html")));

  FullscreenEventsRecorder recorder(web_contents);

  ASSERT_TRUE(content::ExecJs(web_contents, "makeFullscreen('big_div')"));

  recorder.Wait(2);
  EXPECT_EQ(recorder.event(0), FullscreenTestEvent::kEnterFullscreen);
  EXPECT_EQ(recorder.event(1), FullscreenTestEvent::kEffectivelyFullscreen);

  ASSERT_TRUE(content::ExecJs(web_contents, "exitFullscreen()"));
  recorder.Wait(4);
  EXPECT_EQ(recorder.event(2), FullscreenTestEvent::kLeaveFullscreen);
  EXPECT_EQ(recorder.event(3), FullscreenTestEvent::kNotEffectivelyFullscreen);
}

// A <div>, containing a <video>, goes fullscreen.
// But the <video> occupies a small part of <div> (and viewport), and
// it is not enough to be counted as effectively-fullscreen.
IN_PROC_BROWSER_TEST_F(FullscreenDetectionTest, EncompassingDivNotFullscreen) {
  auto* web_contents = shell()->web_contents();
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/media/fullscreen.html")));

  FullscreenEventsRecorder recorder(web_contents);

#if BUILDFLAG(IS_ANDROID)
  RenderWidgetHostImpl* rwh = static_cast<RenderWidgetHostImpl*>(
      web_contents->GetRenderViewHost()->GetWidget());
  gfx::Size compositor_viewport_pixel_rect =
      rwh->GetVisualProperties().compositor_viewport_pixel_rect.size();
#endif

  ASSERT_TRUE(content::ExecJs(web_contents, "makeFullscreen('small_div')"));

  recorder.Wait(1);
  EXPECT_EQ(recorder.event(0), FullscreenTestEvent::kEnterFullscreen);

#if BUILDFLAG(IS_ANDROID)
  // Android fullscreen transitions causes layout changes, which triggers
  // SurfaceSync. We should confirm the fullscreen frames have been produced
  // before exiting.
  RenderFrameSubmissionObserver rfm_observer(web_contents);
  while (rwh->GetVisualProperties().compositor_viewport_pixel_rect.size() ==
         compositor_viewport_pixel_rect) {
    rfm_observer.WaitForMetadataChange();
  }

  viz::LocalSurfaceId target = static_cast<RenderWidgetHostViewBase*>(
                                   web_contents->GetRenderWidgetHostView())
                                   ->GetLocalSurfaceId();
  while (!rfm_observer.LastRenderFrameMetadata()
              .local_surface_id->IsSameOrNewerThan(target)) {
    rfm_observer.WaitForMetadataChange();
  }
#endif

  ASSERT_TRUE(content::ExecJs(web_contents, "exitFullscreen()"));
  recorder.Wait(2);
  EXPECT_EQ(recorder.event(1), FullscreenTestEvent::kLeaveFullscreen);
}

// A <div>, containing a <video>, goes fullscreen.
// The test changes size of the <video> relative to the viewport and observes
// how it gets and looses effectively-fullscreen status.
// This somewhat emulates YT scrolling in the fullscreen mode.
IN_PROC_BROWSER_TEST_F(FullscreenDetectionTest, VideoTagSizeChange) {
  auto* web_contents = shell()->web_contents();
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/media/fullscreen.html")));

  FullscreenEventsRecorder recorder(web_contents);

  ASSERT_TRUE(content::ExecJs(web_contents, "makeFullscreen('small_div')"));

  recorder.Wait(1);
  EXPECT_EQ(recorder.event(0), FullscreenTestEvent::kEnterFullscreen);

  ASSERT_TRUE(content::ExecJs(web_contents, "makeBig('small_video')"));
  recorder.Wait(2);
  EXPECT_EQ(recorder.event(1), FullscreenTestEvent::kEffectivelyFullscreen);

  ASSERT_TRUE(content::ExecJs(web_contents, "makeSmall('small_video')"));
  recorder.Wait(3);
  EXPECT_EQ(recorder.event(2), FullscreenTestEvent::kNotEffectivelyFullscreen);

  ASSERT_TRUE(content::ExecJs(web_contents, "makePortrait('small_video')"));
  recorder.Wait(4);
  EXPECT_EQ(recorder.event(3), FullscreenTestEvent::kEffectivelyFullscreen);

  ASSERT_TRUE(content::ExecJs(web_contents, "exitFullscreen()"));
  recorder.Wait(5);
  EXPECT_EQ(recorder.event(4), FullscreenTestEvent::kLeaveFullscreen);
}

// Test how attaching/detaching the <video> affects its fullscreen status.
IN_PROC_BROWSER_TEST_F(FullscreenDetectionTest, DetachAttachDuringFullscreen) {
  auto* web_contents = shell()->web_contents();
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/media/fullscreen.html")));

  FullscreenEventsRecorder recorder(web_contents);

  ASSERT_TRUE(content::ExecJs(web_contents, "makeFullscreen('big_div')"));

  recorder.Wait(2);
  EXPECT_EQ(recorder.event(0), FullscreenTestEvent::kEnterFullscreen);
  EXPECT_EQ(recorder.event(1), FullscreenTestEvent::kEffectivelyFullscreen);

  ASSERT_TRUE(content::ExecJs(web_contents, "detach('big_video')"));
  recorder.Wait(3);
  EXPECT_EQ(recorder.event(2), FullscreenTestEvent::kNotEffectivelyFullscreen);

  ASSERT_TRUE(content::ExecJs(web_contents, "attach_to('big_div')"));
  recorder.Wait(4);
  EXPECT_EQ(recorder.event(3), FullscreenTestEvent::kEffectivelyFullscreen);

  ASSERT_TRUE(content::ExecJs(web_contents, "exitFullscreen()"));
  recorder.Wait(6);
  EXPECT_EQ(recorder.event(4), FullscreenTestEvent::kLeaveFullscreen);
  EXPECT_EQ(recorder.event(5), FullscreenTestEvent::kNotEffectivelyFullscreen);
}

// The test changes visibility of the <video> and observes
// how it gets and loses effectively-fullscreen status.
// TODO(crbug.com/40857652): Re-enable this test
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID)
#define MAYBE_HideVideoTag DISABLED_HideVideoTag
#else
#define MAYBE_HideVideoTag HideVideoTag
#endif
IN_PROC_BROWSER_TEST_F(FullscreenDetectionTest, MAYBE_HideVideoTag) {
  auto* web_contents = shell()->web_contents();
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/media/fullscreen.html")));

  FullscreenEventsRecorder recorder(web_contents);

  ASSERT_TRUE(content::ExecJs(web_contents, "makeFullscreen('big_div')"));

  recorder.Wait(2);
  EXPECT_EQ(recorder.event(0), FullscreenTestEvent::kEnterFullscreen);
  EXPECT_EQ(recorder.event(1), FullscreenTestEvent::kEffectivelyFullscreen);

  ASSERT_TRUE(content::ExecJs(web_contents, "hide('big_video')"));
  recorder.Wait(3);
  EXPECT_EQ(recorder.event(2), FullscreenTestEvent::kNotEffectivelyFullscreen);

  ASSERT_TRUE(content::ExecJs(web_contents, "makeBig('big_video')"));
  recorder.Wait(4);
  EXPECT_EQ(recorder.event(3), FullscreenTestEvent::kEffectivelyFullscreen);

  ASSERT_TRUE(content::ExecJs(web_contents, "exitFullscreen()"));
  recorder.Wait(6);
  EXPECT_EQ(recorder.event(4), FullscreenTestEvent::kLeaveFullscreen);
  EXPECT_EQ(recorder.event(5), FullscreenTestEvent::kNotEffectivelyFullscreen);
}

}  // namespace content
