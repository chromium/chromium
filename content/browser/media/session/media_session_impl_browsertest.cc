// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/session/media_session_impl.h"

#include <stddef.h>

#include <list>
#include <memory>
#include <vector>

#include "base/atomic_sequence_num.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_samples.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/media/session/audio_focus_delegate.h"
#include "content/browser/media/session/mock_media_session_player_observer.h"
#include "content/browser/media/session/mock_media_session_service_impl.h"
#include "content/browser/renderer_host/back_forward_cache_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "media/base/media_content_type.h"
#include "net/base/filename_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/media_session/public/cpp/test/mock_media_session.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"

using media_session::mojom::AudioFocusType;
using media_session::mojom::MediaPlaybackState;
using media_session::mojom::MediaSessionInfo;

using ::testing::_;
using ::testing::Eq;
using ::testing::Expectation;
using ::testing::NiceMock;

namespace {

const double kDefaultVolumeMultiplier = 1.0;
const double kDuckingVolumeMultiplier = 0.2;
const double kDifferentDuckingVolumeMultiplier = 0.018;

const std::u16string kExpectedSourceTitlePrefix = u"http://example.com:";

constexpr gfx::Size kDefaultFaviconSize = gfx::Size(16, 16);

const std::string kExampleSinkId = "example_device_id";

class MockAudioFocusDelegate : public content::AudioFocusDelegate {
 public:
  MockAudioFocusDelegate(content::MediaSessionImpl* media_session,
                         bool async_mode)
      : media_session_(media_session), async_mode_(async_mode) {}

  MOCK_METHOD0(AbandonAudioFocus, void());

  AudioFocusDelegate::AudioFocusResult RequestAudioFocus(
      AudioFocusType audio_focus_type) override {
    if (async_mode_) {
      requests_.push_back(audio_focus_type);
      return AudioFocusDelegate::AudioFocusResult::kDelayed;
    } else {
      audio_focus_type_ = audio_focus_type;
      return sync_result_;
    }
  }

  std::optional<AudioFocusType> GetCurrentFocusType() const override {
    return audio_focus_type_;
  }

  void MediaSessionInfoChanged(
      const media_session::mojom::MediaSessionInfoPtr& session_info) override {}

  MOCK_CONST_METHOD0(request_id, const base::UnguessableToken&());

  MOCK_METHOD(void, ReleaseRequestId, (), (override));

  void ResolveRequest(bool result) {
    if (!async_mode_) {
      return;
    }

    audio_focus_type_ = requests_.front();
    requests_.pop_front();

    media_session_->FinishSystemAudioFocusRequest(audio_focus_type_.value(),
                                                  result);
  }

  bool HasRequests() const { return !requests_.empty(); }

  void SetSyncResult(AudioFocusDelegate::AudioFocusResult result) {
    sync_result_ = result;
  }

 private:
  AudioFocusDelegate::AudioFocusResult sync_result_ =
      AudioFocusDelegate::AudioFocusResult::kSuccess;

  raw_ptr<content::MediaSessionImpl> media_session_ = nullptr;
  const bool async_mode_ = false;

  std::list<AudioFocusType> requests_;
  std::optional<AudioFocusType> audio_focus_type_;
};

// Helper class to mock `GetVisibility` callbacks.
class GetVisibilityWaiter {
 public:
  using GetVisibilityCallback = base::OnceCallback<void(bool)>;

  GetVisibilityWaiter() = default;
  GetVisibilityWaiter(const GetVisibilityWaiter&) = delete;
  GetVisibilityWaiter(GetVisibilityWaiter&&) = delete;
  GetVisibilityWaiter& operator=(const GetVisibilityWaiter&) = delete;

  GetVisibilityCallback VisibilityCallback() {
    meets_visibility_ = std::nullopt;
    weak_factory_.InvalidateWeakPtrs();

    // base::Unretained() is safe since no further tasks can run after
    // RunLoop::Run() returns.
    return base::BindOnce(&GetVisibilityWaiter::GetVisibility,
                          weak_factory_.GetWeakPtr());
  }

  void WaitUntilDone() {
    if (meets_visibility_) {
      return;
    }
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  bool MeetsVisibility() {
    DCHECK(meets_visibility_);
    return meets_visibility_.value();
  }

 private:
  void GetVisibility(bool meets_visibility) {
    meets_visibility_ = meets_visibility;
    if (run_loop_) {
      run_loop_->Quit();
    }
  }

  std::unique_ptr<base::RunLoop> run_loop_;
  std::optional<bool> meets_visibility_;
  base::WeakPtrFactory<GetVisibilityWaiter> weak_factory_{this};
};

}  // namespace

namespace content {

class MediaSessionImplBrowserTest : public ContentBrowserTest {
 public:
  MediaSessionImplBrowserTest(const MediaSessionImplBrowserTest&) = delete;
  MediaSessionImplBrowserTest& operator=(const MediaSessionImplBrowserTest&) =
      delete;

 protected:
  MediaSessionImplBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kMediaSessionEnterPictureInPicture);
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    // Navigate to a test page with a a real origin.
    ASSERT_TRUE(embedded_test_server()->Start());
    host_resolver()->AddRule("*", "127.0.0.1");
    EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL(
                                           "example.com", "/title1.html")));

    // Setup the favicon server.
    favicon_server().RegisterRequestHandler(base::BindRepeating(
        &MediaSessionImplBrowserTest::HandleRequest, base::Unretained(this)));
    ASSERT_TRUE(favicon_server().Start());

    media_session_ = MediaSessionImpl::Get(shell()->web_contents());
    mock_audio_focus_delegate_ = new NiceMock<MockAudioFocusDelegate>(
        media_session_, true /* async_mode */);
    media_session_->SetDelegateForTests(
        base::WrapUnique(mock_audio_focus_delegate_.get()));
    ASSERT_TRUE(media_session_);
  }

  void TearDownOnMainThread() override {
    media_session_->RemoveAllPlayersForTest();
    mock_media_session_service_.reset();

    mock_audio_focus_delegate_ = nullptr;
    media_session_ = nullptr;

    ContentBrowserTest::TearDownOnMainThread();
  }

  void StartNewPlayer(MockMediaSessionPlayerObserver* player_observer) {
    int player_id = player_observer->StartNewPlayer();

    bool result = AddPlayer(player_observer, player_id);

    EXPECT_TRUE(result);
  }

  bool AddPlayer(MockMediaSessionPlayerObserver* player_observer,
                 int player_id) {
    return media_session_->AddPlayer(player_observer, player_id);
  }

  void RemovePlayer(MockMediaSessionPlayerObserver* player_observer,
                    int player_id) {
    media_session_->RemovePlayer(player_observer, player_id);
  }

  void RemovePlayers(MockMediaSessionPlayerObserver* player_observer) {
    media_session_->RemovePlayers(player_observer);
  }

  void OnPlayerPaused(MockMediaSessionPlayerObserver* player_observer,
                      int player_id) {
    media_session_->OnPlayerPaused(player_observer, player_id);
  }

  void SetPosition(MockMediaSessionPlayerObserver* player_observer,
                   int player_id,
                   media_session::MediaPosition& position) {
    player_observer->SetPosition(player_id, position);
    media_session_->RebuildAndNotifyMediaPositionChanged();
  }

  bool IsActive() { return media_session_->IsActive(); }

  std::optional<AudioFocusType> GetSessionAudioFocusType() {
    return mock_audio_focus_delegate_->GetCurrentFocusType();
  }

  bool IsControllable() { return media_session_->IsControllable(); }

  void UIResume() { media_session_->Resume(MediaSession::SuspendType::kUI); }

  void SystemResume() {
    media_session_->Resume(MediaSession::SuspendType::kSystem);
  }

  void UISuspend() { media_session_->Suspend(MediaSession::SuspendType::kUI); }

  void SystemSuspend(bool temporary) {
    media_session_->OnSuspendInternal(MediaSession::SuspendType::kSystem,
                                      temporary
                                          ? MediaSessionImpl::State::SUSPENDED
                                          : MediaSessionImpl::State::INACTIVE);
  }

  void UISeekForward() { media_session_->Seek(base::Seconds(1)); }

  void UISeekBackward() { media_session_->Seek(base::Seconds(-1)); }

  void UISeekTo() { media_session_->SeekTo(base::Seconds(10)); }

  void UIScrubTo() { media_session_->ScrubTo(base::Seconds(10)); }

  void UISetAudioSink(const std::string& sink_id) {
    media_session_->SetAudioSinkId(sink_id);
  }

  void UIGetVisibility(
      MediaSessionImpl::GetVisibilityCallback get_visibility_callback) {
    media_session_->GetVisibility(std::move(get_visibility_callback));
  }

  void SystemStartDucking() { media_session_->StartDucking(); }

  void SystemStopDucking() { media_session_->StopDucking(); }

  void EnsureMediaSessionService() {
    mock_media_session_service_ =
        std::make_unique<NiceMock<MockMediaSessionServiceImpl>>(
            shell()->web_contents()->GetPrimaryMainFrame());
  }

  void SetPlaybackState(blink::mojom::MediaSessionPlaybackState state) {
    mock_media_session_service_->SetPlaybackState(state);
  }

  void ResolveAudioFocusSuccess() {
    mock_audio_focus_delegate()->ResolveRequest(true /* result */);
  }

  void ResolveAudioFocusFailure() {
    mock_audio_focus_delegate()->ResolveRequest(false /* result */);
  }

  void SetSyncAudioFocusResult(AudioFocusDelegate::AudioFocusResult result) {
    mock_audio_focus_delegate()->SetSyncResult(result);
  }

  bool HasUnresolvedAudioFocusRequest() {
    return mock_audio_focus_delegate()->HasRequests();
  }

  MockAudioFocusDelegate* mock_audio_focus_delegate() {
    return mock_audio_focus_delegate_;
  }

  std::unique_ptr<MediaSessionImpl> CreateDummyMediaSession() {
    return base::WrapUnique<MediaSessionImpl>(
        new MediaSessionImpl(CreateBrowser()->web_contents()));
  }

  MediaSessionUmaHelper* GetMediaSessionUMAHelper() {
    return media_session_->uma_helper_for_test();
  }

  void SetAudioFocusDelegateForTests(MockAudioFocusDelegate* delegate) {
    mock_audio_focus_delegate_ = delegate;
    media_session_->SetDelegateForTests(
        base::WrapUnique(mock_audio_focus_delegate_.get()));
  }

  bool IsDucking() const { return media_session_->is_ducking_; }

  std::u16string GetExpectedSourceTitle() {
    std::u16string expected_title =
        base::StrCat({kExpectedSourceTitlePrefix,
                      base::NumberToString16(embedded_test_server()->port())});

    return expected_title.substr(strlen("http://"));
  }

  net::EmbeddedTestServer& favicon_server() { return favicon_server_; }

  int get_favicon_calls() { return favicon_calls_.GetNext(); }

 protected:
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    get_favicon_calls();
    return std::make_unique<net::test_server::BasicHttpResponse>();
  }

  raw_ptr<MediaSessionImpl> media_session_ = nullptr;
  raw_ptr<MockAudioFocusDelegate> mock_audio_focus_delegate_ = nullptr;
  std::unique_ptr<MockMediaSessionServiceImpl> mock_media_session_service_;
  net::EmbeddedTestServer favicon_server_;
  base::AtomicSequenceNumber favicon_calls_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

class MediaSessionImplParamBrowserTest
    : public MediaSessionImplBrowserTest,
      public testing::WithParamInterface<bool> {
 protected:
  MediaSessionImplParamBrowserTest() = default;

  void SetUpOnMainThread() override {
    MediaSessionImplBrowserTest::SetUpOnMainThread();

    SetAudioFocusDelegateForTests(
        new NiceMock<MockAudioFocusDelegate>(media_session_, GetParam()));
  }
};

class MediaSessionImplSyncBrowserTest : public MediaSessionImplBrowserTest {
 protected:
  MediaSessionImplSyncBrowserTest() = default;

