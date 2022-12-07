// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "content/browser/media/session/media_session_impl.h"

#include <map>
#include <memory>

#include "base/metrics/histogram_samples.h"
#include "base/test/metrics/histogram_tester.h"
#include "content/browser/media/session/media_session_player_observer.h"
#include "content/browser/media/session/mock_media_session_service_impl.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "media/base/media_content_type.h"
#include "services/media_session/public/mojom/media_session.mojom.h"

namespace content {

using MediaSessionUserAction = MediaSessionUmaHelper::MediaSessionUserAction;
using SuspendType = MediaSession::SuspendType;
using MediaSessionAction = media_session::mojom::MediaSessionAction;

namespace {

static const int kPlayerId = 0;

class MockMediaSessionPlayerObserver : public MediaSessionPlayerObserver {
 public:
  MockMediaSessionPlayerObserver(RenderFrameHost* rfh,
                                 media::MediaContentType media_content_type)
      : render_frame_host_(rfh), media_content_type_(media_content_type) {}
  explicit MockMediaSessionPlayerObserver(
      media::MediaContentType media_content_type)
      : MockMediaSessionPlayerObserver(nullptr, media_content_type) {}

  ~MockMediaSessionPlayerObserver() override = default;

  void OnSuspend(int player_id) override {}
  void OnResume(int player_id) override {}
  void OnSeekForward(int player_id, base::TimeDelta seek_time) override {}
  void OnSeekBackward(int player_id, base::TimeDelta seek_time) override {}
  void OnSeekTo(int player_id, base::TimeDelta seek_time) override {}
  void OnSetVolumeMultiplier(int player_id, double volume_multiplier) override {
  }
  void OnEnterPictureInPicture(int player_id) override {}
  void OnExitPictureInPicture(int player_id) override {}
  void OnSetAudioSinkId(int player_id,
                        const std::string& raw_device_id) override {}
  void OnSetMute(int player_id, bool mute) override {}
  void OnRequestMediaRemoting(int player_id) override {}

  absl::optional<media_session::MediaPosition> GetPosition(
      int player_id) const override {
    return absl::nullopt;
  }

  bool IsPictureInPictureAvailable(int player_id) const override {
    return false;
  }

  bool HasAudio(int player_id) const override { return true; }
  bool HasVideo(int player_id) const override { return false; }

  std::string GetAudioOutputSinkId(int player_id) const override { return ""; }

  bool SupportsAudioOutputDeviceSwitching(int player_id) const override {
    return false;
  }

  media::MediaContentType GetMediaContentType() const override {
    return media_content_type_;
  }

  void SetMediaContentType(media::MediaContentType media_content_type) {
    media_content_type_ = media_content_type;
  }

  RenderFrameHost* render_frame_host() const override {
    return render_frame_host_;
  }

