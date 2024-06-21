// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/session/mock_media_session_player_observer.h"
#include "content/public/browser/render_frame_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

MockMediaSessionPlayerObserver::MockMediaSessionPlayerObserver(
    RenderFrameHost* render_frame_host,
    media::MediaContentType media_content_type)
    : render_frame_host_global_id_(
          render_frame_host
              ? std::make_optional(render_frame_host->GetGlobalId())
              : std::nullopt),
      media_content_type_(media_content_type) {}

MockMediaSessionPlayerObserver::MockMediaSessionPlayerObserver(
    media::MediaContentType media_content_type)
    : MockMediaSessionPlayerObserver(nullptr, media_content_type) {}

MockMediaSessionPlayerObserver::~MockMediaSessionPlayerObserver() = default;

void MockMediaSessionPlayerObserver::OnSuspend(int player_id) {
  EXPECT_GE(player_id, 0);
  EXPECT_GT(players_.size(), static_cast<size_t>(player_id));

  ++received_suspend_calls_;
  players_[player_id].is_playing_ = false;
}

void MockMediaSessionPlayerObserver::OnResume(int player_id) {
  EXPECT_GE(player_id, 0);
  EXPECT_GT(players_.size(), static_cast<size_t>(player_id));

  ++received_resume_calls_;
  players_[player_id].is_playing_ = true;
}

void MockMediaSessionPlayerObserver::OnSeekForward(int player_id,
                                                   base::TimeDelta seek_time) {
  EXPECT_GE(player_id, 0);
  EXPECT_GT(players_.size(), static_cast<size_t>(player_id));
  ++received_seek_forward_calls_;
}

void MockMediaSessionPlayerObserver::OnSeekBackward(int player_id,
                                                    base::TimeDelta seek_time) {
  EXPECT_GE(player_id, 0);
  EXPECT_GT(players_.size(), static_cast<size_t>(player_id));
  ++received_seek_backward_calls_;
}

void MockMediaSessionPlayerObserver::OnSeekTo(int player_id,
                                              base::TimeDelta seek_time) {
  EXPECT_GE(player_id, 0);
  EXPECT_GT(players_.size(), static_cast<size_t>(player_id));
  ++received_seek_to_calls_;
}

void MockMediaSessionPlayerObserver::OnSetVolumeMultiplier(
    int player_id,
    double volume_multiplier) {
  EXPECT_GE(player_id, 0);
  EXPECT_GT(players_.size(), static_cast<size_t>(player_id));

  EXPECT_GE(volume_multiplier, 0.0f);
  EXPECT_LE(volume_multiplier, 1.0f);

  players_[player_id].volume_multiplier_ = volume_multiplier;
}

void MockMediaSessionPlayerObserver::OnEnterPictureInPicture(int player_id) {
  EXPECT_GE(player_id, 0);
  EXPECT_EQ(players_.size(), 1u);

  ++received_enter_picture_in_picture_calls_;
  players_[player_id].is_in_picture_in_picture_ = true;
}

void MockMediaSessionPlayerObserver::OnSetAudioSinkId(
    int player_id,
    const std::string& raw_device_id) {
  EXPECT_GE(player_id, 0);
  EXPECT_GT(players_.size(), static_cast<size_t>(player_id));

  ++received_set_audio_sink_id_calls_;
  players_[player_id].audio_sink_id_ = raw_device_id;
}

void MockMediaSessionPlayerObserver::OnSetMute(int player_id, bool mute) {
  EXPECT_GE(player_id, 0);
  EXPECT_GT(players_.size(), static_cast<size_t>(player_id));
}

void MockMediaSessionPlayerObserver::OnRequestMediaRemoting(int player_id) {
  EXPECT_GE(player_id, 0);
  EXPECT_GT(players_.size(), static_cast<size_t>(player_id));
}

void MockMediaSessionPlayerObserver::OnRequestVisibility(
    int player_id,
    RequestVisibilityCallback request_visibility_callback) {
  EXPECT_GE(player_id, 0);
  EXPECT_GT(players_.size(), static_cast<size_t>(player_id));
  std::move(request_visibility_callback)
      .Run(HasSufficientlyVisibleVideo(player_id));
  ++received_request_visibility_calls_;
}

std::optional<media_session::MediaPosition>
MockMediaSessionPlayerObserver::GetPosition(int player_id) const {
  EXPECT_GE(player_id, 0);
  EXPECT_GT(players_.size(), static_cast<size_t>(player_id));
  return players_[player_id].position_;
}

bool MockMediaSessionPlayerObserver::IsPictureInPictureAvailable(
    int player_id) const {
  EXPECT_GE(player_id, 0);
  EXPECT_GT(players_.size(), static_cast<size_t>(player_id));
  return false;
}

bool MockMediaSessionPlayerObserver::HasSufficientlyVisibleVideo(
    int player_id) const {
  EXPECT_GE(player_id, 0);
  EXPECT_GT(players_.size(), static_cast<size_t>(player_id));
  return players_[player_id].has_sufficiently_visible_video_;
}