  void SetUpOnMainThread() override {
    MediaSessionImplBrowserTest::SetUpOnMainThread();

    SetAudioFocusDelegateForTests(new NiceMock<MockAudioFocusDelegate>(
        media_session_, false /* async_mode */));
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         MediaSessionImplParamBrowserTest,
                         testing::Bool());

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       PlayersFromSameObserverDoNotStopEachOtherInSameSession) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);

  StartNewPlayer(player_observer.get());
  StartNewPlayer(player_observer.get());
  StartNewPlayer(player_observer.get());
  ResolveAudioFocusSuccess();

  EXPECT_TRUE(player_observer->IsPlaying(0));
  EXPECT_TRUE(player_observer->IsPlaying(1));
  EXPECT_TRUE(player_observer->IsPlaying(2));
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       PlayersFromManyObserverDoNotStopEachOtherInSameSession) {
  auto player_observer_1 = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);
  auto player_observer_2 = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);
  auto player_observer_3 = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);

  StartNewPlayer(player_observer_1.get());
  StartNewPlayer(player_observer_2.get());
  StartNewPlayer(player_observer_3.get());
  ResolveAudioFocusSuccess();

  EXPECT_TRUE(player_observer_1->IsPlaying(0));
  EXPECT_TRUE(player_observer_2->IsPlaying(0));
  EXPECT_TRUE(player_observer_3->IsPlaying(0));
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       SuspendedMediaSessionStopsPlayers) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);

  StartNewPlayer(player_observer.get());
  StartNewPlayer(player_observer.get());
  StartNewPlayer(player_observer.get());
  ResolveAudioFocusSuccess();

  SystemSuspend(true);

  EXPECT_FALSE(player_observer->IsPlaying(0));
  EXPECT_FALSE(player_observer->IsPlaying(1));
  EXPECT_FALSE(player_observer->IsPlaying(2));
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ResumedMediaSessionRestartsPlayers) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);

  StartNewPlayer(player_observer.get());
  StartNewPlayer(player_observer.get());
  StartNewPlayer(player_observer.get());
  ResolveAudioFocusSuccess();

  SystemSuspend(true);
  SystemResume();

  EXPECT_TRUE(player_observer->IsPlaying(0));
  EXPECT_TRUE(player_observer->IsPlaying(1));
  EXPECT_TRUE(player_observer->IsPlaying(2));
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       StartedPlayerOnSuspendedSessionPlaysAlone) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);

  StartNewPlayer(player_observer.get());
  ResolveAudioFocusSuccess();

  EXPECT_TRUE(player_observer->IsPlaying(0));

  SystemSuspend(true);

  EXPECT_FALSE(player_observer->IsPlaying(0));

  StartNewPlayer(player_observer.get());
  ResolveAudioFocusSuccess();

  EXPECT_FALSE(player_observer->IsPlaying(0));
  EXPECT_TRUE(player_observer->IsPlaying(1));

  StartNewPlayer(player_observer.get());

  EXPECT_FALSE(player_observer->IsPlaying(0));
  EXPECT_TRUE(player_observer->IsPlaying(1));
  EXPECT_TRUE(player_observer->IsPlaying(2));
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       InitialVolumeMultiplier) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);

  StartNewPlayer(player_observer.get());
  StartNewPlayer(player_observer.get());

  EXPECT_EQ(kDefaultVolumeMultiplier, player_observer->GetVolumeMultiplier(0));
  EXPECT_EQ(kDefaultVolumeMultiplier, player_observer->GetVolumeMultiplier(1));

  ResolveAudioFocusSuccess();

  EXPECT_EQ(kDefaultVolumeMultiplier, player_observer->GetVolumeMultiplier(0));
  EXPECT_EQ(kDefaultVolumeMultiplier, player_observer->GetVolumeMultiplier(1));
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       StartDuckingReducesVolumeMultiplier) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);

  StartNewPlayer(player_observer.get());
  StartNewPlayer(player_observer.get());
  SystemStartDucking();

  EXPECT_EQ(kDuckingVolumeMultiplier, player_observer->GetVolumeMultiplier(0));
  EXPECT_EQ(kDuckingVolumeMultiplier, player_observer->GetVolumeMultiplier(1));

  StartNewPlayer(player_observer.get());

  EXPECT_EQ(kDuckingVolumeMultiplier, player_observer->GetVolumeMultiplier(2));
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       StopDuckingRecoversVolumeMultiplier) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);

  StartNewPlayer(player_observer.get());
  StartNewPlayer(player_observer.get());
  SystemStartDucking();
  SystemStopDucking();

  EXPECT_EQ(kDefaultVolumeMultiplier, player_observer->GetVolumeMultiplier(0));
  EXPECT_EQ(kDefaultVolumeMultiplier, player_observer->GetVolumeMultiplier(1));

  StartNewPlayer(player_observer.get());

  EXPECT_EQ(kDefaultVolumeMultiplier, player_observer->GetVolumeMultiplier(2));
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       DuckingUsesConfiguredMultiplier) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);

  StartNewPlayer(player_observer.get());
  StartNewPlayer(player_observer.get());
  media_session_->SetDuckingVolumeMultiplier(kDifferentDuckingVolumeMultiplier);
  SystemStartDucking();
  EXPECT_EQ(kDifferentDuckingVolumeMultiplier,
            player_observer->GetVolumeMultiplier(0));
  EXPECT_EQ(kDifferentDuckingVolumeMultiplier,
            player_observer->GetVolumeMultiplier(1));
  SystemStopDucking();
  EXPECT_EQ(kDefaultVolumeMultiplier, player_observer->GetVolumeMultiplier(0));
  EXPECT_EQ(kDefaultVolumeMultiplier, player_observer->GetVolumeMultiplier(1));
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       AudioFocusInitialState) {
  EXPECT_FALSE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       AddPlayerOnSuspendedFocusUnducksWhenPlaybackRestarts) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);
  constexpr int player_id = 0;
  StartNewPlayer(player_observer.get());
  ResolveAudioFocusSuccess();

  UISuspend();
  EXPECT_FALSE(IsActive());

  SystemStartDucking();
  EXPECT_EQ(kDuckingVolumeMultiplier,
            player_observer->GetVolumeMultiplier(player_id));

  // On resume, ducking should stop.
  UIResume();
  ResolveAudioFocusSuccess();
  EXPECT_EQ(kDefaultVolumeMultiplier,
            player_observer->GetVolumeMultiplier(player_id));
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       CanRequestFocusBeforePlayerCreation) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);

  media_session_->RequestSystemAudioFocus(AudioFocusType::kGain);
  EXPECT_TRUE(IsActive());

  ResolveAudioFocusSuccess();
  EXPECT_TRUE(IsActive());

  StartNewPlayer(player_observer.get());
  EXPECT_TRUE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       StartPlayerGivesFocus) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);

  StartNewPlayer(player_observer.get());
  EXPECT_TRUE(IsActive());

  ResolveAudioFocusSuccess();
  EXPECT_TRUE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       SuspendGivesAwayAudioFocus) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);

  StartNewPlayer(player_observer.get());
  ResolveAudioFocusSuccess();

  SystemSuspend(true);

  EXPECT_FALSE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       StopGivesAwayAudioFocus) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);

  StartNewPlayer(player_observer.get());
  ResolveAudioFocusSuccess();

  media_session_->Stop(MediaSession::SuspendType::kUI);

  EXPECT_FALSE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       SystemResumeGivesBackAudioFocus) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);

  StartNewPlayer(player_observer.get());
  ResolveAudioFocusSuccess();

  SystemSuspend(true);
  SystemResume();

  EXPECT_TRUE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       UIResumeGivesBackAudioFocus) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);

  StartNewPlayer(player_observer.get());
  ResolveAudioFocusSuccess();

  UISuspend();

  UIResume();
  EXPECT_TRUE(IsActive());

  ResolveAudioFocusSuccess();
  EXPECT_TRUE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       RemovingLastPlayerDropsAudioFocus) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);

  StartNewPlayer(player_observer.get());
  StartNewPlayer(player_observer.get());
  StartNewPlayer(player_observer.get());
  ResolveAudioFocusSuccess();

  RemovePlayer(player_observer.get(), 0);
  EXPECT_TRUE(IsActive());
  RemovePlayer(player_observer.get(), 1);
  EXPECT_TRUE(IsActive());
  RemovePlayer(player_observer.get(), 2);
  EXPECT_FALSE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       RemovingLastPlayerFromManyObserversDropsAudioFocus) {
  auto player_observer_1 = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);
  auto player_observer_2 = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);
  auto player_observer_3 = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);

  StartNewPlayer(player_observer_1.get());
  StartNewPlayer(player_observer_2.get());
  StartNewPlayer(player_observer_3.get());
  ResolveAudioFocusSuccess();

  RemovePlayer(player_observer_1.get(), 0);
  EXPECT_TRUE(IsActive());
  RemovePlayer(player_observer_2.get(), 0);
  EXPECT_TRUE(IsActive());
  RemovePlayer(player_observer_3.get(), 0);
  EXPECT_FALSE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       RemovingAllPlayersFromObserversDropsAudioFocus) {
  auto player_observer_1 = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);
  auto player_observer_2 = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);

  StartNewPlayer(player_observer_1.get());
  StartNewPlayer(player_observer_1.get());
  StartNewPlayer(player_observer_2.get());
  StartNewPlayer(player_observer_2.get());
  ResolveAudioFocusSuccess();

  RemovePlayers(player_observer_1.get());
  EXPECT_TRUE(IsActive());
  RemovePlayers(player_observer_2.get());
  EXPECT_FALSE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ResumePlayGivesAudioFocus) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);

  StartNewPlayer(player_observer.get());
  ResolveAudioFocusSuccess();

  RemovePlayer(player_observer.get(), 0);
  EXPECT_FALSE(IsActive());

  EXPECT_TRUE(AddPlayer(player_observer.get(), 0));
  ResolveAudioFocusSuccess();
  EXPECT_TRUE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ResumeSuspendSeekAreSentOnlyOncePerPlayers) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);

  EXPECT_EQ(0, player_observer->received_suspend_calls());
  EXPECT_EQ(0, player_observer->received_resume_calls());

  StartNewPlayer(player_observer.get());
  StartNewPlayer(player_observer.get());
  StartNewPlayer(player_observer.get());

  EXPECT_EQ(0, player_observer->received_suspend_calls());
  EXPECT_EQ(0, player_observer->received_resume_calls());

  ResolveAudioFocusSuccess();

  EXPECT_EQ(0, player_observer->received_suspend_calls());
  EXPECT_EQ(0, player_observer->received_resume_calls());
  EXPECT_EQ(0, player_observer->received_seek_forward_calls());
  EXPECT_EQ(0, player_observer->received_seek_backward_calls());
  EXPECT_EQ(0, player_observer->received_seek_to_calls());

  SystemSuspend(true);
  EXPECT_EQ(3, player_observer->received_suspend_calls());

  SystemResume();
  EXPECT_EQ(3, player_observer->received_resume_calls());

  UISeekForward();
  EXPECT_EQ(3, player_observer->received_seek_forward_calls());

  UISeekBackward();
  EXPECT_EQ(3, player_observer->received_seek_backward_calls());

  UISeekTo();
  EXPECT_EQ(3, player_observer->received_seek_to_calls());

  UIScrubTo();
  EXPECT_EQ(6, player_observer->received_seek_to_calls());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ResumeSuspendSeekAreSentOnlyOncePerPlayersAddedTwice) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);

  EXPECT_EQ(0, player_observer->received_suspend_calls());
  EXPECT_EQ(0, player_observer->received_resume_calls());

  StartNewPlayer(player_observer.get());
  StartNewPlayer(player_observer.get());
  StartNewPlayer(player_observer.get());

  EXPECT_EQ(0, player_observer->received_suspend_calls());
  EXPECT_EQ(0, player_observer->received_resume_calls());

  ResolveAudioFocusSuccess();

  // Adding the three players above again.
  EXPECT_TRUE(AddPlayer(player_observer.get(), 0));
  EXPECT_TRUE(AddPlayer(player_observer.get(), 1));
  EXPECT_TRUE(AddPlayer(player_observer.get(), 2));

  EXPECT_EQ(0, player_observer->received_suspend_calls());
  EXPECT_EQ(0, player_observer->received_resume_calls());
  EXPECT_EQ(0, player_observer->received_seek_forward_calls());
  EXPECT_EQ(0, player_observer->received_seek_backward_calls());
  EXPECT_EQ(0, player_observer->received_seek_to_calls());

  SystemSuspend(true);
  EXPECT_EQ(3, player_observer->received_suspend_calls());

  SystemResume();
  EXPECT_EQ(3, player_observer->received_resume_calls());

  UISeekForward();
  EXPECT_EQ(3, player_observer->received_seek_forward_calls());

  UISeekBackward();
  EXPECT_EQ(3, player_observer->received_seek_backward_calls());

  UISeekTo();
  EXPECT_EQ(3, player_observer->received_seek_to_calls());

  UIScrubTo();
  EXPECT_EQ(6, player_observer->received_seek_to_calls());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       RemovingTheSamePlayerTwiceIsANoop) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);

  StartNewPlayer(player_observer.get());
  ResolveAudioFocusSuccess();

  RemovePlayer(player_observer.get(), 0);
  RemovePlayer(player_observer.get(), 0);
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest, AudioFocusType) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kTransient);

  // Starting a player with a given type should set the session to that type.
  StartNewPlayer(player_observer.get());
  ResolveAudioFocusSuccess();
  EXPECT_EQ(AudioFocusType::kGainTransientMayDuck, GetSessionAudioFocusType());

  // Adding a player of the same type should have no effect on the type.
  StartNewPlayer(player_observer.get());
  EXPECT_FALSE(HasUnresolvedAudioFocusRequest());
  EXPECT_EQ(AudioFocusType::kGainTransientMayDuck, GetSessionAudioFocusType());

  // Adding a player of Content type should override the current type.
  player_observer->SetMediaContentType(media::MediaContentType::kPersistent);
  StartNewPlayer(player_observer.get());
  ResolveAudioFocusSuccess();
  EXPECT_EQ(AudioFocusType::kGain, GetSessionAudioFocusType());

  // Adding a player of the Transient type should have no effect on the type.
  player_observer->SetMediaContentType(media::MediaContentType::kTransient);
  StartNewPlayer(player_observer.get());
  EXPECT_FALSE(HasUnresolvedAudioFocusRequest());
  EXPECT_EQ(AudioFocusType::kGain, GetSessionAudioFocusType());

  EXPECT_TRUE(player_observer->IsPlaying(0));
  EXPECT_TRUE(player_observer->IsPlaying(1));
  EXPECT_TRUE(player_observer->IsPlaying(2));
  EXPECT_TRUE(player_observer->IsPlaying(3));

  SystemSuspend(true);

  EXPECT_FALSE(player_observer->IsPlaying(0));
  EXPECT_FALSE(player_observer->IsPlaying(1));
  EXPECT_FALSE(player_observer->IsPlaying(2));
  EXPECT_FALSE(player_observer->IsPlaying(3));

  EXPECT_EQ(AudioFocusType::kGain, GetSessionAudioFocusType());

  SystemResume();

  EXPECT_TRUE(player_observer->IsPlaying(0));
  EXPECT_TRUE(player_observer->IsPlaying(1));
  EXPECT_TRUE(player_observer->IsPlaying(2));
  EXPECT_TRUE(player_observer->IsPlaying(3));

  EXPECT_EQ(AudioFocusType::kGain, GetSessionAudioFocusType());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ControlsShowForContent) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);

    // Starting a player with a persistent type should show the media controls.
    StartNewPlayer(player_observer.get());
    ResolveAudioFocusSuccess();

    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
    observer.WaitForControllable(true);
  }

  EXPECT_TRUE(IsControllable());
  EXPECT_TRUE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ControlsNoShowForTransient) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kTransient);

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);

    // Starting a player with a transient type should not show the media
    // controls.
    StartNewPlayer(player_observer.get());
    ResolveAudioFocusSuccess();

    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
    observer.WaitForControllable(false);
  }

  EXPECT_FALSE(IsControllable());
  EXPECT_TRUE(IsActive());
}

