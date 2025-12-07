// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_WIN_AUDIO_DUCKER_WIN_H_
#define MEDIA_AUDIO_WIN_AUDIO_DUCKER_WIN_H_

#include <mmdeviceapi.h>

#include <audiopolicy.h>
#include <wrl/client.h>

#include <map>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process_handle.h"
#include "media/base/media_export.h"

namespace media {

class AudioDeviceListenerWin;
class AudioSessionCreationObserverWin;

// The AudioDuckerWin is responsible for ducking Windows applications.
class MEDIA_EXPORT AudioDuckerWin {
 public:
  using ShouldDuckProcessCallback =
      base::RepeatingCallback<bool(base::ProcessId)>;

  // `callback` should return true if the application associated with the given
  // process ID should be ducked.
  explicit AudioDuckerWin(ShouldDuckProcessCallback callback);
  AudioDuckerWin(const AudioDuckerWin&) = delete;
  AudioDuckerWin& operator=(const AudioDuckerWin&) = delete;
  ~AudioDuckerWin();

  // Start ducking all Windows applications for which
  // `should_duck_process_callback_` returns true.
  void StartDuckingOtherWindowsApplications();

  // Stop ducking all applications ducked by
  // `StartDuckingOtherWindowsApplications()`.
  void StopDuckingOtherWindowsApplications();

 private:
  void StartDuckingAudioSessionIfNecessary(
      Microsoft::WRL::ComPtr<IAudioSessionControl2>& session);
  void StopDuckingAudioSessionIfNecessary(
      Microsoft::WRL::ComPtr<IAudioSessionControl2>& session);

  // Called by `output_device_listener_` when the default audio output device
  // changes.
  void OnDefaultDeviceChanged();

  // Called when the system notifies us of a new audio session being created.
  void DuckNewAudioSessionsIfNecessary();

  // Listens for audio session creation notifications from the OS.
  std::unique_ptr<AudioSessionCreationObserverWin> session_creation_observer_;

  // Used to determine which applications we should duck.
  ShouldDuckProcessCallback should_duck_process_callback_;

  // Stores the applications we're currently ducking along with their original
  // volume so we can restore it when we stop ducking.
  std::map<std::wstring, float> ducked_applications_;

  // The AudioSession manager for the device that we're currently ducking. We
  // need to keep a connection to this alive so we can receive notifications of
  // new audio sessions.
  Microsoft::WRL::ComPtr<IAudioSessionManager2> ducked_audio_session_manager_;

  std::unique_ptr<AudioDeviceListenerWin> output_device_listener_;

  base::WeakPtrFactory<AudioDuckerWin> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_AUDIO_WIN_AUDIO_DUCKER_WIN_H_
