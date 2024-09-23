// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_SESSION_PEPPER_PLAYER_DELEGATE_H_
#define CONTENT_BROWSER_MEDIA_SESSION_PEPPER_PLAYER_DELEGATE_H_

#include <stdint.h>

#include "base/memory/raw_ptr.h"
#include "content/browser/media/session/media_session_player_observer.h"

namespace content {

class RenderFrameHost;

class PepperPlayerDelegate : public MediaSessionPlayerObserver {
 public:
  // The Id can only be 0 for PepperPlayerDelegate. Declare the constant here so
  // it can be used elsewhere.
  static const int kPlayerId;

  PepperPlayerDelegate(RenderFrameHost* render_frame_host,
                       int32_t pp_instance,
                       media::MediaContentType media_content_type);

  PepperPlayerDelegate(const PepperPlayerDelegate&) = delete;
  PepperPlayerDelegate& operator=(const PepperPlayerDelegate&) = delete;

  ~PepperPlayerDelegate() override;

  // MediaSessionPlayerObserver implementation.
  void OnSuspend(int player_id) override;
  void OnResume(int player_id) override;
  void OnSeekForward(int player_id, base::TimeDelta seek_time) override;
  void OnSeekBackward(int player_id, base::TimeDelta seek_time) override;
  void OnSeekTo(int player_id, base::TimeDelta seek_time) override;
  void OnSetVolumeMultiplier(int player_id, double volume_multiplier) override;
  void OnEnterPictureInPicture(int player_id) override;
  void OnSetAudioSinkId(int player_id,
                        const std::string& raw_device_id) override;
  void OnSetMute(int player_id, bool mute) override;
  void OnRequestMediaRemoting(int player_id) override {}
  std::optional<media_session::MediaPosition> GetPosition(
      int player_id) const override;
  void OnRequestVisibility(
      int player_id,
      RequestVisibilityCallback request_visibility_callback) override {}
  bool IsPictureInPictureAvailable(int player_id) const override;
  bool HasSufficientlyVisibleVideo(int player_id) const override;
  RenderFrameHost* render_frame_host() const override;
  bool HasAudio(int player_id) const override;
  bool HasVideo(int player_id) const override;
  bool IsPaused(int player_id) const override;
  std::string GetAudioOutputSinkId(int player_id) const override;
  bool SupportsAudioOutputDeviceSwitching(int player_id) const override;
  media::MediaContentType GetMediaContentType() const override;

 private:
  void SetVolume(int player_id, double volume);

  raw_ptr<RenderFrameHost> render_frame_host_;
  int32_t pp_instance_;
  const media::MediaContentType media_content_type_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_SESSION_PEPPER_PLAYER_DELEGATE_H_