 private:
  raw_ptr<RenderFrameHost, DanglingUntriaged> render_frame_host_;
  media::MediaContentType media_content_type_;
};

struct ActionMappingEntry {
  media_session::mojom::MediaSessionAction action;
  MediaSessionUserAction user_action;
};

ActionMappingEntry kActionMappings[] = {
    {MediaSessionAction::kPlay, MediaSessionUserAction::kPlay},
    {MediaSessionAction::kPause, MediaSessionUserAction::kPause},
    {MediaSessionAction::kPreviousTrack,
     MediaSessionUserAction::kPreviousTrack},
    {MediaSessionAction::kNextTrack, MediaSessionUserAction::kNextTrack},
    {MediaSessionAction::kSeekBackward, MediaSessionUserAction::kSeekBackward},
    {MediaSessionAction::kSeekForward, MediaSessionUserAction::kSeekForward},
    {MediaSessionAction::kSkipAd, MediaSessionUserAction::kSkipAd},
    {MediaSessionAction::kStop, MediaSessionUserAction::kStop},
    {MediaSessionAction::kSeekTo, MediaSessionUserAction::kSeekTo},
    {MediaSessionAction::kScrubTo, MediaSessionUserAction::kScrubTo},
    {MediaSessionAction::kEnterPictureInPicture,
     MediaSessionUserAction::kEnterPictureInPicture},
    {MediaSessionAction::kExitPictureInPicture,
     MediaSessionUserAction::kExitPictureInPicture},
    {MediaSessionAction::kSwitchAudioDevice,
     MediaSessionUserAction::kSwitchAudioDevice},
    {MediaSessionAction::kToggleMicrophone,
     MediaSessionUserAction::kToggleMicrophone},
    {MediaSessionAction::kToggleCamera, MediaSessionUserAction::kToggleCamera},
    {MediaSessionAction::kHangUp, MediaSessionUserAction::kHangUp},
    {MediaSessionAction::kRaise, MediaSessionUserAction::kRaise},
    {MediaSessionAction::kSetMute, MediaSessionUserAction::kSetMute},
    {MediaSessionAction::kPreviousSlide,
     MediaSessionUserAction::kPreviousSlide},
    {MediaSessionAction::kNextSlide, MediaSessionUserAction::kNextSlide},
};

}  // anonymous namespace

class MediaSessionImplUmaTest : public RenderViewHostImplTestHarness {
 public:
  MediaSessionImplUmaTest() = default;
  ~MediaSessionImplUmaTest() override = default;

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();

    contents()->GetPrimaryMainFrame()->InitializeRenderFrameIfNeeded();
    StartPlayer();

    mock_media_session_service_ =
        std::make_unique<testing::NiceMock<MockMediaSessionServiceImpl>>(
            contents()->GetPrimaryMainFrame());
  }

  void TearDown() override {
    mock_media_session_service_.reset();
    RenderViewHostImplTestHarness::TearDown();
  }

 protected:
  MediaSessionImpl* GetSession() { return MediaSessionImpl::Get(contents()); }

  void StartPlayer() {
    player_ = std::make_unique<MockMediaSessionPlayerObserver>(
        contents()->GetPrimaryMainFrame(), media::MediaContentType::Persistent);
    GetSession()->AddPlayer(player_.get(), kPlayerId);
  }

  std::unique_ptr<base::HistogramSamples> GetHistogramSamplesSinceTestStart(
      const std::string& name) {
    return histogram_tester_.GetHistogramSamplesSinceCreation(name);
  }

  std::unique_ptr<MockMediaSessionServiceImpl> mock_media_session_service_;
  std::unique_ptr<MockMediaSessionPlayerObserver> player_;
  base::HistogramTester histogram_tester_;
};

TEST_F(MediaSessionImplUmaTest, RecordPauseDefaultOnUISuspend) {
  GetSession()->Suspend(SuspendType::kUI);
  std::unique_ptr<base::HistogramSamples> samples(
      GetHistogramSamplesSinceTestStart("Media.Session.UserAction"));
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(static_cast<base::HistogramBase::Sample>(
                   MediaSessionUserAction::kPauseDefault)));
}

TEST_F(MediaSessionImplUmaTest, RecordPauseDefaultOnUISuspendWithAction) {
  mock_media_session_service_->EnableAction(
      media_session::mojom::MediaSessionAction::kPause);

  GetSession()->Suspend(SuspendType::kUI);

  std::unique_ptr<base::HistogramSamples> samples(
      GetHistogramSamplesSinceTestStart("Media.Session.UserAction"));
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(static_cast<base::HistogramBase::Sample>(
                   MediaSessionUserAction::kPause)));
}

TEST_F(MediaSessionImplUmaTest, RecordPauseDefaultOnSystemSuspend) {
  GetSession()->Suspend(SuspendType::kSystem);
  std::unique_ptr<base::HistogramSamples> samples(
      GetHistogramSamplesSinceTestStart("Media.Session.UserAction"));
  EXPECT_EQ(0, samples->TotalCount());
}

