// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_SESSION_PEPPER_PLAYER_DELEGATE_H_
#define CONTENT_BROWSER_MEDIA_SESSION_PEPPER_PLAYER_DELEGATE_H_

#include <stdint.h>

#include "base/macros.h"
#include "content/browser/media/session/media_session_player_observer.h"

namespace content {

class RenderFrameHost;

class PepperPlayerDelegate : public MediaSessionPlayerObserver {
 public:
  // The Id can only be 0 for PepperPlayerDelegate. Declare the constant here so
  // it can be used elsewhere.
  static const int kPlayerId;

  PepperPlayerDelegate(RenderFrameHost* render_frame_host, int32_t pp_instance);
  ~PepperPlayerDelegate() override;

  // MediaSessionPlayerObserver implementation.
  void OnSuspend(int player_id) override;
  void OnResume(int player_id) override;
  void OnSeekForward(int player_id, base::TimeDelta seek_time) override;
  void OnSeekBackward(int player_id, base::TimeDelta seek_time) override;
  void OnSetVolumeMultiplier(int player_id,
                             double volume_multiplier) override;
  base::Optional<media_session::MediaPosition> GetPosition(
      int player_id) const override;
  RenderFrameHost* render_frame_host() const override;

 private:
  void SetVolume(int player_id, double volume);

  RenderFrameHost* render_frame_host_;
  int32_t pp_instance_;

  DISALLOW_COPY_AND_ASSIGN(PepperPlayerDelegate);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_SESSION_PEPPER_PLAYER_DELEGATE_H_