// This behaviour is specific to desktop.
#if !BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ControlsNoShowForTransientAndRoutedService) {
  EnsureMediaSessionService();
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      shell()->web_contents()->GetPrimaryMainFrame(),
      media::MediaContentType::kTransient);

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);

    // Starting a player with a transient type should show the media controls.
    StartNewPlayer(player_observer.get());
    ResolveAudioFocusSuccess();

    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
    observer.WaitForControllable(false);
  }

  EXPECT_FALSE(IsControllable());
  EXPECT_TRUE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ControlsNoShowForTransientAndPlaybackStateNone) {
  EnsureMediaSessionService();
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      shell()->web_contents()->GetPrimaryMainFrame(),
      media::MediaContentType::kTransient);

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);

    // Starting a player with a transient type should not show the media
    // controls.
    StartNewPlayer(player_observer.get());
    ResolveAudioFocusSuccess();

    SetPlaybackState(blink::mojom::MediaSessionPlaybackState::NONE);

    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
    observer.WaitForControllable(false);
  }

  EXPECT_FALSE(IsControllable());
  EXPECT_TRUE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ControlsShowForTransientAndPlaybackStatePaused) {
  EnsureMediaSessionService();
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      shell()->web_contents()->GetPrimaryMainFrame(),
      media::MediaContentType::kTransient);

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);

    // Starting a player with a transient type should show the media controls if
    // we have a playback state from the service.
    StartNewPlayer(player_observer.get());
    ResolveAudioFocusSuccess();

    SetPlaybackState(blink::mojom::MediaSessionPlaybackState::PAUSED);

    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
    observer.WaitForControllable(true);
  }

  EXPECT_TRUE(IsControllable());
  EXPECT_TRUE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ControlsShowForTransientAndPlaybackStatePlaying) {
  EnsureMediaSessionService();
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      shell()->web_contents()->GetPrimaryMainFrame(),
      media::MediaContentType::kTransient);

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);

    // Starting a player with a transient type should show the media controls if
    // we have a playback state from the service.
    StartNewPlayer(player_observer.get());
    ResolveAudioFocusSuccess();

    SetPlaybackState(blink::mojom::MediaSessionPlaybackState::PLAYING);

    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
    observer.WaitForControllable(true);
  }

  EXPECT_TRUE(IsControllable());
  EXPECT_TRUE(IsActive());
}

