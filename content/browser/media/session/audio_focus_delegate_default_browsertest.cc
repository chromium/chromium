// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "content/browser/media/session/media_session_impl.h"
#include "content/browser/media/session/mock_media_session_player_observer.h"
#include "content/public/browser/media_session_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"
#include "media/base/media_content_type.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/media_session/public/cpp/features.h"
#include "services/media_session/public/cpp/test/audio_focus_test_util.h"
#include "services/media_session/public/cpp/test/mock_media_session.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"

namespace content {

using media_session::test::TestAudioFocusObserver;

namespace {

const char kExpectedSourceName[] = "web";

}  // namespace

class AudioFocusDelegateDefaultBrowserTest : public ContentBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    GetMediaSessionService().BindAudioFocusManager(
        audio_focus_.BindNewPipeAndPassReceiver());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    scoped_feature_list_.InitWithFeatures(
        {media_session::features::kMediaSessionService,
         media_session::features::kAudioFocusEnforcement},
        {});
  }

  void CheckSessionSourceName() {
    audio_focus_->GetFocusRequests(base::BindOnce(
        [](std::vector<media_session::mojom::AudioFocusRequestStatePtr>
               requests) {
          for (auto& request : requests)
            EXPECT_EQ(kExpectedSourceName, request->source_name.value());
        }));

    audio_focus_.FlushForTesting();
  }

  void Run(WebContents* start_contents,
           WebContents* interrupt_contents,
           bool use_separate_group_id) {
    std::unique_ptr<MockMediaSessionPlayerObserver> player_observer(
        new MockMediaSessionPlayerObserver(
            nullptr, media::MediaContentType::kPersistent));

    MediaSessionImpl* media_session = MediaSessionImpl::Get(start_contents);
    EXPECT_TRUE(media_session);

    MediaSessionImpl* other_media_session =
        MediaSessionImpl::Get(interrupt_contents);
    EXPECT_TRUE(other_media_session);

    if (use_separate_group_id)
      other_media_session->SetAudioFocusGroupId(
          base::UnguessableToken::Create());

    player_observer->StartNewPlayer();

    {
      std::unique_ptr<TestAudioFocusObserver> observer = CreateObserver();
      media_session->AddPlayer(player_observer.get(), 0);
      observer->WaitForGainedEvent();
    }

    {
      media_session::test::MockMediaSessionMojoObserver observer(
          *media_session);
      observer.WaitForState(
          media_session::mojom::MediaSessionInfo::SessionState::kActive);
    }

    {
      media_session::test::MockMediaSessionMojoObserver observer(
          *other_media_session);
      observer.WaitForState(
          media_session::mojom::MediaSessionInfo::SessionState::kInactive);
    }

    CheckSessionSourceName();

    player_observer->StartNewPlayer();

    {
      std::unique_ptr<TestAudioFocusObserver> observer = CreateObserver();
      other_media_session->AddPlayer(player_observer.get(), 1);
      observer->WaitForGainedEvent();
    }

    {
      media_session::test::MockMediaSessionMojoObserver observer(
          *media_session);
      observer.WaitForState(
          use_separate_group_id ||
                  !base::FeatureList::IsEnabled(
                      media_session::features::kAudioFocusSessionGrouping)
              ? media_session::mojom::MediaSessionInfo::SessionState::kSuspended
              : media_session::mojom::MediaSessionInfo::SessionState::kActive);
    }

    {
      media_session::test::MockMediaSessionMojoObserver observer(
          *other_media_session);
      observer.WaitForState(
          media_session::mojom::MediaSessionInfo::SessionState::kActive);
    }

    CheckSessionSourceName();

    {
      std::unique_ptr<TestAudioFocusObserver> observer = CreateObserver();
      media_session->Stop(MediaSessionImpl::SuspendType::kUI);
      other_media_session->Stop(MediaSessionImpl::SuspendType::kUI);
      observer->WaitForLostEvent();
    }

    {
      media_session::test::MockMediaSessionMojoObserver observer(
          *media_session);
      observer.WaitForState(
          media_session::mojom::MediaSessionInfo::SessionState::kInactive);
    }

    {
      media_session::test::MockMediaSessionMojoObserver observer(
          *other_media_session);
      observer.WaitForState(
          media_session::mojom::MediaSessionInfo::SessionState::kInactive);
    }
  }

 private:
  std::unique_ptr<TestAudioFocusObserver> CreateObserver() {
    std::unique_ptr<TestAudioFocusObserver> observer =
        std::make_unique<TestAudioFocusObserver>();
    audio_focus_->AddObserver(observer->BindNewPipeAndPassRemote());
    audio_focus_.FlushForTesting();
    return observer;
  }

  mojo::Remote<media_session::mojom::AudioFocusManager> audio_focus_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Two windows from the same BrowserContext.
IN_PROC_BROWSER_TEST_F(AudioFocusDelegateDefaultBrowserTest,
                       ActiveWebContentsPausesOthers) {
  Run(shell()->web_contents(), CreateBrowser()->web_contents(), false);
}

// Two windows with different group ids.
IN_PROC_BROWSER_TEST_F(AudioFocusDelegateDefaultBrowserTest,
                       ActiveWebContentsPausesOtherWithGroupId) {
  Run(shell()->web_contents(), CreateBrowser()->web_contents(), true);
}

// Regular BrowserContext is interrupted by OffTheRecord one.
IN_PROC_BROWSER_TEST_F(AudioFocusDelegateDefaultBrowserTest,
                       RegularBrowserInterruptsOffTheRecord) {
  Run(shell()->web_contents(), CreateOffTheRecordBrowser()->web_contents(),
      false);
}

// OffTheRecord BrowserContext is interrupted by regular one.
IN_PROC_BROWSER_TEST_F(AudioFocusDelegateDefaultBrowserTest,
                       OffTheRecordInterruptsRegular) {
  Run(CreateOffTheRecordBrowser()->web_contents(), shell()->web_contents(),
      false);
}

}  // namespace content
