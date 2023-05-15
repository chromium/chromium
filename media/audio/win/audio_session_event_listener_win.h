// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_WIN_AUDIO_SESSION_EVENT_LISTENER_WIN_H_
#define MEDIA_AUDIO_WIN_AUDIO_SESSION_EVENT_LISTENER_WIN_H_

#include <audiopolicy.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include "base/functional/callback.h"
#include "media/base/media_export.h"

namespace media {

class MEDIA_EXPORT AudioSessionEventListener
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IAudioSessionEvents> {
 public:
  // Handles event callbacks from
  // IAudioSessionControl::RegisterAudioSessionNotification().  Runs
  // |device_change_cb| when OnSessionDisconnected() is called.
  //
  // Since the IAudioClient session is dead after the disconnection, we use a
  // OnceCallback. The delivery of this notification is fatal to the |client|.
  AudioSessionEventListener(base::OnceClosure device_change_cb);
  ~AudioSessionEventListener() override;

 private:
  friend class AudioSessionEventListenerTest;

  // IAudioSessionEvents implementation.
  IFACEMETHODIMP OnChannelVolumeChanged(DWORD channel_count,
                                        float new_channel_volume_array[],
                                        DWORD changed_channel,
                                        LPCGUID event_context) override;
  IFACEMETHODIMP OnDisplayNameChanged(LPCWSTR new_display_name,
                                      LPCGUID event_context) override;
  IFACEMETHODIMP OnGroupingParamChanged(LPCGUID new_grouping_param,
                                        LPCGUID event_context) override;
  IFACEMETHODIMP OnIconPathChanged(LPCWSTR new_icon_path,
                                   LPCGUID event_context) override;
  IFACEMETHODIMP OnSessionDisconnected(
      AudioSessionDisconnectReason disconnect_reason) override;
  IFACEMETHODIMP OnSimpleVolumeChanged(float new_volume,
                                       BOOL new_mute,
                                       LPCGUID event_context) override;
  IFACEMETHODIMP OnStateChanged(AudioSessionState new_state) override;

  base::OnceClosure device_change_cb_;
};

}  // namespace media

#endif  // MEDIA_AUDIO_WIN_AUDIO_SESSION_EVENT_LISTENER_WIN_H_
