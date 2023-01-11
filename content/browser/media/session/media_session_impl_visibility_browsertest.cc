// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/media/session/media_session_impl.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "media/base/media_switches.h"
#include "services/media_session/public/cpp/features.h"
#include "services/media_session/public/cpp/test/mock_media_session.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

using media_session::mojom::MediaSessionInfo;

namespace {
static const char kStartPlayerScript[] =
    "document.getElementById('long-video').play()";
static const char kPausePlayerScript[] =
    "document.getElementById('long-video').pause()";

enum class MediaSuspend {
  ENABLED,
  DISABLED,
};

enum class BackgroundResuming {
  ENABLED,
  DISABLED,
};

struct VisibilityTestData {
  MediaSuspend media_suspend;
  BackgroundResuming background_resuming;
  MediaSessionInfo::SessionState session_state_before_hide;
  MediaSessionInfo::SessionState session_state_after_hide;
};
}

// Base class of MediaSession visibility tests. The class is intended
// to be used to run tests under different configurations. Tests
// should inheret from this class, set up their own command line per
// their configuration, and use macro INCLUDE_TEST_FROM_BASE_CLASS to
// include required tests. See
// media_session_visibility_browsertest_instances.cc for examples.
class MediaSessionImplVisibilityBrowserTest
    : public ContentBrowserTest,
      public ::testing::WithParamInterface<VisibilityTestData> {
 public:
  MediaSessionImplVisibilityBrowserTest() {
    VisibilityTestData params = GetVisibilityTestData();
    EnableDisableResumingBackgroundVideos(params.background_resuming ==
                                          BackgroundResuming::ENABLED);
  }

  MediaSessionImplVisibilityBrowserTest(
      const MediaSessionImplVisibilityBrowserTest&) = delete;
  MediaSessionImplVisibilityBrowserTest& operator=(
      const MediaSessionImplVisibilityBrowserTest&) = delete;

  ~MediaSessionImplVisibilityBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ms_feature_list_.InitAndEnableFeature(
        media_session::features::kMediaSessionService);

    ContentBrowserTest::SetUpOnMainThread();
    web_contents_ = shell()->web_contents();
    media_session_ = MediaSessionImpl::Get(web_contents_);
  }

  void EnableDisableResumingBackgroundVideos(bool enable) {
    if (enable)
      scoped_feature_list_.InitAndEnableFeature(media::kResumeBackgroundVideo);
    else
      scoped_feature_list_.InitAndDisableFeature(media::kResumeBackgroundVideo);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        switches::kAutoplayPolicy,
        switches::autoplay::kNoUserGestureRequiredPolicy);

    VisibilityTestData params = GetVisibilityTestData();

    if (params.media_suspend == MediaSuspend::DISABLED)
      command_line->AppendSwitch(switches::kDisableBackgroundMediaSuspend);
  }

  const VisibilityTestData& GetVisibilityTestData() {
    return GetParam();
  }

  void StartPlayer() {
    LoadTestPage();
    RunScript(kStartPlayerScript);
    WaitForMediaSessionState(MediaSessionInfo::SessionState::kActive);
  }

  // Maybe pause the player depending on whether the session state before hide
  // is SUSPENDED.
  void MaybePausePlayer() {
    ASSERT_TRUE(GetVisibilityTestData().session_state_before_hide !=
                MediaSessionInfo::SessionState::kInactive);
    if (GetVisibilityTestData().session_state_before_hide ==
        MediaSessionInfo::SessionState::kActive)
      return;

    RunScript(kPausePlayerScript);
    WaitForMediaSessionState(MediaSessionInfo::SessionState::kSuspended);
  }

  void HideTab() {
    web_contents_->WasHidden();
  }

  void CheckSessionStateAfterHide() {
    MediaSessionInfo::SessionState state_before_hide =
        GetVisibilityTestData().session_state_before_hide;
    MediaSessionInfo::SessionState state_after_hide =
        GetVisibilityTestData().session_state_after_hide;

    if (state_before_hide == state_after_hide) {
      Wait(base::Seconds(1));
      ASSERT_EQ(state_after_hide,
                media_session_->GetMediaSessionInfoSync()->state);
    } else {
      WaitForMediaSessionState(state_after_hide);
    }
  }

 private:
  void LoadTestPage() {
    TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
    shell()->LoadURL(GetTestUrl("media/session", "media-session.html"));
    navigation_observer.Wait();
  }

  void RunScript(const std::string& script) {
    ASSERT_TRUE(ExecJs(web_contents_->GetPrimaryMainFrame(), script));
  }

  // TODO(zqzhang): This method is shared with
  // MediaRouterIntegrationTests. Move it into a general place.
  void Wait(base::TimeDelta timeout) {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), timeout);
    run_loop.Run();
  }

  void WaitForMediaSessionState(MediaSessionInfo::SessionState state) {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);
    observer.WaitForState(state);
  }

  base::test::ScopedFeatureList ms_feature_list_;
  base::test::ScopedFeatureList scoped_feature_list_;

  raw_ptr<WebContents> web_contents_;
  raw_ptr<MediaSessionImpl> media_session_;
};

namespace {

VisibilityTestData kTestParams[] = {
    {MediaSuspend::ENABLED, BackgroundResuming::DISABLED,
     MediaSessionInfo::SessionState::kSuspended,
     MediaSessionInfo::SessionState::kInactive},
    {MediaSuspend::ENABLED, BackgroundResuming::DISABLED,
     MediaSessionInfo::SessionState::kActive,
     MediaSessionInfo::SessionState::kInactive},
    {MediaSuspend::ENABLED, BackgroundResuming::ENABLED,
     MediaSessionInfo::SessionState::kActive,
     MediaSessionInfo::SessionState::kSuspended},
    {MediaSuspend::ENABLED, BackgroundResuming::ENABLED,
     MediaSessionInfo::SessionState::kSuspended,
     MediaSessionInfo::SessionState::kSuspended},
    {MediaSuspend::DISABLED, BackgroundResuming::DISABLED,
     MediaSessionInfo::SessionState::kSuspended,
     MediaSessionInfo::SessionState::kSuspended},
    {MediaSuspend::DISABLED, BackgroundResuming::DISABLED,
     MediaSessionInfo::SessionState::kActive,
     MediaSessionInfo::SessionState::kActive},
    {MediaSuspend::DISABLED, BackgroundResuming::ENABLED,
     MediaSessionInfo::SessionState::kActive,
     MediaSessionInfo::SessionState::kActive},
    {MediaSuspend::DISABLED, BackgroundResuming::ENABLED,
     MediaSessionInfo::SessionState::kSuspended,
     MediaSessionInfo::SessionState::kSuspended},
};

}  // anonymous namespace

IN_PROC_BROWSER_TEST_P(MediaSessionImplVisibilityBrowserTest,
                       DISABLED_TestEntryPoint) {
  StartPlayer();
  MaybePausePlayer();
  HideTab();
  CheckSessionStateAfterHide();
}

INSTANTIATE_TEST_SUITE_P(MediaSessionImplVisibilityBrowserTestInstances,
                         MediaSessionImplVisibilityBrowserTest,
                         ::testing::ValuesIn(kTestParams));

}  // namespace content