#endif  // !BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ControlsHideWhenStopped) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);

    StartNewPlayer(player_observer.get());
    ResolveAudioFocusSuccess();

    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
    observer.WaitForControllable(true);

    EXPECT_EQ(MediaPlaybackState::kPlaying,
              observer.session_info()->playback_state);
  }

  RemovePlayers(player_observer.get());

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);

    observer.WaitForState(MediaSessionInfo::SessionState::kInactive);
    observer.WaitForControllable(false);

    EXPECT_EQ(MediaPlaybackState::kPaused,
              observer.session_info()->playback_state);
  }

  EXPECT_FALSE(IsControllable());
  EXPECT_FALSE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ControlsShownAcceptTransient) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);

    StartNewPlayer(player_observer.get());
    ResolveAudioFocusSuccess();

    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
    observer.WaitForControllable(true);

    EXPECT_EQ(MediaPlaybackState::kPlaying,
              observer.session_info()->playback_state);
  }

  // Transient player join the session without affecting the controls.
  player_observer->SetMediaContentType(media::MediaContentType::kTransient);
  StartNewPlayer(player_observer.get());

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);

    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
    observer.WaitForControllable(true);

    EXPECT_EQ(MediaPlaybackState::kPlaying,
              observer.session_info()->playback_state);
  }

  EXPECT_TRUE(IsControllable());
  EXPECT_TRUE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ControlsShownAfterContentAdded) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kTransient);

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);

    StartNewPlayer(player_observer.get());
    ResolveAudioFocusSuccess();

    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
    observer.WaitForControllable(false);

    EXPECT_EQ(MediaPlaybackState::kPlaying,
              observer.session_info()->playback_state);
  }

  // The controls are shown when the content player is added.
  player_observer->SetMediaContentType(media::MediaContentType::kPersistent);
  StartNewPlayer(player_observer.get());
  ResolveAudioFocusSuccess();

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);

    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
    observer.WaitForControllable(true);

    EXPECT_EQ(MediaPlaybackState::kPlaying,
              observer.session_info()->playback_state);
  }

  EXPECT_TRUE(IsControllable());
  EXPECT_TRUE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ControlsStayIfOnlyOnePlayerHasBeenPaused) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);

    StartNewPlayer(player_observer.get());
    ResolveAudioFocusSuccess();

    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
    observer.WaitForControllable(true);

    EXPECT_EQ(MediaPlaybackState::kPlaying,
              observer.session_info()->playback_state);
  }

  player_observer->SetMediaContentType(media::MediaContentType::kTransient);
  StartNewPlayer(player_observer.get());

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);

    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
    observer.WaitForControllable(true);

    EXPECT_EQ(MediaPlaybackState::kPlaying,
              observer.session_info()->playback_state);
  }

  // Removing only content player doesn't hide the controls since the session
  // is still active.
  RemovePlayer(player_observer.get(), 0);

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);

    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
    observer.WaitForControllable(true);

    EXPECT_EQ(MediaPlaybackState::kPlaying,
              observer.session_info()->playback_state);
  }

  EXPECT_TRUE(IsControllable());
  EXPECT_TRUE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ControlsHideWhenTheLastPlayerIsRemoved) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);

    StartNewPlayer(player_observer.get());
    StartNewPlayer(player_observer.get());
    ResolveAudioFocusSuccess();

    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
    observer.WaitForControllable(true);
  }

  RemovePlayer(player_observer.get(), 0);

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);
    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
    observer.WaitForControllable(true);
  }

  EXPECT_TRUE(IsControllable());
  EXPECT_TRUE(IsActive());

  RemovePlayer(player_observer.get(), 1);

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);
    observer.WaitForState(MediaSessionInfo::SessionState::kInactive);
    observer.WaitForControllable(false);
  }

  EXPECT_FALSE(IsControllable());
  EXPECT_FALSE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ControlsHideWhenAllThePlayersAreRemoved) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);

    StartNewPlayer(player_observer.get());
    StartNewPlayer(player_observer.get());
    ResolveAudioFocusSuccess();

    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
    observer.WaitForControllable(true);
  }

  RemovePlayers(player_observer.get());

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);
    observer.WaitForState(MediaSessionInfo::SessionState::kInactive);
    observer.WaitForControllable(false);
  }

  EXPECT_FALSE(IsControllable());
  EXPECT_FALSE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ControlsNotHideWhenTheLastPlayerIsPaused) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);

    StartNewPlayer(player_observer.get());
    StartNewPlayer(player_observer.get());
    ResolveAudioFocusSuccess();

    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
    observer.WaitForControllable(true);

    EXPECT_EQ(MediaPlaybackState::kPlaying,
              observer.session_info()->playback_state);
  }

  OnPlayerPaused(player_observer.get(), 0);

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);
    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
    observer.WaitForControllable(true);

    EXPECT_EQ(MediaPlaybackState::kPlaying,
              observer.session_info()->playback_state);
  }

  EXPECT_TRUE(IsControllable());
  EXPECT_TRUE(IsActive());

  OnPlayerPaused(player_observer.get(), 1);

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);
    observer.WaitForState(MediaSessionInfo::SessionState::kSuspended);
    observer.WaitForControllable(true);

    EXPECT_EQ(MediaPlaybackState::kPaused,
              observer.session_info()->playback_state);
  }

  EXPECT_TRUE(IsControllable());
  EXPECT_FALSE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       SuspendTemporaryUpdatesControls) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);

    StartNewPlayer(player_observer.get());
    ResolveAudioFocusSuccess();

    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
    observer.WaitForControllable(true);

    EXPECT_EQ(MediaPlaybackState::kPlaying,
              observer.session_info()->playback_state);
  }

  SystemSuspend(true);

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);
    observer.WaitForState(MediaSessionInfo::SessionState::kSuspended);
    observer.WaitForControllable(true);

    EXPECT_EQ(MediaPlaybackState::kPaused,
              observer.session_info()->playback_state);
  }

  EXPECT_TRUE(IsControllable());
  EXPECT_FALSE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ControlsUpdatedWhenResumed) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);

    StartNewPlayer(player_observer.get());
    ResolveAudioFocusSuccess();

    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
    observer.WaitForControllable(true);

    EXPECT_EQ(MediaPlaybackState::kPlaying,
              observer.session_info()->playback_state);
  }

  SystemSuspend(true);
  SystemResume();

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);

    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
    observer.WaitForControllable(true);

    EXPECT_EQ(MediaPlaybackState::kPlaying,
              observer.session_info()->playback_state);
  }

  EXPECT_TRUE(IsControllable());
  EXPECT_TRUE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ControlsHideWhenSessionSuspendedPermanently) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);

    StartNewPlayer(player_observer.get());
    ResolveAudioFocusSuccess();

    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
    observer.WaitForControllable(true);

    EXPECT_EQ(MediaPlaybackState::kPlaying,
              observer.session_info()->playback_state);
  }

  SystemSuspend(false);

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);
    observer.WaitForState(MediaSessionInfo::SessionState::kInactive);
    observer.WaitForControllable(false);

    EXPECT_EQ(MediaPlaybackState::kPaused,
              observer.session_info()->playback_state);
  }

  EXPECT_FALSE(IsControllable());
  EXPECT_FALSE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ControlsHideWhenSessionStops) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);

    StartNewPlayer(player_observer.get());
    ResolveAudioFocusSuccess();

    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
    observer.WaitForControllable(true);

    EXPECT_EQ(MediaPlaybackState::kPlaying,
              observer.session_info()->playback_state);
  }

  media_session_->Stop(MediaSession::SuspendType::kUI);

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);

    observer.WaitForState(MediaSessionInfo::SessionState::kInactive);
    observer.WaitForControllable(false);

    EXPECT_EQ(MediaPlaybackState::kPaused,
              observer.session_info()->playback_state);
  }

  EXPECT_FALSE(IsControllable());
  EXPECT_FALSE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ControlsHideWhenSessionChangesFromContentToTransient) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);

    StartNewPlayer(player_observer.get());
    ResolveAudioFocusSuccess();

    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
    observer.WaitForControllable(true);

    EXPECT_EQ(MediaPlaybackState::kPlaying,
              observer.session_info()->playback_state);
  }

  SystemSuspend(true);

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);
    observer.WaitForState(MediaSessionInfo::SessionState::kSuspended);
    observer.WaitForControllable(true);

    EXPECT_EQ(MediaPlaybackState::kPaused,
              observer.session_info()->playback_state);
  }

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);

    // This should reset the session and change it to a transient, so
    // hide the controls.
    player_observer->SetMediaContentType(media::MediaContentType::kTransient);
    StartNewPlayer(player_observer.get());
    ResolveAudioFocusSuccess();

    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
    observer.WaitForControllable(false);

    EXPECT_EQ(MediaPlaybackState::kPlaying,
              observer.session_info()->playback_state);
  }

  EXPECT_FALSE(IsControllable());
  EXPECT_TRUE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ControlsUpdatedWhenNewPlayerResetsSession) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);

    StartNewPlayer(player_observer.get());
    ResolveAudioFocusSuccess();

    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
    observer.WaitForControllable(true);

    EXPECT_EQ(MediaPlaybackState::kPlaying,
              observer.session_info()->playback_state);
  }

  SystemSuspend(true);

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);
    observer.WaitForState(MediaSessionInfo::SessionState::kSuspended);
    observer.WaitForControllable(true);

    EXPECT_EQ(MediaPlaybackState::kPaused,
              observer.session_info()->playback_state);
  }

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);

    // This should reset the session and update the controls.
    StartNewPlayer(player_observer.get());
    ResolveAudioFocusSuccess();

    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
    observer.WaitForControllable(true);

    EXPECT_EQ(MediaPlaybackState::kPlaying,
              observer.session_info()->playback_state);
  }

  EXPECT_TRUE(IsControllable());
  EXPECT_TRUE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ControlsResumedWhenPlayerIsResumed) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);

    StartNewPlayer(player_observer.get());
    ResolveAudioFocusSuccess();

    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
    observer.WaitForControllable(true);

    EXPECT_EQ(MediaPlaybackState::kPlaying,
              observer.session_info()->playback_state);
  }

  // Temporarily suspend, which does not give up audio focus.
  media_session_->Suspend(MediaSession::SuspendType::kSystem);

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);
    observer.WaitForState(MediaSessionInfo::SessionState::kSuspended);
    observer.WaitForControllable(true);

    EXPECT_EQ(MediaPlaybackState::kPaused,
              observer.session_info()->playback_state);
    EXPECT_TRUE(media_session_->IsSuspended());
    EXPECT_FALSE(IsActive());
  }

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);

    // This should resume the session and update the controls.
    SystemResume();
    // The player was paused, and still has the audio focus anyway.  For either
    // reason, it should not request audio focus now.
    EXPECT_FALSE(HasUnresolvedAudioFocusRequest());

    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
    observer.WaitForControllable(true);

    EXPECT_EQ(MediaPlaybackState::kPlaying,
              observer.session_info()->playback_state);
    // Verify that it still has audio focus, even though it wasn't requested.
    EXPECT_TRUE(IsActive());
  }

  EXPECT_TRUE(IsControllable());
  EXPECT_TRUE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ControlsUpdatedDueToResumeSessionAction) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);

    StartNewPlayer(player_observer.get());
    ResolveAudioFocusSuccess();

    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
    observer.WaitForControllable(true);

    EXPECT_EQ(MediaPlaybackState::kPlaying,
              observer.session_info()->playback_state);
  }

  UISuspend();

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);
    observer.WaitForState(MediaSessionInfo::SessionState::kSuspended);
    observer.WaitForControllable(true);

    EXPECT_EQ(MediaPlaybackState::kPaused,
              observer.session_info()->playback_state);
  }

  EXPECT_TRUE(IsControllable());
  EXPECT_FALSE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ControlsUpdatedDueToSuspendSessionAction) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);

    StartNewPlayer(player_observer.get());
    ResolveAudioFocusSuccess();

    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
    observer.WaitForControllable(true);

    EXPECT_EQ(MediaPlaybackState::kPlaying,
              observer.session_info()->playback_state);
  }

  UISuspend();

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);
    observer.WaitForState(MediaSessionInfo::SessionState::kSuspended);
    observer.WaitForControllable(true);

    EXPECT_EQ(MediaPlaybackState::kPaused,
              observer.session_info()->playback_state);
  }

  UIResume();

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);
    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
    observer.WaitForControllable(true);

    EXPECT_EQ(MediaPlaybackState::kPlaying,
              observer.session_info()->playback_state);
  }

  EXPECT_TRUE(IsControllable());
  EXPECT_TRUE(IsActive());

  ResolveAudioFocusSuccess();

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);
    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
    observer.WaitForControllable(true);

    EXPECT_EQ(MediaPlaybackState::kPlaying,
              observer.session_info()->playback_state);
  }

  EXPECT_TRUE(IsControllable());
  EXPECT_TRUE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ControlsDontShowWhenOnlyOneShotIsPresent) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kOneShot);

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);

    StartNewPlayer(player_observer.get());
    ResolveAudioFocusSuccess();

    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
    observer.WaitForControllable(false);

    EXPECT_FALSE(IsControllable());
    EXPECT_TRUE(IsActive());
  }

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);

    player_observer->SetMediaContentType(media::MediaContentType::kTransient);
    StartNewPlayer(player_observer.get());

    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
    observer.WaitForControllable(true);

    EXPECT_TRUE(IsControllable());
    EXPECT_TRUE(IsActive());
  }

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);

    player_observer->SetMediaContentType(media::MediaContentType::kPersistent);
    StartNewPlayer(player_observer.get());

    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
    observer.WaitForControllable(true);

    EXPECT_TRUE(IsControllable());
    EXPECT_TRUE(IsActive());
  }
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ControlsHiddenAfterRemoveOneShotWithoutOtherPlayers) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kOneShot);

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);

    StartNewPlayer(player_observer.get());
    ResolveAudioFocusSuccess();

    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
    observer.WaitForControllable(false);
  }

  RemovePlayer(player_observer.get(), 0);

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);
    observer.WaitForState(MediaSessionInfo::SessionState::kInactive);
    observer.WaitForControllable(false);
  }

  EXPECT_FALSE(IsControllable());
  EXPECT_FALSE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ControlsShowAfterRemoveOneShotWithPersistentPresent) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kOneShot);

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);

    StartNewPlayer(player_observer.get());
    player_observer->SetMediaContentType(media::MediaContentType::kTransient);
    StartNewPlayer(player_observer.get());
    player_observer->SetMediaContentType(media::MediaContentType::kPersistent);
    StartNewPlayer(player_observer.get());
    ResolveAudioFocusSuccess();

    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
    observer.WaitForControllable(false);
  }

  RemovePlayer(player_observer.get(), 0);

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);
    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
    observer.WaitForControllable(true);
  }

  EXPECT_TRUE(IsControllable());
  EXPECT_TRUE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       DontSuspendWhenOnlyOneShotIsPresent) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kOneShot);

  StartNewPlayer(player_observer.get());
  ResolveAudioFocusSuccess();

  SystemSuspend(false);

  EXPECT_FALSE(IsControllable());
  EXPECT_TRUE(IsActive());

  EXPECT_EQ(0, player_observer->received_suspend_calls());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       SuspendWhenOneShotAndNormalArePresent) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kOneShot);

  StartNewPlayer(player_observer.get());
  player_observer->SetMediaContentType(media::MediaContentType::kTransient);
  StartNewPlayer(player_observer.get());
  player_observer->SetMediaContentType(media::MediaContentType::kPersistent);
  StartNewPlayer(player_observer.get());
  ResolveAudioFocusSuccess();

  SystemSuspend(false);

  EXPECT_FALSE(IsControllable());
  EXPECT_FALSE(IsActive());

  EXPECT_EQ(2, player_observer->received_suspend_calls());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       DontResumeBySystemUISuspendedSessions) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);

  StartNewPlayer(player_observer.get());
  ResolveAudioFocusSuccess();

  UISuspend();
  EXPECT_TRUE(IsControllable());
  EXPECT_FALSE(IsActive());

  SystemResume();
  EXPECT_TRUE(IsControllable());
  EXPECT_FALSE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       AllowUIResumeForSystemSuspend) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);

  StartNewPlayer(player_observer.get());
  ResolveAudioFocusSuccess();

  SystemSuspend(true);
  EXPECT_TRUE(IsControllable());
  EXPECT_FALSE(IsActive());

  UIResume();
  ResolveAudioFocusSuccess();

  EXPECT_TRUE(IsControllable());
  EXPECT_TRUE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest, ResumeSuspendFromUI) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);

  StartNewPlayer(player_observer.get());
  ResolveAudioFocusSuccess();

  UISuspend();
  EXPECT_TRUE(IsControllable());
  EXPECT_FALSE(IsActive());

  UIResume();
  EXPECT_TRUE(IsActive());

  ResolveAudioFocusSuccess();
  EXPECT_TRUE(IsControllable());
  EXPECT_TRUE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ResumeSuspendFromSystem) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);

  StartNewPlayer(player_observer.get());
  ResolveAudioFocusSuccess();

  SystemSuspend(true);
  EXPECT_TRUE(IsControllable());
  EXPECT_FALSE(IsActive());

  SystemResume();
  EXPECT_FALSE(HasUnresolvedAudioFocusRequest());
  EXPECT_TRUE(IsControllable());
  EXPECT_TRUE(IsActive());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       OneShotTakesGainFocus) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kOneShot);

  StartNewPlayer(player_observer.get());
  ResolveAudioFocusSuccess();

  player_observer->SetMediaContentType(media::MediaContentType::kTransient);
  StartNewPlayer(player_observer.get());
  EXPECT_FALSE(HasUnresolvedAudioFocusRequest());

  player_observer->SetMediaContentType(media::MediaContentType::kPersistent);
  StartNewPlayer(player_observer.get());
  EXPECT_FALSE(HasUnresolvedAudioFocusRequest());

  EXPECT_EQ(AudioFocusType::kGain,
            mock_audio_focus_delegate()->GetCurrentFocusType());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       RemovingOneShotDropsFocus) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kOneShot);

  EXPECT_CALL(*mock_audio_focus_delegate(), AbandonAudioFocus());
  StartNewPlayer(player_observer.get());
  ResolveAudioFocusSuccess();

  RemovePlayer(player_observer.get(), 0);
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       RemovingOneShotWhileStillHavingOtherPlayersKeepsFocus) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kOneShot);

  EXPECT_CALL(*mock_audio_focus_delegate(), AbandonAudioFocus())
      .Times(1);  // Called in TearDown
  StartNewPlayer(player_observer.get());
  ResolveAudioFocusSuccess();

  player_observer->SetMediaContentType(media::MediaContentType::kPersistent);
  StartNewPlayer(player_observer.get());
  EXPECT_FALSE(HasUnresolvedAudioFocusRequest());

  RemovePlayer(player_observer.get(), 0);
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ActualPlaybackStateWhilePlayerPaused) {
  EnsureMediaSessionService();
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      shell()->web_contents()->GetPrimaryMainFrame(),
      media::MediaContentType::kPersistent);

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);

    StartNewPlayer(player_observer.get());
    ResolveAudioFocusSuccess();

    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
    EXPECT_EQ(MediaPlaybackState::kPlaying,
              observer.session_info()->playback_state);
  }

  OnPlayerPaused(player_observer.get(), 0);

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);
    observer.WaitForState(MediaSessionInfo::SessionState::kSuspended);
    EXPECT_EQ(MediaPlaybackState::kPaused,
              observer.session_info()->playback_state);
  }

  SetPlaybackState(blink::mojom::MediaSessionPlaybackState::PLAYING);

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);
    observer.WaitForState(MediaSessionInfo::SessionState::kSuspended);
    EXPECT_EQ(MediaPlaybackState::kPlaying,
              observer.session_info()->playback_state);
  }

  SetPlaybackState(blink::mojom::MediaSessionPlaybackState::PAUSED);

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);
    observer.WaitForState(MediaSessionInfo::SessionState::kSuspended);
    EXPECT_EQ(MediaPlaybackState::kPaused,
              observer.session_info()->playback_state);
  }

  SetPlaybackState(blink::mojom::MediaSessionPlaybackState::NONE);

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);
    observer.WaitForState(MediaSessionInfo::SessionState::kSuspended);
    EXPECT_EQ(MediaPlaybackState::kPaused,
              observer.session_info()->playback_state);
  }
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ActualPlaybackStateWhilePlayerPlaying) {
  EnsureMediaSessionService();
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      shell()->web_contents()->GetPrimaryMainFrame(),
      media::MediaContentType::kPersistent);

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);

    StartNewPlayer(player_observer.get());
    ResolveAudioFocusSuccess();

    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
    EXPECT_EQ(MediaPlaybackState::kPlaying,
              observer.session_info()->playback_state);
  }

  SetPlaybackState(blink::mojom::MediaSessionPlaybackState::PLAYING);

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);
    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
    EXPECT_EQ(MediaPlaybackState::kPlaying,
              observer.session_info()->playback_state);
  }

  SetPlaybackState(blink::mojom::MediaSessionPlaybackState::PAUSED);

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);
    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
    EXPECT_EQ(MediaPlaybackState::kPlaying,
              observer.session_info()->playback_state);
  }

  SetPlaybackState(blink::mojom::MediaSessionPlaybackState::NONE);

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);
    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
    EXPECT_EQ(MediaPlaybackState::kPlaying,
              observer.session_info()->playback_state);
  }
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       ActualPlaybackStateWhilePlayerRemoved) {
  EnsureMediaSessionService();
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      shell()->web_contents()->GetPrimaryMainFrame(),
      media::MediaContentType::kPersistent);

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);

    StartNewPlayer(player_observer.get());
    ResolveAudioFocusSuccess();

    observer.WaitForState(MediaSessionInfo::SessionState::kActive);
    EXPECT_EQ(MediaPlaybackState::kPlaying,
              observer.session_info()->playback_state);
  }

  RemovePlayer(player_observer.get(), 0);

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);
    observer.WaitForState(MediaSessionInfo::SessionState::kInactive);
    EXPECT_EQ(MediaPlaybackState::kPaused,
              observer.session_info()->playback_state);
  }

  SetPlaybackState(blink::mojom::MediaSessionPlaybackState::PLAYING);

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);
    observer.WaitForState(MediaSessionInfo::SessionState::kInactive);
    EXPECT_EQ(MediaPlaybackState::kPlaying,
              observer.session_info()->playback_state);
  }

  SetPlaybackState(blink::mojom::MediaSessionPlaybackState::PAUSED);

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);
    observer.WaitForState(MediaSessionInfo::SessionState::kInactive);
    EXPECT_EQ(MediaPlaybackState::kPaused,
              observer.session_info()->playback_state);
  }

  SetPlaybackState(blink::mojom::MediaSessionPlaybackState::NONE);

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);
    observer.WaitForState(MediaSessionInfo::SessionState::kInactive);
    EXPECT_EQ(MediaPlaybackState::kPaused,
              observer.session_info()->playback_state);
  }
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       UMA_Suspended_SystemTransient) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);
  base::HistogramTester tester;

  StartNewPlayer(player_observer.get());
  ResolveAudioFocusSuccess();
  SystemSuspend(true);

  std::unique_ptr<base::HistogramSamples> samples(
      tester.GetHistogramSamplesSinceCreation("Media.Session.Suspended"));
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(0));  // System Transient
  EXPECT_EQ(0, samples->GetCount(1));  // System Permanent
  EXPECT_EQ(0, samples->GetCount(2));  // UI
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       UMA_Suspended_SystemPermantent) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);
  base::HistogramTester tester;

  StartNewPlayer(player_observer.get());
  ResolveAudioFocusSuccess();
  SystemSuspend(false);

  std::unique_ptr<base::HistogramSamples> samples(
      tester.GetHistogramSamplesSinceCreation("Media.Session.Suspended"));
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(0, samples->GetCount(0));  // System Transient
  EXPECT_EQ(1, samples->GetCount(1));  // System Permanent
  EXPECT_EQ(0, samples->GetCount(2));  // UI
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest, UMA_Suspended_UI) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);

  base::HistogramTester tester;

  StartNewPlayer(player_observer.get());
  ResolveAudioFocusSuccess();
  UISuspend();

  std::unique_ptr<base::HistogramSamples> samples(
      tester.GetHistogramSamplesSinceCreation("Media.Session.Suspended"));
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(0, samples->GetCount(0));  // System Transient
  EXPECT_EQ(0, samples->GetCount(1));  // System Permanent
  EXPECT_EQ(1, samples->GetCount(2));  // UI
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       UMA_Suspended_Multiple) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);
  base::HistogramTester tester;

  StartNewPlayer(player_observer.get());
  ResolveAudioFocusSuccess();

  UISuspend();
  UIResume();
  ResolveAudioFocusSuccess();

  SystemSuspend(true);
  SystemResume();

  UISuspend();
  UIResume();
  ResolveAudioFocusSuccess();

  SystemSuspend(false);

  std::unique_ptr<base::HistogramSamples> samples(
      tester.GetHistogramSamplesSinceCreation("Media.Session.Suspended"));
  EXPECT_EQ(4, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(0));  // System Transient
  EXPECT_EQ(1, samples->GetCount(1));  // System Permanent
  EXPECT_EQ(2, samples->GetCount(2));  // UI
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       UMA_Suspended_Crossing) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);
  base::HistogramTester tester;

  StartNewPlayer(player_observer.get());
  ResolveAudioFocusSuccess();

  UISuspend();
  SystemSuspend(true);
  SystemSuspend(false);
  UIResume();
  ResolveAudioFocusSuccess();

  SystemSuspend(true);
  SystemSuspend(true);
  SystemSuspend(false);
  SystemResume();

  std::unique_ptr<base::HistogramSamples> samples(
      tester.GetHistogramSamplesSinceCreation("Media.Session.Suspended"));
  EXPECT_EQ(2, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(0));  // System Transient
  EXPECT_EQ(0, samples->GetCount(1));  // System Permanent
  EXPECT_EQ(1, samples->GetCount(2));  // UI
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest, UMA_Suspended_Stop) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);
  base::HistogramTester tester;

  StartNewPlayer(player_observer.get());
  ResolveAudioFocusSuccess();
  media_session_->Stop(MediaSession::SuspendType::kUI);

  std::unique_ptr<base::HistogramSamples> samples(
      tester.GetHistogramSamplesSinceCreation("Media.Session.Suspended"));
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(0, samples->GetCount(0));  // System Transient
  EXPECT_EQ(0, samples->GetCount(1));  // System Permanent
  EXPECT_EQ(1, samples->GetCount(2));  // UI
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       UMA_ActiveTime_NoActivation) {
  base::HistogramTester tester;

  std::unique_ptr<MediaSessionImpl> media_session = CreateDummyMediaSession();
  media_session.reset();

  // A MediaSession that wasn't active doesn't register an active time.
  std::unique_ptr<base::HistogramSamples> samples(
      tester.GetHistogramSamplesSinceCreation("Media.Session.ActiveTime"));
  EXPECT_EQ(0, samples->TotalCount());
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       UMA_ActiveTime_SimpleActivation) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);
  base::HistogramTester tester;

  MediaSessionUmaHelper* media_session_uma_helper = GetMediaSessionUMAHelper();
  base::SimpleTestTickClock clock;
  clock.SetNowTicks(base::TimeTicks::Now());
  media_session_uma_helper->SetClockForTest(&clock);

  StartNewPlayer(player_observer.get());
  ResolveAudioFocusSuccess();

  clock.Advance(base::Milliseconds(1000));
  media_session_->Stop(MediaSession::SuspendType::kUI);

  std::unique_ptr<base::HistogramSamples> samples(
      tester.GetHistogramSamplesSinceCreation("Media.Session.ActiveTime"));
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(1000));
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       UMA_ActiveTime_ActivationWithUISuspension) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);
  base::HistogramTester tester;

  MediaSessionUmaHelper* media_session_uma_helper = GetMediaSessionUMAHelper();
  base::SimpleTestTickClock clock;
  clock.SetNowTicks(base::TimeTicks::Now());
  media_session_uma_helper->SetClockForTest(&clock);

  StartNewPlayer(player_observer.get());
  ResolveAudioFocusSuccess();

  clock.Advance(base::Milliseconds(1000));
  UISuspend();

  clock.Advance(base::Milliseconds(2000));
  UIResume();
  ResolveAudioFocusSuccess();

  clock.Advance(base::Milliseconds(1000));
  media_session_->Stop(MediaSession::SuspendType::kUI);

  std::unique_ptr<base::HistogramSamples> samples(
      tester.GetHistogramSamplesSinceCreation("Media.Session.ActiveTime"));
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(2000));
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       UMA_ActiveTime_ActivationWithSystemSuspension) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);
  base::HistogramTester tester;

  MediaSessionUmaHelper* media_session_uma_helper = GetMediaSessionUMAHelper();
  base::SimpleTestTickClock clock;
  clock.SetNowTicks(base::TimeTicks::Now());
  media_session_uma_helper->SetClockForTest(&clock);

  StartNewPlayer(player_observer.get());
  ResolveAudioFocusSuccess();

  clock.Advance(base::Milliseconds(1000));
  SystemSuspend(true);

  clock.Advance(base::Milliseconds(2000));
  SystemResume();

  clock.Advance(base::Milliseconds(1000));
  media_session_->Stop(MediaSession::SuspendType::kUI);

  std::unique_ptr<base::HistogramSamples> samples(
      tester.GetHistogramSamplesSinceCreation("Media.Session.ActiveTime"));
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(2000));
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       UMA_ActiveTime_ActivateSuspendedButNotStopped) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);
  base::HistogramTester tester;

  MediaSessionUmaHelper* media_session_uma_helper = GetMediaSessionUMAHelper();
  base::SimpleTestTickClock clock;
  clock.SetNowTicks(base::TimeTicks::Now());
  media_session_uma_helper->SetClockForTest(&clock);

  StartNewPlayer(player_observer.get());
  ResolveAudioFocusSuccess();
  clock.Advance(base::Milliseconds(500));
  SystemSuspend(true);

  {
    std::unique_ptr<base::HistogramSamples> samples(
        tester.GetHistogramSamplesSinceCreation("Media.Session.ActiveTime"));
    EXPECT_EQ(0, samples->TotalCount());
  }

  SystemResume();
  clock.Advance(base::Milliseconds(5000));
  UISuspend();

  {
    std::unique_ptr<base::HistogramSamples> samples(
        tester.GetHistogramSamplesSinceCreation("Media.Session.ActiveTime"));
    EXPECT_EQ(0, samples->TotalCount());
  }
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       UMA_ActiveTime_ActivateSuspendStopTwice) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);
  base::HistogramTester tester;

  MediaSessionUmaHelper* media_session_uma_helper = GetMediaSessionUMAHelper();
  base::SimpleTestTickClock clock;
  clock.SetNowTicks(base::TimeTicks::Now());
  media_session_uma_helper->SetClockForTest(&clock);

  StartNewPlayer(player_observer.get());
  ResolveAudioFocusSuccess();
  clock.Advance(base::Milliseconds(500));
  SystemSuspend(true);
  media_session_->Stop(MediaSession::SuspendType::kUI);

  StartNewPlayer(player_observer.get());
  ResolveAudioFocusSuccess();
  clock.Advance(base::Milliseconds(5000));
  SystemResume();
  media_session_->Stop(MediaSession::SuspendType::kUI);

  std::unique_ptr<base::HistogramSamples> samples(
      tester.GetHistogramSamplesSinceCreation("Media.Session.ActiveTime"));
  EXPECT_EQ(2, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(500));
  EXPECT_EQ(1, samples->GetCount(5000));
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       UMA_ActiveTime_MultipleActivations) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);
  base::HistogramTester tester;

  MediaSessionUmaHelper* media_session_uma_helper = GetMediaSessionUMAHelper();
  base::SimpleTestTickClock clock;
  clock.SetNowTicks(base::TimeTicks::Now());
  media_session_uma_helper->SetClockForTest(&clock);

  StartNewPlayer(player_observer.get());
  ResolveAudioFocusSuccess();
  clock.Advance(base::Milliseconds(10000));
  RemovePlayer(player_observer.get(), 0);

  StartNewPlayer(player_observer.get());
  ResolveAudioFocusSuccess();
  clock.Advance(base::Milliseconds(1000));
  media_session_->Stop(MediaSession::SuspendType::kUI);

  std::unique_ptr<base::HistogramSamples> samples(
      tester.GetHistogramSamplesSinceCreation("Media.Session.ActiveTime"));
  EXPECT_EQ(2, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(1000));
  EXPECT_EQ(1, samples->GetCount(10000));
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       AddingObserverNotifiesCurrentInformation_EmptyInfo) {
  media_session::test::MockMediaSessionMojoObserver observer(*media_session_);

  media_session::MediaMetadata expected_metadata;
  expected_metadata.title = shell()->web_contents()->GetTitle();
  expected_metadata.source_title = GetExpectedSourceTitle();
  observer.WaitForExpectedMetadata(expected_metadata);
}