TEST_F(MediaSessionImplUmaTest, RecordPauseDefaultOnContentSuspend) {
  GetSession()->Suspend(SuspendType::kContent);
  std::unique_ptr<base::HistogramSamples> samples(
      GetHistogramSamplesSinceTestStart("Media.Session.UserAction"));
  EXPECT_EQ(0, samples->TotalCount());
}

TEST_F(MediaSessionImplUmaTest, RecordPauseDefaultOnUIResume) {
  GetSession()->Suspend(SuspendType::kSystem);
  GetSession()->Resume(SuspendType::kUI);
  std::unique_ptr<base::HistogramSamples> samples(
      GetHistogramSamplesSinceTestStart("Media.Session.UserAction"));
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(static_cast<base::HistogramBase::Sample>(
                   MediaSessionUserAction::kPlayDefault)));
}

TEST_F(MediaSessionImplUmaTest, RecordPauseDefaultOnUIResumeWithAction) {
  mock_media_session_service_->EnableAction(
      media_session::mojom::MediaSessionAction::kPlay);

  GetSession()->Suspend(SuspendType::kSystem);
  GetSession()->Resume(SuspendType::kUI);

  std::unique_ptr<base::HistogramSamples> samples(
      GetHistogramSamplesSinceTestStart("Media.Session.UserAction"));
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(static_cast<base::HistogramBase::Sample>(
                   MediaSessionUserAction::kPlay)));
}

TEST_F(MediaSessionImplUmaTest, RecordPauseDefaultOnSystemResume) {
  GetSession()->Suspend(SuspendType::kSystem);
  GetSession()->Resume(SuspendType::kSystem);
  std::unique_ptr<base::HistogramSamples> samples(
      GetHistogramSamplesSinceTestStart("Media.Session.UserAction"));
  EXPECT_EQ(0, samples->TotalCount());
}

// This should never happen but just check this to be safe.
TEST_F(MediaSessionImplUmaTest, RecordPauseDefaultOnContentResume) {
  GetSession()->Suspend(SuspendType::kSystem);
  GetSession()->Resume(SuspendType::kContent);
  std::unique_ptr<base::HistogramSamples> samples(
      GetHistogramSamplesSinceTestStart("Media.Session.UserAction"));
  EXPECT_EQ(0, samples->TotalCount());
}

TEST_F(MediaSessionImplUmaTest, RecordPauseDefaultOnUIStop) {
  GetSession()->Stop(SuspendType::kUI);
  std::unique_ptr<base::HistogramSamples> samples(
      GetHistogramSamplesSinceTestStart("Media.Session.UserAction"));
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(static_cast<base::HistogramBase::Sample>(
                   MediaSessionUserAction::kStopDefault)));
}

// This should never happen but just check this to be safe.
TEST_F(MediaSessionImplUmaTest, RecordPauseDefaultOnSystemStop) {
  GetSession()->Stop(SuspendType::kSystem);
  std::unique_ptr<base::HistogramSamples> samples(
      GetHistogramSamplesSinceTestStart("Media.Session.UserAction"));
  EXPECT_EQ(0, samples->TotalCount());
}

TEST_F(MediaSessionImplUmaTest, RecordMediaSessionAction) {
  for (const auto& mapping_entry : kActionMappings) {
    // Uniquely create a HistogramTester for each action to check the histograms
    // for each action independently.
    base::HistogramTester histogram_tester;
    GetSession()->DidReceiveAction(mapping_entry.action);

    std::unique_ptr<base::HistogramSamples> samples(
        histogram_tester.GetHistogramSamplesSinceCreation(
            "Media.Session.UserAction"));
    EXPECT_EQ(1, samples->TotalCount());
    EXPECT_EQ(1, samples->GetCount(static_cast<base::HistogramBase::Sample>(
                     mapping_entry.user_action)));
  }
}

}  // namespace content