RenderFrameHost* MockMediaSessionPlayerObserver::render_frame_host() const {
  if (render_frame_host_global_id_.has_value()) {
    return RenderFrameHost::FromID(render_frame_host_global_id_.value());
  }
  return nullptr;
}

int MockMediaSessionPlayerObserver::StartNewPlayer() {
  players_.push_back(MockPlayer(true, 1.0f));
  return players_.size() - 1;
}

bool MockMediaSessionPlayerObserver::IsPlaying(size_t player_id) {
  EXPECT_GT(players_.size(), player_id);
  return players_[player_id].is_playing_;
}

double MockMediaSessionPlayerObserver::GetVolumeMultiplier(size_t player_id) {
  EXPECT_GT(players_.size(), player_id);
  return players_[player_id].volume_multiplier_;
}

void MockMediaSessionPlayerObserver::SetAudioSinkId(size_t player_id,
                                                    std::string sink_id) {
  EXPECT_GT(players_.size(), player_id);
  players_[player_id].audio_sink_id_ = std::move(sink_id);
}

void MockMediaSessionPlayerObserver::SetPlaying(size_t player_id,
                                                bool playing) {
  EXPECT_GT(players_.size(), player_id);
  players_[player_id].is_playing_ = playing;
}

void MockMediaSessionPlayerObserver::SetPosition(
    size_t player_id,
    media_session::MediaPosition& position) {
  EXPECT_GT(players_.size(), player_id);
  players_[player_id].position_ = position;
}

void MockMediaSessionPlayerObserver::SetHasSufficientlyVisibleVideo(
    size_t player_id,
    bool has_sufficiently_visible_video) {
  EXPECT_GT(players_.size(), player_id);
  players_[player_id].has_sufficiently_visible_video_ =
      has_sufficiently_visible_video;
}

int MockMediaSessionPlayerObserver::received_suspend_calls() const {
  return received_suspend_calls_;
}

int MockMediaSessionPlayerObserver::received_resume_calls() const {
  return received_resume_calls_;
}

int MockMediaSessionPlayerObserver::received_seek_forward_calls() const {
  return received_seek_forward_calls_;
}

int MockMediaSessionPlayerObserver::received_seek_backward_calls() const {
  return received_seek_backward_calls_;
}

int MockMediaSessionPlayerObserver::received_seek_to_calls() const {
  return received_seek_to_calls_;
}

int MockMediaSessionPlayerObserver::received_enter_picture_in_picture_calls()
    const {
  return received_enter_picture_in_picture_calls_;
}

int MockMediaSessionPlayerObserver::received_exit_picture_in_picture_calls()
    const {
  return received_exit_picture_in_picture_calls_;
}

int MockMediaSessionPlayerObserver::received_set_audio_sink_id_calls() const {
  return received_set_audio_sink_id_calls_;
}

int MockMediaSessionPlayerObserver::received_request_visibility_calls() const {
  return received_request_visibility_calls_;
}

bool MockMediaSessionPlayerObserver::HasAudio(int player_id) const {
  EXPECT_GE(player_id, 0);
  EXPECT_GT(players_.size(), static_cast<size_t>(player_id));
  return true;
}

bool MockMediaSessionPlayerObserver::HasVideo(int player_id) const {
  EXPECT_GE(player_id, 0);
  EXPECT_GT(players_.size(), static_cast<size_t>(player_id));
  return false;
}

bool MockMediaSessionPlayerObserver::IsPaused(int player_id) const {
  EXPECT_GE(player_id, 0);
  EXPECT_GT(players_.size(), static_cast<size_t>(player_id));
  return !players_[player_id].is_playing_;
}

std::string MockMediaSessionPlayerObserver::GetAudioOutputSinkId(
    int player_id) const {
  EXPECT_GE(player_id, 0);
  EXPECT_GT(players_.size(), static_cast<size_t>(player_id));
  return players_.at(player_id).audio_sink_id_;
}

bool MockMediaSessionPlayerObserver::SupportsAudioOutputDeviceSwitching(
    int player_id) const {
  EXPECT_GE(player_id, 0);
  EXPECT_GT(players_.size(), static_cast<size_t>(player_id));
  return players_.at(player_id).supports_device_switching_;
}

media::MediaContentType MockMediaSessionPlayerObserver::GetMediaContentType()
    const {
  return media_content_type_;
}

void MockMediaSessionPlayerObserver::SetMediaContentType(
    media::MediaContentType media_content_type) {
  media_content_type_ = media_content_type;
}

MockMediaSessionPlayerObserver::MockPlayer::MockPlayer(bool is_playing,
                                                       double volume_multiplier)
    : is_playing_(is_playing), volume_multiplier_(volume_multiplier) {}

MockMediaSessionPlayerObserver::MockPlayer::~MockPlayer() = default;

MockMediaSessionPlayerObserver::MockPlayer::MockPlayer(const MockPlayer&) =
    default;

}  // namespace content