IN_PROC_BROWSER_TEST_P(MediaSessionImplParamBrowserTest,
                       AddingMojoObserverNotifiesCurrentInformation_WithInfo) {
  // Set up the service and information.
  EnsureMediaSessionService();

  media_session::MediaMetadata expected_metadata;
  expected_metadata.title = u"title";
  expected_metadata.artist = u"artist";
  expected_metadata.album = u"album";
  expected_metadata.source_title = GetExpectedSourceTitle();

  blink::mojom::SpecMediaMetadataPtr spec_metadata(
      blink::mojom::SpecMediaMetadata::New());
  spec_metadata->title = u"title";
  spec_metadata->artist = u"artist";
  spec_metadata->album = u"album";
  mock_media_session_service_->SetMetadata(std::move(spec_metadata));

  // Make sure the service is routed,
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      shell()->web_contents()->GetPrimaryMainFrame(),
      media::MediaContentType::kPersistent);

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);
    StartNewPlayer(player_observer.get());
    ResolveAudioFocusSuccess();

    observer.WaitForExpectedMetadata(expected_metadata);
  }
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplSyncBrowserTest,
                       PepperPlayerNotAddedIfFocusFailed) {
  SetSyncAudioFocusResult(AudioFocusDelegate::AudioFocusResult::kFailed);

  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPepper);
  int player_id = player_observer->StartNewPlayer();

  EXPECT_FALSE(AddPlayer(player_observer.get(), player_id));

  EXPECT_FALSE(media_session_->HasPepper());
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest, Async_RequestFailure_Gain) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);

  StartNewPlayer(player_observer.get());
  player_observer->SetMediaContentType(media::MediaContentType::kTransient);
  StartNewPlayer(player_observer.get());

  EXPECT_TRUE(player_observer->IsPlaying(0));
  EXPECT_TRUE(player_observer->IsPlaying(1));
  EXPECT_TRUE(IsActive());

  // The gain request failed so we should suspend the whole session.
  ResolveAudioFocusFailure();
  EXPECT_FALSE(player_observer->IsPlaying(0));
  EXPECT_FALSE(player_observer->IsPlaying(1));
  EXPECT_FALSE(IsActive());

  ResolveAudioFocusSuccess();
  EXPECT_FALSE(player_observer->IsPlaying(0));
  EXPECT_FALSE(player_observer->IsPlaying(1));
  EXPECT_FALSE(IsActive());
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       Async_RequestFailure_GainTransient) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);

  StartNewPlayer(player_observer.get());
  player_observer->SetMediaContentType(media::MediaContentType::kTransient);
  StartNewPlayer(player_observer.get());

  EXPECT_TRUE(player_observer->IsPlaying(0));
  EXPECT_TRUE(player_observer->IsPlaying(1));
  EXPECT_TRUE(IsActive());

  ResolveAudioFocusSuccess();
  EXPECT_TRUE(player_observer->IsPlaying(0));
  EXPECT_TRUE(player_observer->IsPlaying(1));
  EXPECT_TRUE(IsActive());

  // A transient audio focus failure should only affect transient players.
  ResolveAudioFocusFailure();
  EXPECT_TRUE(player_observer->IsPlaying(0));
  EXPECT_FALSE(player_observer->IsPlaying(1));
  EXPECT_TRUE(IsActive());
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest, Async_GainThenTransient) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);

  StartNewPlayer(player_observer.get());
  player_observer->SetMediaContentType(media::MediaContentType::kTransient);
  StartNewPlayer(player_observer.get());

  EXPECT_TRUE(player_observer->IsPlaying(0));
  EXPECT_TRUE(player_observer->IsPlaying(1));

  ResolveAudioFocusSuccess();
  EXPECT_TRUE(player_observer->IsPlaying(0));
  EXPECT_TRUE(player_observer->IsPlaying(1));

  ResolveAudioFocusSuccess();
  EXPECT_TRUE(player_observer->IsPlaying(0));
  EXPECT_TRUE(player_observer->IsPlaying(1));
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest, Async_TransientThenGain) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kTransient);

  StartNewPlayer(player_observer.get());
  player_observer->SetMediaContentType(media::MediaContentType::kPersistent);
  StartNewPlayer(player_observer.get());

  EXPECT_TRUE(player_observer->IsPlaying(0));
  EXPECT_TRUE(player_observer->IsPlaying(1));

  ResolveAudioFocusSuccess();
  EXPECT_TRUE(player_observer->IsPlaying(0));
  EXPECT_TRUE(player_observer->IsPlaying(1));

  ResolveAudioFocusSuccess();
  EXPECT_TRUE(player_observer->IsPlaying(0));
  EXPECT_TRUE(player_observer->IsPlaying(1));
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       Async_SuspendBeforeResolve) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);

  StartNewPlayer(player_observer.get());
  EXPECT_TRUE(player_observer->IsPlaying(0));

  SystemSuspend(true);
  EXPECT_FALSE(player_observer->IsPlaying(0));
  EXPECT_FALSE(IsActive());

  ResolveAudioFocusSuccess();
  EXPECT_FALSE(player_observer->IsPlaying(0));
  EXPECT_FALSE(IsActive());

  SystemResume();
  EXPECT_TRUE(IsActive());
  EXPECT_TRUE(player_observer->IsPlaying(0));
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest, Async_ResumeBeforeResolve) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);

  StartNewPlayer(player_observer.get());
  EXPECT_TRUE(IsActive());
  EXPECT_TRUE(player_observer->IsPlaying(0));

  UISuspend();
  EXPECT_FALSE(IsActive());
  EXPECT_FALSE(player_observer->IsPlaying(0));

  UIResume();
  EXPECT_TRUE(IsActive());
  EXPECT_TRUE(player_observer->IsPlaying(0));

  ResolveAudioFocusSuccess();
  EXPECT_TRUE(IsActive());
  EXPECT_TRUE(player_observer->IsPlaying(0));

  ResolveAudioFocusFailure();
  EXPECT_FALSE(IsActive());
  EXPECT_FALSE(player_observer->IsPlaying(0));
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest, Async_RemoveBeforeResolve) {
  {
    auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
        media::MediaContentType::kPersistent);

    EXPECT_CALL(*mock_audio_focus_delegate(), AbandonAudioFocus());
    StartNewPlayer(player_observer.get());
    EXPECT_TRUE(player_observer->IsPlaying(0));

    RemovePlayer(player_observer.get(), 0);
  }

  ResolveAudioFocusSuccess();
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest, Async_StopBeforeResolve) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kTransient);

  StartNewPlayer(player_observer.get());
  ResolveAudioFocusSuccess();
  EXPECT_TRUE(player_observer->IsPlaying(0));

  player_observer->SetMediaContentType(media::MediaContentType::kPersistent);
  StartNewPlayer(player_observer.get());
  EXPECT_TRUE(player_observer->IsPlaying(1));

  media_session_->Stop(MediaSession::SuspendType::kUI);
  ResolveAudioFocusSuccess();

  EXPECT_FALSE(player_observer->IsPlaying(0));
  EXPECT_FALSE(player_observer->IsPlaying(1));
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest, Async_Unducking_Failure) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);

  StartNewPlayer(player_observer.get());
  EXPECT_TRUE(IsActive());
  EXPECT_TRUE(player_observer->IsPlaying(0));

  SystemStartDucking();
  EXPECT_TRUE(IsDucking());

  ResolveAudioFocusFailure();
  EXPECT_TRUE(IsDucking());
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest, Async_Unducking_Inactive) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);

  StartNewPlayer(player_observer.get());
  EXPECT_TRUE(IsActive());
  EXPECT_TRUE(player_observer->IsPlaying(0));

  media_session_->Stop(MediaSession::SuspendType::kUI);
  SystemStartDucking();
  EXPECT_TRUE(IsDucking());

  ResolveAudioFocusSuccess();
  EXPECT_TRUE(IsDucking());
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest, Async_Unducking_Success) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);

  StartNewPlayer(player_observer.get());
  EXPECT_TRUE(IsActive());
  EXPECT_TRUE(player_observer->IsPlaying(0));

  SystemStartDucking();
  EXPECT_TRUE(IsDucking());

  ResolveAudioFocusSuccess();
  EXPECT_FALSE(IsDucking());
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest, Async_Unducking_Suspended) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);

  StartNewPlayer(player_observer.get());
  EXPECT_TRUE(IsActive());
  EXPECT_TRUE(player_observer->IsPlaying(0));

  UISuspend();
  SystemStartDucking();
  EXPECT_TRUE(IsDucking());

  ResolveAudioFocusSuccess();
  EXPECT_TRUE(IsDucking());
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest, MetadataWhenFileUrlScheme) {
  base::FilePath path = GetTestFilePath(nullptr, "title1.html");
  GURL file_url = net::FilePathToFileURL(path);
  EXPECT_TRUE(NavigateToURL(shell(), file_url));

  media_session::test::MockMediaSessionMojoObserver observer(*media_session_);

  media_session::MediaMetadata expected_metadata;
  expected_metadata.title = shell()->web_contents()->GetTitle();
  expected_metadata.source_title = u"File on your device";
  observer.WaitForExpectedMetadata(expected_metadata);
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest, UpdateFaviconURL) {
  std::vector<gfx::Size> valid_sizes;
  valid_sizes.push_back(gfx::Size(100, 100));
  valid_sizes.push_back(gfx::Size(200, 200));

  std::vector<blink::mojom::FaviconURLPtr> favicons;
  favicons.push_back(blink::mojom::FaviconURL::New(
      GURL("https://www.example.org/favicon1.png"),
      blink::mojom::FaviconIconType::kInvalid, valid_sizes,
      /*is_default_icon=*/false));
  favicons.push_back(blink::mojom::FaviconURL::New(
      GURL(), blink::mojom::FaviconIconType::kFavicon, valid_sizes,
      /*is_default_icon=*/false));
  favicons.push_back(blink::mojom::FaviconURL::New(
      GURL("https://www.example.org/favicon2.png"),
      blink::mojom::FaviconIconType::kFavicon, std::vector<gfx::Size>(),
      /*is_default_icon=*/false));
  favicons.push_back(blink::mojom::FaviconURL::New(
      GURL("https://www.example.org/favicon3.png"),
      blink::mojom::FaviconIconType::kFavicon, valid_sizes,
      /*is_default_icon=*/false));
  favicons.push_back(blink::mojom::FaviconURL::New(
      GURL("https://www.example.org/favicon4.png"),
      blink::mojom::FaviconIconType::kTouchIcon, valid_sizes,
      /*is_default_icon=*/false));
  favicons.push_back(blink::mojom::FaviconURL::New(
      GURL("https://www.example.org/favicon5.png"),
      blink::mojom::FaviconIconType::kTouchPrecomposedIcon, valid_sizes,
      /*is_default_icon=*/false));
  favicons.push_back(blink::mojom::FaviconURL::New(
      GURL("https://www.example.org/favicon6.png"),
      blink::mojom::FaviconIconType::kTouchIcon, std::vector<gfx::Size>(),
      /*is_default_icon=*/false));

  media_session_->DidUpdateFaviconURL(
      shell()->web_contents()->GetPrimaryMainFrame(), favicons);

  {
    std::vector<media_session::MediaImage> expected_images;
    media_session::MediaImage test_image_1;
    test_image_1.src = GURL("https://www.example.org/favicon2.png");
    test_image_1.sizes.push_back(kDefaultFaviconSize);
    expected_images.push_back(test_image_1);

    media_session::MediaImage test_image_2;
    test_image_2.src = GURL("https://www.example.org/favicon3.png");
    test_image_2.sizes = valid_sizes;
    expected_images.push_back(test_image_2);

    media_session::MediaImage test_image_3;
    test_image_3.src = GURL("https://www.example.org/favicon4.png");
    test_image_3.sizes = valid_sizes;
    expected_images.push_back(test_image_3);

    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);
    observer.WaitForExpectedImagesOfType(
        media_session::mojom::MediaSessionImageType::kSourceIcon,
        expected_images);
  }

  {
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);
    media_session_->DidUpdateFaviconURL(
        shell()->web_contents()->GetPrimaryMainFrame(),
        std::vector<blink::mojom::FaviconURLPtr>());
    observer.WaitForExpectedImagesOfType(
        media_session::mojom::MediaSessionImageType::kSourceIcon,
        std::vector<media_session::MediaImage>());
  }
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       UpdateFaviconURL_ClearOnNavigate) {
  std::vector<blink::mojom::FaviconURLPtr> favicons;
  favicons.push_back(blink::mojom::FaviconURL::New(
      GURL("https://www.example.org/favicon1.png"),
      blink::mojom::FaviconIconType::kFavicon, std::vector<gfx::Size>(),
      /*is_default_icon=*/false));

  media_session_->DidUpdateFaviconURL(
      shell()->web_contents()->GetPrimaryMainFrame(), favicons);

  {
    std::vector<media_session::MediaImage> expected_images;
    media_session::MediaImage test_image_1;
    test_image_1.src = GURL("https://www.example.org/favicon1.png");
    test_image_1.sizes.push_back(kDefaultFaviconSize);
    expected_images.push_back(test_image_1);

    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);
    observer.WaitForExpectedImagesOfType(
        media_session::mojom::MediaSessionImageType::kSourceIcon,
        expected_images);
  }

  {
    std::vector<media_session::MediaImage> expected_images;
    media_session::MediaImage test_image_1;
    test_image_1.src =
        embedded_test_server()->GetURL("example.com", "/favicon.ico");
    test_image_1.sizes.push_back(kDefaultFaviconSize);
    expected_images.push_back(test_image_1);

    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);
    EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL(
                                           "example.com", "/title1.html")));

    observer.WaitForExpectedImagesOfType(
        media_session::mojom::MediaSessionImageType::kSourceIcon,
        expected_images);
  }
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       SinkIdChangeNotifiesObservers) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);

  StartNewPlayer(player_observer.get());

  UISetAudioSink(kExampleSinkId);
  EXPECT_EQ(player_observer->received_set_audio_sink_id_calls(), 1);
  EXPECT_EQ(player_observer->GetAudioOutputSinkId(0), kExampleSinkId);
}

