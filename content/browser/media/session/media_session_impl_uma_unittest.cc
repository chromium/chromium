// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/session/media_session_impl.h"

#include <map>
#include <memory>

#include "base/metrics/histogram_samples.h"
#include "base/test/metrics/histogram_tester.h"
#include "content/browser/media/session/media_session_player_observer.h"
#include "content/browser/media/session/mock_media_session_service_impl.h"
#include "content/public/test/test_service_manager_context.h"
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
  explicit MockMediaSessionPlayerObserver(RenderFrameHost* rfh)
      : render_frame_host_(rfh) {}

  ~MockMediaSessionPlayerObserver() override = default;

  void OnSuspend(int player_id) override {}
  void OnResume(int player_id) override {}
  void OnSeekForward(int player_id, base::TimeDelta seek_time) override {}
  void OnSeekBackward(int player_id, base::TimeDelta seek_time) override {}
  void OnSetVolumeMultiplier(int player_id, double volume_multiplier) override {
  }

  base::Optional<media_session::MediaPosition> GetPosition(
      int player_id) const override {
    return base::nullopt;
  }

  RenderFrameHost* render_frame_host() const override {
    return render_frame_host_;
  }

 private:
  RenderFrameHost* render_frame_host_;
};

struct ActionMappingEntry {
  media_session::mojom::MediaSessionAction action;
  MediaSessionUserAction user_action;
};

ActionMappingEntry kActionMappings[] = {
    {MediaSessionAction::kPlay, MediaSessionUserAction::Play},
    {MediaSessionAction::kPause, MediaSessionUserAction::Pause},
    {MediaSessionAction::kPreviousTrack, MediaSessionUserAction::PreviousTrack},
    {MediaSessionAction::kNextTrack, MediaSessionUserAction::NextTrack},
    {MediaSessionAction::kSeekBackward, MediaSessionUserAction::SeekBackward},
    {MediaSessionAction::kSeekForward, MediaSessionUserAction::SeekForward},
    {MediaSessionAction::kSkipAd, MediaSessionUserAction::SkipAd},
};

}  // anonymous namespace

class MediaSessionImplUmaTest : public RenderViewHostImplTestHarness {
 public:
  MediaSessionImplUmaTest() = default;
  ~MediaSessionImplUmaTest() override = default;

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();

    test_service_manager_context_ =
        std::make_unique<content::TestServiceManagerContext>();

    contents()->GetMainFrame()->InitializeRenderFrameIfNeeded();
    StartPlayer();

    mock_media_session_service_.reset(
        new testing::NiceMock<MockMediaSessionServiceImpl>(
            contents()->GetMainFrame()));
  }

  void TearDown() override {
    mock_media_session_service_.reset();
    test_service_manager_context_.reset();
    RenderViewHostImplTestHarness::TearDown();
  }

 protected:
  MediaSessionImpl* GetSession() { return MediaSessionImpl::Get(contents()); }

  void StartPlayer() {
    player_.reset(
        new MockMediaSessionPlayerObserver(contents()->GetMainFrame()));
    GetSession()->AddPlayer(player_.get(), kPlayerId,
                            media::MediaContentType::Persistent);
  }

  std::unique_ptr<base::HistogramSamples> GetHistogramSamplesSinceTestStart(
      const std::string& name) {
    return histogram_tester_.GetHistogramSamplesSinceCreation(name);
  }

  std::unique_ptr<MockMediaSessionServiceImpl> mock_media_session_service_;
  std::unique_ptr<MockMediaSessionPlayerObserver> player_;
  base::HistogramTester histogram_tester_;

 private:
  std::unique_ptr<content::TestServiceManagerContext>
      test_service_manager_context_;
};

TEST_F(MediaSessionImplUmaTest, RecordPauseDefaultOnUISuspend) {
  GetSession()->Suspend(SuspendType::kUI);
  std::unique_ptr<base::HistogramSamples> samples(
      GetHistogramSamplesSinceTestStart("Media.Session.UserAction"));
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(static_cast<base::HistogramBase::Sample>(
                   MediaSessionUserAction::PauseDefault)));
}

TEST_F(MediaSessionImplUmaTest, RecordPauseDefaultOnUISuspendWithAction) {
  mock_media_session_service_->EnableAction(
      media_session::mojom::MediaSessionAction::kPause);

  GetSession()->Suspend(SuspendType::kUI);

  std::unique_ptr<base::HistogramSamples> samples(
      GetHistogramSamplesSinceTestStart("Media.Session.UserAction"));
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(static_cast<base::HistogramBase::Sample>(
                   MediaSessionUserAction::Pause)));
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
                   MediaSessionUserAction::PlayDefault)));
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
                   MediaSessionUserAction::Play)));
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
                   MediaSessionUserAction::StopDefault)));
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
