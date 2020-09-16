// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/session/pepper_player_delegate.h"

#include "base/command_line.h"
#include "content/browser/media/session/pepper_playback_observer.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/common/frame_messages.h"
#include "media/base/media_switches.h"
#include "services/media_session/public/cpp/media_position.h"

namespace content {

namespace {

const double kDuckVolume = 0.2f;

}  // anonymous namespace

const int PepperPlayerDelegate::kPlayerId = 0;

PepperPlayerDelegate::PepperPlayerDelegate(RenderFrameHost* render_frame_host,
                                           int32_t pp_instance)
    : render_frame_host_(render_frame_host), pp_instance_(pp_instance) {}

PepperPlayerDelegate::~PepperPlayerDelegate() = default;

void PepperPlayerDelegate::OnSuspend(int player_id) {
  if (!base::FeatureList::IsEnabled(media::kAudioFocusDuckFlash))
    return;

  // Pepper player cannot be really suspended. Duck the volume instead.
  DCHECK_EQ(player_id, kPlayerId);
  SetVolume(player_id, kDuckVolume);
}

void PepperPlayerDelegate::OnResume(int player_id) {
  if (!base::FeatureList::IsEnabled(media::kAudioFocusDuckFlash))
    return;

  DCHECK_EQ(player_id, kPlayerId);
  SetVolume(player_id, 1.0f);
}

void PepperPlayerDelegate::OnSeekForward(int player_id,
                                         base::TimeDelta seek_time) {
  // Cannot seek pepper player. Do nothing.
}

void PepperPlayerDelegate::OnSeekBackward(int player_id,
                                          base::TimeDelta seek_time) {
  // Cannot seek pepper player. Do nothing.
}

void PepperPlayerDelegate::OnSetVolumeMultiplier(int player_id,
                                                 double volume_multiplier) {
  if (!base::FeatureList::IsEnabled(media::kAudioFocusDuckFlash))
    return;

  DCHECK_EQ(player_id, kPlayerId);
  SetVolume(player_id, volume_multiplier);
}

void PepperPlayerDelegate::OnEnterPictureInPicture(int player_id) {
  // Pepper player cannot enter picture-in-picture. Do nothing.
}

void PepperPlayerDelegate::OnExitPictureInPicture(int player_id) {
  // Pepper player cannot exit picture-in-picture. Do nothing.
}

void PepperPlayerDelegate::OnSetAudioSinkId(int player_id,
                                            const std::string& raw_device_id) {
  // Pepper player cannot change audio sinks. Do nothing.
  NOTREACHED();
}

base::Optional<media_session::MediaPosition> PepperPlayerDelegate::GetPosition(
    int player_id) const {
  // Pepper does not support position data.
  DCHECK_EQ(player_id, kPlayerId);
  return base::nullopt;
}

bool PepperPlayerDelegate::IsPictureInPictureAvailable(int player_id) const {
  DCHECK_EQ(player_id, kPlayerId);
  return false;
}

RenderFrameHost* PepperPlayerDelegate::render_frame_host() const {
  return render_frame_host_;
}

void PepperPlayerDelegate::SetVolume(int player_id, double volume) {
  render_frame_host_->Send(new FrameMsg_SetPepperVolume(
      render_frame_host_->GetRoutingID(), pp_instance_, volume));
}

bool PepperPlayerDelegate::HasVideo(int player_id) const {
  // We don't actually know whether a pepper player has both audio/video.
  return true;
}

std::string PepperPlayerDelegate::GetAudioOutputSinkId(int player_id) const {
  // This operation is not supported for pepper players.
  return "";
}

bool PepperPlayerDelegate::SupportsAudioOutputDeviceSwitching(
    int player_id) const {
  return false;
}

}  // namespace content