class MediaSessionFaviconBrowserTest : public ContentBrowserTest {
 protected:
  MediaSessionFaviconBrowserTest() = default;

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");
  }
};

// Helper class that waits to receive a favicon from the renderer process.
class FaviconWaiter : public WebContentsObserver {
 public:
  explicit FaviconWaiter(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  void DidUpdateFaviconURL(
      RenderFrameHost* render_frame_host,
      const std::vector<blink::mojom::FaviconURLPtr>& candidates) override {
    received_favicon_ = true;
    run_loop_.Quit();
  }

  void Wait() {
    if (received_favicon_) {
      return;
    }
    run_loop_.Run();
  }

 private:
  bool received_favicon_ = false;
  base::RunLoop run_loop_;
};

IN_PROC_BROWSER_TEST_F(MediaSessionFaviconBrowserTest, StartupInitalization) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("example.com", "/title1.html")));

  std::unique_ptr<FaviconWaiter> favicon_waiter(
      new FaviconWaiter(shell()->web_contents()));

  // Insert the favicon dynamically.
  ASSERT_TRUE(
      ExecJs(shell()->web_contents(),
             "let l = document.createElement('link'); "
             "l.rel='icon'; l.type='image/png'; l.href='single_face.jpg'; "
             "document.head.appendChild(l)"));

  // Wait until it's received by the browser process.
  favicon_waiter->Wait();

  // The MediaSession should be created with the favicon already available.
  MediaSession* media_session = MediaSessionImpl::Get(shell()->web_contents());

  media_session::MediaImage icon;
  icon.src = embedded_test_server()->GetURL("example.com", "/single_face.jpg");
  icon.sizes.push_back({16, 16});

  media_session::test::MockMediaSessionMojoObserver observer(*media_session);
  observer.WaitForExpectedImagesOfType(
      media_session::mojom::MediaSessionImageType::kSourceIcon, {icon});
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       PositionStateRouteWithTwoPlayers) {
  media_session::MediaPosition expected_position(
      /*playback_rate=*/0.0, /*duration=*/base::Seconds(10),
      /*position=*/base::TimeDelta(), /*end_of_media=*/false);

  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);
  int player_id = player_observer->StartNewPlayer();
  SetPosition(player_observer.get(), player_id, expected_position);

  {
    // With one normal player we should use the position that one provides.
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);
    AddPlayer(player_observer.get(), player_id);
    observer.WaitForExpectedPosition(expected_position);
  }

  int player_id_2 = player_observer->StartNewPlayer();

  {
    // If we add another player then we should become empty again.
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);
    AddPlayer(player_observer.get(), player_id_2);
    observer.WaitForEmptyPosition();
  }

  {
    // If we remove the player then we should use the first player position.
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);
    RemovePlayer(player_observer.get(), player_id_2);
    observer.WaitForExpectedPosition(expected_position);
  }
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       PositionStateWithOneShotPlayer) {
  media_session::MediaPosition expected_position(
      /*playback_rate=*/0.0, /*duration=*/base::Seconds(10),
      /*position=*/base::TimeDelta(), /*end_of_media=*/false);

  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kOneShot);
  int player_id = player_observer->StartNewPlayer();
  SetPosition(player_observer.get(), player_id, expected_position);
  AddPlayer(player_observer.get(), player_id);

  // OneShot players should be ignored for position data.
  media_session::test::MockMediaSessionMojoObserver observer(*media_session_);
  observer.WaitForEmptyPosition();
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       PositionStateWithPepperPlayer) {
  media_session::MediaPosition expected_position(
      /*playback_rate=*/0.0, /*duration=*/base::Seconds(10),
      /*position=*/base::TimeDelta(), /*end_of_media=*/false);

  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPepper);
  int player_id = player_observer->StartNewPlayer();
  SetPosition(player_observer.get(), player_id, expected_position);
  AddPlayer(player_observer.get(), player_id);

  // Pepper players should be ignored for position data.
  media_session::test::MockMediaSessionMojoObserver observer(*media_session_);
  observer.WaitForEmptyPosition();
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       PositionStateRouteWithTwoPlayers_OneShot) {
  media_session::MediaPosition expected_position(
      /*playback_rate=*/0.0, /*duration=*/base::Seconds(10),
      /*position=*/base::TimeDelta(), /*end_of_media=*/false);

  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);
  int player_id = player_observer->StartNewPlayer();
  SetPosition(player_observer.get(), player_id, expected_position);

  {
    // With one normal player we should use the position that one provides.
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);
    AddPlayer(player_observer.get(), player_id);
    observer.WaitForExpectedPosition(expected_position);
  }

  {
    // If we add an OneShot player then we should become empty again.
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);
    player_observer->SetMediaContentType(media::MediaContentType::kOneShot);
    StartNewPlayer(player_observer.get());
    observer.WaitForEmptyPosition();
  }
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       PositionStateRouteWithTwoPlayers_Pepper) {
  media_session::MediaPosition expected_position(
      /*playback_rate=*/0.0, /*duration=*/base::Seconds(10),
      /*position=*/base::TimeDelta(), /*end_of_media=*/false);

  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);
  int player_id = player_observer->StartNewPlayer();
  SetPosition(player_observer.get(), player_id, expected_position);

  {
    // With one normal player we should use the position that one provides.
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);
    AddPlayer(player_observer.get(), player_id);
    observer.WaitForExpectedPosition(expected_position);
  }

  {
    // If we add a Papper player then we should become empty again.
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);
    player_observer->SetMediaContentType(media::MediaContentType::kPepper);
    StartNewPlayer(player_observer.get());
    observer.WaitForEmptyPosition();
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_ANDROID)
// TODO(crbug.com/40097218): Re-enable this test.
#define MAYBE_PositionStateRouteWithOnePlayer \
  DISABLED_PositionStateRouteWithOnePlayer
