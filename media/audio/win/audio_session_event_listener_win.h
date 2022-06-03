// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_WIN_AUDIO_SESSION_EVENT_LISTENER_WIN_H_
#define MEDIA_AUDIO_WIN_AUDIO_SESSION_EVENT_LISTENER_WIN_H_

#include <audiopolicy.h>
#include <wrl/client.h>

#include "base/callback.h"
#include "media/base/media_export.h"

namespace media {

class MEDIA_EXPORT AudioSessionEventListener : public IAudioSessionEvents {
 public:
  // Calls RegisterAudioSessionNotification() on |client| and calls
  // |device_change_cb| when OnSessionDisconnected() is called.
  //
  // Since the IAudioClient session is dead after the disconnection, we use a
  // OnceCallback. The delivery of this notification is fatal to the |client|.
  AudioSessionEventListener(IAudioClient* client,
                            base::OnceClosure device_change_cb);
  virtual ~AudioSessionEventListener();

 private:
  friend class AudioSessionEventListenerTest;

  IFACEMETHODIMP_(ULONG) AddRef() override;
  IFACEMETHODIMP_(ULONG) Release() override;
  IFACEMETHODIMP QueryInterface(REFIID iid, void** object) override;

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
  Microsoft::WRL::ComPtr<IAudioSessionControl> audio_session_control_;
};

}  // namespace media

#endif  // MEDIA_AUDIO_WIN_AUDIO_SESSION_EVENT_LISTENER_WIN_H_
