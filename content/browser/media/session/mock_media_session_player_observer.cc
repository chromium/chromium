// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/session/mock_media_session_player_observer.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace content {

MockMediaSessionPlayerObserver::MockMediaSessionPlayerObserver(
    RenderFrameHost* render_frame_host)
    : MediaSessionPlayerObserver(), render_frame_host_(render_frame_host) {}

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

void MockMediaSessionPlayerObserver::OnSetVolumeMultiplier(
    int player_id,
    double volume_multiplier) {
  EXPECT_GE(player_id, 0);
  EXPECT_GT(players_.size(), static_cast<size_t>(player_id));

  EXPECT_GE(volume_multiplier, 0.0f);
  EXPECT_LE(volume_multiplier, 1.0f);

  players_[player_id].volume_multiplier_ = volume_multiplier;
}

base::Optional<media_session::MediaPosition>
MockMediaSessionPlayerObserver::GetPosition(int player_id) const {
  EXPECT_GE(player_id, 0);
  EXPECT_GT(players_.size(), static_cast<size_t>(player_id));
  return players_[player_id].position_;
}

RenderFrameHost* MockMediaSessionPlayerObserver::render_frame_host() const {
  return render_frame_host_;
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

MockMediaSessionPlayerObserver::MockPlayer::MockPlayer(bool is_playing,
                                                       double volume_multiplier)
    : is_playing_(is_playing), volume_multiplier_(volume_multiplier) {}

MockMediaSessionPlayerObserver::MockPlayer::~MockPlayer() = default;

MockMediaSessionPlayerObserver::MockPlayer::MockPlayer(const MockPlayer&) =
    default;

}  // namespace content
