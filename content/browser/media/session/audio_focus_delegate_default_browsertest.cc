// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "content/browser/media/session/media_session_impl.h"
#include "content/browser/media/session/mock_media_session_player_observer.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"
#include "media/base/media_content_type.h"
#include "services/media_session/public/cpp/switches.h"
#include "services/media_session/public/cpp/test/audio_focus_test_util.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "services/media_session/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace content {

using media_session::test::TestAudioFocusObserver;

namespace {

const char kExpectedSourceName[] = "web";

}  // namespace

class MediaSessionStateObserver
    : public media_session::mojom::MediaSessionObserver {
 public:
  explicit MediaSessionStateObserver(
      media_session::mojom::MediaSession* media_session)
      : binding_(this) {
    media_session::mojom::MediaSessionObserverPtr observer;
    binding_.Bind(mojo::MakeRequest(&observer));
    media_session->AddObserver(std::move(observer));
  }

  ~MediaSessionStateObserver() override = default;

  // media_session::mojom::MediaSessionObserver
  void MediaSessionInfoChanged(
      media_session::mojom::MediaSessionInfoPtr session_info) override {
    if (desired_state_.has_value() &&
        desired_state_.value() == session_info->state) {
      run_loop_.Quit();
      return;
    }

    session_info_ = std::move(session_info);
  }

  void WaitForState(
      media_session::mojom::MediaSessionInfo::SessionState state) {
    if (session_info_ && state == session_info_->state)
      return;

    desired_state_ = state;
    run_loop_.Run();
  }

 protected:
  base::RunLoop run_loop_;
  mojo::Binding<media_session::mojom::MediaSessionObserver> binding_;
  base::Optional<media_session::mojom::MediaSessionInfo::SessionState>
      desired_state_;
  media_session::mojom::MediaSessionInfoPtr session_info_;

  DISALLOW_COPY_AND_ASSIGN(MediaSessionStateObserver);
};

class AudioFocusDelegateDefaultBrowserTest : public ContentBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    service_manager::Connector* connector =
        ServiceManagerConnection::GetForProcess()->GetConnector();
    connector->BindInterface(media_session::mojom::kServiceName,
                             mojo::MakeRequest(&audio_focus_ptr_));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(media_session::switches::kEnableAudioFocus);
  }

  void CheckSessionSourceName() {
    audio_focus_ptr_->GetFocusRequests(base::BindOnce(
        [](std::vector<media_session::mojom::AudioFocusRequestStatePtr>
               requests) {
          for (auto& request : requests)
            EXPECT_EQ(kExpectedSourceName, request->source_name.value());
        }));

    audio_focus_ptr_.FlushForTesting();
  }

  void Run(WebContents* start_contents, WebContents* interrupt_contents) {
    std::unique_ptr<MockMediaSessionPlayerObserver>
        player_observer(new MockMediaSessionPlayerObserver);

    MediaSessionImpl* media_session = MediaSessionImpl::Get(start_contents);
    EXPECT_TRUE(media_session);

    MediaSessionImpl* other_media_session =
        MediaSessionImpl::Get(interrupt_contents);
    EXPECT_TRUE(other_media_session);

    player_observer->StartNewPlayer();

    {
      std::unique_ptr<TestAudioFocusObserver> observer = CreateObserver();
      media_session->AddPlayer(player_observer.get(), 0,
                               media::MediaContentType::Persistent);
      observer->WaitForGainedEvent();
    }

    {
      MediaSessionStateObserver state_observer(media_session);
      state_observer.WaitForState(
          media_session::mojom::MediaSessionInfo::SessionState::kActive);
    }

    {
      MediaSessionStateObserver state_observer(other_media_session);
      state_observer.WaitForState(
          media_session::mojom::MediaSessionInfo::SessionState::kInactive);
    }

    CheckSessionSourceName();

    player_observer->StartNewPlayer();

    {
      std::unique_ptr<TestAudioFocusObserver> observer = CreateObserver();
      other_media_session->AddPlayer(player_observer.get(), 1,
                                     media::MediaContentType::Persistent);
      observer->WaitForGainedEvent();
    }

    {
      MediaSessionStateObserver state_observer(media_session);
      state_observer.WaitForState(
          media_session::mojom::MediaSessionInfo::SessionState::kSuspended);
    }

    {
      MediaSessionStateObserver state_observer(other_media_session);
      state_observer.WaitForState(
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
      MediaSessionStateObserver state_observer(media_session);
      state_observer.WaitForState(
          media_session::mojom::MediaSessionInfo::SessionState::kInactive);
    }

    {
      MediaSessionStateObserver state_observer(other_media_session);
      state_observer.WaitForState(
          media_session::mojom::MediaSessionInfo::SessionState::kInactive);
    }
  }

 private:
  std::unique_ptr<TestAudioFocusObserver> CreateObserver() {
    std::unique_ptr<TestAudioFocusObserver> observer =
        std::make_unique<TestAudioFocusObserver>();
    media_session::mojom::AudioFocusObserverPtr observer_ptr;
    observer->BindToMojoRequest(mojo::MakeRequest(&observer_ptr));
    audio_focus_ptr_->AddObserver(std::move(observer_ptr));
    audio_focus_ptr_.FlushForTesting();
    return observer;
  }

  media_session::mojom::AudioFocusManagerPtr audio_focus_ptr_;
};

// Two windows from the same BrowserContext.
IN_PROC_BROWSER_TEST_F(AudioFocusDelegateDefaultBrowserTest,
                       ActiveWebContentsPauseOthers) {
  Run(shell()->web_contents(), CreateBrowser()->web_contents());
}

// Regular BrowserContext is interrupted by OffTheRecord one.
IN_PROC_BROWSER_TEST_F(AudioFocusDelegateDefaultBrowserTest,
                       RegularBrowserInterruptsOffTheRecord) {
  Run(shell()->web_contents(), CreateOffTheRecordBrowser()->web_contents());
}

// OffTheRecord BrowserContext is interrupted by regular one.
IN_PROC_BROWSER_TEST_F(AudioFocusDelegateDefaultBrowserTest,
                       OffTheRecordInterruptsRegular) {
  Run(CreateOffTheRecordBrowser()->web_contents(), shell()->web_contents());
}

}  // namespace content