#else
#define MAYBE_PositionStateRouteWithOnePlayer PositionStateRouteWithOnePlayer
#endif
IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       MAYBE_PositionStateRouteWithOnePlayer) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("example.com",
                                              "/media/session/position.html")));

  auto* main_frame = shell()->web_contents()->GetPrimaryMainFrame();
  const base::TimeDelta duration = base::Milliseconds(6060);

  {
    // By default we should have an empty position.
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);
    observer.WaitForEmptyPosition();
  }

  {
    // With one normal player we should use the position that one provides.
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);

    ASSERT_TRUE(ExecJs(main_frame, "document.getElementById('video').play()"));

    observer.WaitForExpectedPosition(media_session::MediaPosition(
        /*playback_rate=*/1.0, duration,
        /*position=*/base::TimeDelta(), /*end_of_media=*/false));
  }

  {
    // If we seek the player then the position should be updated.
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);

    ASSERT_TRUE(
        ExecJs(main_frame, "document.getElementById('video').currentTime = 1"));

    // We might only learn about the rate going back to 1.0 when the media time
    // has already progressed a bit.
    observer.WaitForExpectedPositionAtLeast(media_session::MediaPosition(
        /*playback_rate=*/1.0, duration,
        /*position=*/base::Seconds(1), /*end_of_media=*/false));
  }

  base::TimeDelta paused_position;
  {
    // If we pause the player then the rate should be updated.
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);

    ASSERT_TRUE(ExecJs(main_frame, "document.getElementById('video').pause()"));

    // Media time may have progressed since the time we seeked to 1s.
    paused_position =
        observer.WaitForExpectedPositionAtLeast(media_session::MediaPosition(
            /*playback_rate=*/0.0, duration,
            /*position=*/base::Seconds(1),
            /*end_of_media=*/false));
  }

  base::TimeDelta resumed_position;
  {
    // If we resume the player then the rate should be updated.
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);

    ASSERT_TRUE(ExecJs(main_frame, "document.getElementById('video').play()"));

    // We might only learn about the rate going back to 1.0 when the media time
    // has already progressed a bit.
    resumed_position =
        observer.WaitForExpectedPositionAtLeast(media_session::MediaPosition(
            /*playback_rate=*/1.0, duration, paused_position,
            /*end_of_media=*/false));
  }

  {
    // If we change the playback rate then the MediaPosition should be updated.
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);

    ASSERT_TRUE(ExecJs(main_frame,
                       "document.getElementById('video').playbackRate = 2"));

    // Media time may have progressed since the time we resumed playback.
    observer.WaitForExpectedPositionAtLeast(media_session::MediaPosition(
        /*playback_rate=*/2.0, duration, resumed_position,
        /*end_of_media=*/false));
  }

  {
    // If we remove the player then we should become empty again.
    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);

    ASSERT_TRUE(
        ExecJs(main_frame, "document.getElementById('video').src = ''"));

    observer.WaitForEmptyPosition();
  }
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       AudioDeviceSettingPersists) {
  // When an audio output device has been set: in addition to players switching
  // to that audio device, players created later on the same origin in the same
  // session should also use that device.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title1.html")));

  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);
  int player_1 = player_observer->StartNewPlayer();
  AddPlayer(player_observer.get(), player_1);
  UISetAudioSink("speaker1");
  EXPECT_EQ(player_observer->GetAudioOutputSinkId(player_1), "speaker1");

  // When a second player has been added on the same page, it should use the
  // audio device previously set.
  int player_2 = player_observer->StartNewPlayer();
  AddPlayer(player_observer.get(), player_2);
  EXPECT_EQ(player_observer->GetAudioOutputSinkId(player_2), "speaker1");

  // Clear the players and navigate to a new page on the same origin.
  RemovePlayers(player_observer.get());
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title2.html")));
  player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);

  // After navigating to another page on the same origin, newly created players
  // should use the previously set device.
  player_1 = player_observer->StartNewPlayer();
  AddPlayer(player_observer.get(), player_1);
  EXPECT_EQ(player_observer->GetAudioOutputSinkId(player_1), "speaker1");

  // Clear the players and navigate to a new page on a different origin
  RemovePlayers(player_observer.get());
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title1.html")));
  player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);

  // After navigating to another page on a different origin, newly created
  // players should not use the previously set device.
  player_1 = player_observer->StartNewPlayer();
  AddPlayer(player_observer.get(), player_1);
  EXPECT_NE(player_observer->GetAudioOutputSinkId(player_1), "speaker1");
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest, CacheFaviconImages) {
  std::vector<gfx::Size> valid_sizes;
  valid_sizes.push_back(gfx::Size(100, 100));
  valid_sizes.push_back(gfx::Size(200, 200));

  std::vector<blink::mojom::FaviconURLPtr> favicons;
  favicons.push_back(
      blink::mojom::FaviconURL::New(favicon_server().GetURL("/favicon.ico"),
                                    blink::mojom::FaviconIconType::kFavicon,
                                    valid_sizes, /*is_default_icon=*/false));

  media_session_->DidUpdateFaviconURL(
      shell()->web_contents()->GetPrimaryMainFrame(), favicons);

  media_session::MediaImage test_image;
  test_image.src = favicon_server().GetURL("/favicon.ico");
  test_image.sizes = valid_sizes;

  {
    EXPECT_EQ(0, get_favicon_calls());

    base::RunLoop run_loop;
    media_session_->GetMediaImageBitmap(
        test_image, 100, 100,
        base::BindLambdaForTesting([&](const SkBitmap&) { run_loop.Quit(); }));
    run_loop.Run();

    EXPECT_EQ(2, get_favicon_calls());
  }

  {
    base::RunLoop run_loop;
    media_session_->GetMediaImageBitmap(
        test_image, 100, 100,
        base::BindLambdaForTesting([&](const SkBitmap&) { run_loop.Quit(); }));
    run_loop.Run();

    EXPECT_EQ(3, get_favicon_calls());
  }
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       GetVisibilityNotifiesObservers) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);
  EXPECT_EQ(0, player_observer->received_request_visibility_calls());

  StartNewPlayer(player_observer.get());
  UIGetVisibility(base::DoNothing());
  EXPECT_EQ(1, player_observer->received_request_visibility_calls());
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       GetVisibilityMultiplePlayersMeetsVisibility) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);
  EXPECT_EQ(0, player_observer->received_request_visibility_calls());

  StartNewPlayer(player_observer.get());
  StartNewPlayer(player_observer.get());
  StartNewPlayer(player_observer.get());

  EXPECT_EQ(0, player_observer->received_request_visibility_calls());

  // Set expectations, create waiter and start waiting.
  player_observer->SetHasSufficientlyVisibleVideo(1, true);
  GetVisibilityWaiter waiter;
  UIGetVisibility(waiter.VisibilityCallback());
  waiter.WaitUntilDone();

  // Verify that the player observer received the corresponding request
  // visibility calls.
  EXPECT_EQ(3, player_observer->received_request_visibility_calls());

  // Verify that the waiter meets visibility.
  EXPECT_TRUE(waiter.MeetsVisibility());
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       GetVisibilityMultiplePlayersDoesNotMeetVisibility) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);
  EXPECT_EQ(0, player_observer->received_request_visibility_calls());

  StartNewPlayer(player_observer.get());
  StartNewPlayer(player_observer.get());
  StartNewPlayer(player_observer.get());

  EXPECT_EQ(0, player_observer->received_request_visibility_calls());

  // Create waiter and start waiting. Since we do not set any visibility
  // expectations, default is false.
  GetVisibilityWaiter waiter;
  UIGetVisibility(waiter.VisibilityCallback());
  waiter.WaitUntilDone();

  // Verify that the player observer received the corresponding request
  // visibility calls.
  EXPECT_EQ(3, player_observer->received_request_visibility_calls());

  // Verify that the waiter does not meet visibility.
  EXPECT_FALSE(waiter.MeetsVisibility());
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       GetVisibilityMultiplePlayersEarlyCancelDoesNotCrash) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);
  EXPECT_EQ(0, player_observer->received_request_visibility_calls());

  StartNewPlayer(player_observer.get());
  StartNewPlayer(player_observer.get());
  StartNewPlayer(player_observer.get());

  EXPECT_EQ(0, player_observer->received_request_visibility_calls());

  // Set expectations and create waiter.
  player_observer->SetHasSufficientlyVisibleVideo(1, true);
  GetVisibilityWaiter waiter;

  // Simulate the creation of two callbacks and start waiting.
  auto callback_to_drop = waiter.VisibilityCallback();
  UIGetVisibility(waiter.VisibilityCallback());
  waiter.WaitUntilDone();

  // Running this callback should be a no-op.
  std::move(callback_to_drop).Run(false);

  // Verify that the player observer received the corresponding request
  // visibility calls.
  EXPECT_EQ(3, player_observer->received_request_visibility_calls());

  // Verify that the waiter meets visibility, despite running `callback_to_drop`
  // with false.
  EXPECT_TRUE(waiter.MeetsVisibility());
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest, GetVisibilityNoPlayers) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);
  EXPECT_EQ(0, player_observer->received_request_visibility_calls());

  // Create waiter and start waiting.
  GetVisibilityWaiter waiter;
  UIGetVisibility(waiter.VisibilityCallback());
  waiter.WaitUntilDone();

  // Verify the player observer did not receive any request visibility calls.
  EXPECT_EQ(0, player_observer->received_request_visibility_calls());

  // Verify that the waiter meets visibility, despite running `callback_to_drop`
  // with false.
  EXPECT_FALSE(waiter.MeetsVisibility());
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplBrowserTest,
                       GetVisibilityPlayersPausedDoesNotMeetVisibility) {
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);
  EXPECT_EQ(0, player_observer->received_request_visibility_calls());

  StartNewPlayer(player_observer.get());
  StartNewPlayer(player_observer.get());
  StartNewPlayer(player_observer.get());

  EXPECT_EQ(0, player_observer->received_request_visibility_calls());

  // Set one of the players to have a sufficiently visible video, and suspend
  // the MediaSession.
  player_observer->SetHasSufficientlyVisibleVideo(1, true);
  SystemSuspend(true);

  // Verify that all players were paused.
  EXPECT_FALSE(player_observer->IsPlaying(0));
  EXPECT_FALSE(player_observer->IsPlaying(1));
  EXPECT_FALSE(player_observer->IsPlaying(2));

  // Create waiter and start waiting.
  GetVisibilityWaiter waiter;
  UIGetVisibility(waiter.VisibilityCallback());
  waiter.WaitUntilDone();

  // Verify the player observer did not receive any request visibility calls,
  // since all players are paused.
  EXPECT_EQ(0, player_observer->received_request_visibility_calls());

  // Verify that the waiter does not meet visibility.
  EXPECT_FALSE(waiter.MeetsVisibility());
}

class MediaSessionImplPrerenderingBrowserTest
    : public MediaSessionImplBrowserTest {
 public:
  MediaSessionImplPrerenderingBrowserTest(
      const MediaSessionImplPrerenderingBrowserTest&) = delete;
  MediaSessionImplPrerenderingBrowserTest(
      MediaSessionImplPrerenderingBrowserTest&&) = delete;

 protected:
  MediaSessionImplPrerenderingBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &MediaSessionImplPrerenderingBrowserTest::web_contents,
            base::Unretained(this))) {}

  WebContents* web_contents() { return shell()->web_contents(); }

 protected:
  test::PrerenderTestHelper prerender_helper_;
};

// Tests the audio device persists a prerender activation on same origin and
// that it is reset after a navigation cross-origin.
IN_PROC_BROWSER_TEST_F(MediaSessionImplPrerenderingBrowserTest,
                       AudioDeviceSettingPersists) {
  // When an audio output device has been set: in addition to players switching
  // to that audio device, players created later on the same origin in the same
  // session should also use that device.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title1.html")));

  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);
  int player_1 = player_observer->StartNewPlayer();
  AddPlayer(player_observer.get(), player_1);
  UISetAudioSink("speaker1");
  EXPECT_EQ(player_observer->GetAudioOutputSinkId(player_1), "speaker1");

  // When a second player has been added on the same page, it should use the
  // audio device previously set.
  int player_2 = player_observer->StartNewPlayer();
  AddPlayer(player_observer.get(), player_2);
  EXPECT_EQ(player_observer->GetAudioOutputSinkId(player_2), "speaker1");

  // Clear the players.
  RemovePlayers(player_observer.get());

  // Prerender the next page.
  auto prerender_url_a =
      embedded_test_server()->GetURL("a.com", "/title2.html");
  auto prerender_host_a = prerender_helper_.AddPrerender(prerender_url_a);
  content::test::PrerenderHostObserver host_observer_a(*web_contents(),
                                                       prerender_host_a);
  // Navigate to a new page on the same origin.
  test::PrerenderTestHelper::NavigatePrimaryPage(*web_contents(),
                                                 prerender_url_a);
  EXPECT_TRUE(host_observer_a.was_activated());
  player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);

  // After navigating to another page on the same origin, newly created players
  // should use the previously set device.
  player_1 = player_observer->StartNewPlayer();
  AddPlayer(player_observer.get(), player_1);
  EXPECT_EQ(player_observer->GetAudioOutputSinkId(player_1), "speaker1");

  // Clear the players and navigate to a new page on a different origin
  RemovePlayers(player_observer.get());
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title1.html")));
  player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      media::MediaContentType::kPersistent);

  // After navigating to another page on a different origin, newly created
  // players should not use the previously set device.
  player_1 = player_observer->StartNewPlayer();
  AddPlayer(player_observer.get(), player_1);
  EXPECT_NE(player_observer->GetAudioOutputSinkId(player_1), "speaker1");
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplPrerenderingBrowserTest,
                       DontClearFaviconCacheOnPrerenderNavigation) {
  {
    std::vector<media_session::MediaImage> expected_images;
    media_session::MediaImage test_image;
    test_image.src =
        embedded_test_server()->GetURL("example.com", "/favicon.ico");
    test_image.sizes.emplace_back(kDefaultFaviconSize);
    expected_images.emplace_back(test_image);

    media_session::test::MockMediaSessionMojoObserver observer(*media_session_);

    observer.WaitForExpectedImagesOfType(
        media_session::mojom::MediaSessionImageType::kSourceIcon,
        expected_images);
  }

  std::vector<gfx::Size> valid_sizes;
  valid_sizes.emplace_back(gfx::Size(100, 100));
  valid_sizes.emplace_back(gfx::Size(200, 200));

  GURL test_image_src = favicon_server().GetURL("/favicon.ico");
  EXPECT_FALSE(media_session_->HasImageCacheForTest(test_image_src));

  std::vector<blink::mojom::FaviconURLPtr> favicons;
  favicons.emplace_back(blink::mojom::FaviconURL::New(
      test_image_src, blink::mojom::FaviconIconType::kFavicon, valid_sizes,
      /*is_default_icon=*/false));

  media_session_->DidUpdateFaviconURL(
      shell()->web_contents()->GetPrimaryMainFrame(), favicons);
  media_session::MediaImage test_image;
  test_image.src = test_image_src;
  test_image.sizes = valid_sizes;

  {
    EXPECT_EQ(0, get_favicon_calls());

    base::RunLoop run_loop;
    media_session_->GetMediaImageBitmap(
        test_image, 100, 100,
        base::BindLambdaForTesting([&](const SkBitmap&) { run_loop.Quit(); }));
    run_loop.Run();

    EXPECT_EQ(2, get_favicon_calls());
  }

  EXPECT_TRUE(media_session_->HasImageCacheForTest(test_image_src));

  // Prerender the next page.
  auto prerender_url =
      embedded_test_server()->GetURL("example.com", "/title2.html");
  FrameTreeNodeId host_id = prerender_helper_.AddPrerender(prerender_url);
  content::RenderFrameHost* prerender_rfh =
      prerender_helper_.GetPrerenderedMainFrameHost(host_id);
  EXPECT_NE(prerender_rfh, nullptr);

  {
    base::RunLoop run_loop;
    media_session_->GetMediaImageBitmap(
        test_image, 100, 100,
        base::BindLambdaForTesting([&](const SkBitmap&) { run_loop.Quit(); }));
    run_loop.Run();

    EXPECT_EQ(3, get_favicon_calls());
  }

  EXPECT_TRUE(media_session_->HasImageCacheForTest(test_image_src));
}

class MediaSessionImplWithBackForwardCacheBrowserTest
    : public MediaSessionImplBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    MediaSessionImplBrowserTest::SetUpCommandLine(command_line);

    feature_list_.InitWithFeaturesAndParameters(
        GetBasicBackForwardCacheFeatureForTesting(
#if BUILDFLAG(IS_ANDROID)
            {{ features::kBackForwardCache,
               {
                 { "process_binding_strength",
                   "NORMAL" }
               } }}
#endif
            ),
        GetDefaultDisabledBackForwardCacheFeaturesForTesting());
  }

  RenderFrameHost* GetPrimaryMainFrame() {
    return shell()->web_contents()->GetPrimaryMainFrame();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(MediaSessionImplWithBackForwardCacheBrowserTest,
                       PlayAndCache) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.test", "/title1.html")));

  // Add a player.
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      GetPrimaryMainFrame(), media::MediaContentType::kPersistent);
  StartNewPlayer(player_observer.get());
  ResolveAudioFocusSuccess();
  EXPECT_TRUE(player_observer->IsPlaying(0));
  RenderFrameHostImplWrapper frame_host(GetPrimaryMainFrame());

  // Navigate to another page. The page is cached in back-forward cache.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.test", "/title1.html")));
  EXPECT_TRUE(frame_host->IsInBackForwardCache());

  // Restore the page from the back-forward cache.
  shell()->web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_FALSE(frame_host->IsInBackForwardCache());

  // After the page is restored from back-forward cache, the player observer
  // must be paused.
  EXPECT_FALSE(player_observer->IsPlaying(0));
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplWithBackForwardCacheBrowserTest,
                       PauseAndCache) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.test", "/title1.html")));

  // Add a player and pause this.
  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      GetPrimaryMainFrame(), media::MediaContentType::kPersistent);
  StartNewPlayer(player_observer.get());
  ResolveAudioFocusSuccess();
  UISuspend();
  EXPECT_FALSE(player_observer->IsPlaying(0));
  RenderFrameHostImplWrapper frame_host(GetPrimaryMainFrame());

  // Navigate to another page. The page is cached in back-forward cache.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.test", "/title1.html")));
  EXPECT_TRUE(frame_host->IsInBackForwardCache());

  // Restore the page from the back-forward cache.
  shell()->web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_FALSE(frame_host->IsInBackForwardCache());

  // After the page is restored from back-forward cache, the player observer
  // must be still paused.
  EXPECT_FALSE(player_observer->IsPlaying(0));
}

IN_PROC_BROWSER_TEST_F(MediaSessionImplWithBackForwardCacheBrowserTest,
                       CacheClearDoesntAffectCurrentPage) {
  // The bug this test is protecting against (https://crbug.com/1288620) only
  // reproduces on pages with an iframe, so load one.
  EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL(
                                         "a.test", "/iframe_clipped.html")));

  auto player_observer = std::make_unique<MockMediaSessionPlayerObserver>(
      GetPrimaryMainFrame(), media::MediaContentType::kPersistent);

  // Add a player.
  StartNewPlayer(player_observer.get());
  ResolveAudioFocusSuccess();
  EXPECT_TRUE(player_observer->IsPlaying(0));
  RenderFrameHostImplWrapper frame_host(GetPrimaryMainFrame());

  // Navigate to another page. The original page is cached in back-forward
  // cache.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.test", "/title1.html")));
  EXPECT_TRUE(frame_host->IsInBackForwardCache());

  // Add a player on the new page
  StartNewPlayer(player_observer.get());
  ResolveAudioFocusSuccess();
  EXPECT_TRUE(player_observer->IsPlaying(1));

  // Evict the page from the back-forward cache.
  shell()->web_contents()->GetController().GetBackForwardCache().Flush();
  EXPECT_TRUE(frame_host.WaitUntilRenderFrameDeleted());

  // The page being removed from the back-forward cache should not affect the
  // play state of the current page.
  EXPECT_TRUE(player_observer->IsPlaying(1));
}

}  // namespace content
