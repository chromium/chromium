// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/win/audio_ducker_win.h"

#include <MMDeviceAPI.h>

#include <audioclient.h>
#include <audiopolicy.h>
#include <wrl/client.h>

#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/bind_post_task.h"
#include "base/win/scoped_co_mem.h"
#include "media/audio/win/audio_device_listener_win.h"
#include "media/audio/win/audio_session_creation_observer_win.h"
#include "media/audio/win/core_audio_util_win.h"
#include "media/base/media_switches.h"

using Microsoft::WRL::ComPtr;

namespace {

// Acquires a ComPtr to the AudioSession manager for the default output device
// and sets `out_manager` to the found manager. Returns `true` if no errors are
// received from the system APIs. When this function returns `false`,
// `out_manager` remains unchanged.
bool GetAudioSessionManager(ComPtr<IAudioSessionManager2>& out_manager) {
  ComPtr<IMMDeviceEnumerator> device_enumerator =
      media::CoreAudioUtil::CreateDeviceEnumerator();
  if (!device_enumerator) {
    return false;
  }
  ComPtr<IMMDevice> device;
  HRESULT hr =
      device_enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
  if (!SUCCEEDED(hr)) {
    return false;
  }
  ComPtr<IAudioSessionManager2> audio_session_manager;
  hr = device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr,
                        &audio_session_manager);
  if (!SUCCEEDED(hr)) {
    return false;
  }
  out_manager = audio_session_manager;
  return true;
}

// Runs the given callback with each audio session managed by the given
// `audio_session_manager`. Returns `true` if it is able to iterate over all
// sessions with no failures reported by the Windows APIs.
bool ForEachAudioSession(
    ComPtr<IAudioSessionManager2>& audio_session_manager,
    base::RepeatingCallback<void(ComPtr<IAudioSessionControl2>& session)>
        callback) {
  ComPtr<IAudioSessionEnumerator> session_enumerator;
  HRESULT hr = audio_session_manager->GetSessionEnumerator(&session_enumerator);
  if (!SUCCEEDED(hr)) {
    return false;
  }
  int session_count;
  hr = session_enumerator->GetCount(&session_count);
  if (!SUCCEEDED(hr)) {
    return false;
  }

  bool no_errors = true;
  for (int i = 0; i < session_count; i++) {
    ComPtr<IAudioSessionControl> session_control;
    hr = session_enumerator->GetSession(i, &session_control);
    if (!SUCCEEDED(hr)) {
      no_errors = false;
      continue;
    }
    ComPtr<IAudioSessionControl2> session_control_2;
    hr = session_control->QueryInterface(IID_PPV_ARGS(&session_control_2));
    if (!SUCCEEDED(hr)) {
      no_errors = false;
      continue;
    }
    callback.Run(session_control_2);
  }

  return no_errors;
}

float GetAttenuationMultiplier() {
  return 1.0 -
         (std::clamp(media::kAudioDuckingAttenuation.Get(), 0, 100) / 100.0);
}

void RecordSessionUnduckResult(bool success) {
  base::UmaHistogramBoolean("Media.AudioDuckerWin.UnduckSessionResult",
                            success);
}

}  // namespace

namespace media {

AudioDuckerWin::AudioDuckerWin(ShouldDuckProcessCallback callback)
    : should_duck_process_callback_(callback) {}

AudioDuckerWin::~AudioDuckerWin() {
  StopDuckingOtherWindowsApplications();
}

void AudioDuckerWin::StartDuckingOtherWindowsApplications() {
  if (ducked_audio_session_manager_) {
    // If `ducked_audio_session_manager_` exists then we're already ducking
    // other applications, so nothing needs to be done.
    return;
  }

  if (!GetAudioSessionManager(ducked_audio_session_manager_)) {
    return;
  }

  if (!session_creation_observer_) {
    session_creation_observer_ =
        std::make_unique<AudioSessionCreationObserverWin>(
            base::BindPostTaskToCurrentDefault(base::BindRepeating(
                &AudioDuckerWin::DuckNewAudioSessionsIfNecessary,
                weak_factory_.GetWeakPtr())));
  }
  ducked_audio_session_manager_->RegisterSessionNotification(
      session_creation_observer_.get());

  // `base::Unretained()` is safe here because this callback is called
  // synchronously.
  ForEachAudioSession(
      ducked_audio_session_manager_,
      base::BindRepeating(&AudioDuckerWin::StartDuckingAudioSessionIfNecessary,
                          base::Unretained(this)));

  if (!output_device_listener_) {
    // Need to bind as a posted task since it will be called from a system-level
    // multimedia thread and we need to post back to our COM-enabled thread.
    output_device_listener_ = std::make_unique<AudioDeviceListenerWin>(
        base::BindPostTaskToCurrentDefault(
            base::BindRepeating(&AudioDuckerWin::OnDefaultDeviceChanged,
                                weak_factory_.GetWeakPtr())));
  }
}

void AudioDuckerWin::StopDuckingOtherWindowsApplications() {
  if (ducked_applications_.empty()) {
    if (ducked_audio_session_manager_) {
      CHECK(session_creation_observer_);
      ducked_audio_session_manager_->UnregisterSessionNotification(
          session_creation_observer_.get());
      ducked_audio_session_manager_.Reset();
    }
    return;
  }

  // If we're currently ducking applications, then we must have the associated
  // audio session manager.
  CHECK(ducked_audio_session_manager_);
  CHECK(session_creation_observer_);

  // `base::Unretained()` is safe here because this callback is called
  // synchronously.
  bool no_errors = ForEachAudioSession(
      ducked_audio_session_manager_,
      base::BindRepeating(&AudioDuckerWin::StopDuckingAudioSessionIfNecessary,
                          base::Unretained(this)));

  base::UmaHistogramBoolean("Media.AudioDuckerWin.UnduckSessionIterationResult",
                            no_errors);

  ducked_applications_.clear();
  ducked_audio_session_manager_->UnregisterSessionNotification(
      session_creation_observer_.get());
  ducked_audio_session_manager_.Reset();
}

void AudioDuckerWin::StartDuckingAudioSessionIfNecessary(
    ComPtr<IAudioSessionControl2>& session) {
  base::ProcessId process_id = 0;
  HRESULT hr = session->GetProcessId(&process_id);
  if (!SUCCEEDED(hr)) {
    return;
  }
  if (!should_duck_process_callback_.Run(process_id)) {
    return;
  }
  base::win::ScopedCoMem<wchar_t> session_id;
  hr = session->GetSessionInstanceIdentifier(&session_id);
  if (!SUCCEEDED(hr)) {
    return;
  }

  // If we're already ducking this audio session, don't reduck it. We can get
  // here when we're ducking applications due to a new session being created.
  auto iter = ducked_applications_.find(session_id.get());
  if (iter != ducked_applications_.end()) {
    return;
  }

  ComPtr<ISimpleAudioVolume> simple_audio_volume;
  hr = session->QueryInterface(IID_PPV_ARGS(&simple_audio_volume));
  if (!SUCCEEDED(hr)) {
    return;
  }
  float current_volume = 0;
  hr = simple_audio_volume->GetMasterVolume(&current_volume);
  if (!SUCCEEDED(hr)) {
    return;
  }
  hr = simple_audio_volume->SetMasterVolume(
      current_volume * GetAttenuationMultiplier(), nullptr);
  if (!SUCCEEDED(hr)) {
    return;
  }
  ducked_applications_.insert({session_id.get(), current_volume});
}

void AudioDuckerWin::StopDuckingAudioSessionIfNecessary(
    ComPtr<IAudioSessionControl2>& session) {
  base::win::ScopedCoMem<wchar_t> session_id;
  HRESULT hr = session->GetSessionInstanceIdentifier(&session_id);
  if (!SUCCEEDED(hr)) {
    RecordSessionUnduckResult(false);
    return;
  }
  auto iter = ducked_applications_.find(session_id.get());
  if (iter == ducked_applications_.end()) {
    return;
  }
  ComPtr<ISimpleAudioVolume> simple_audio_volume;
  hr = session->QueryInterface(IID_PPV_ARGS(&simple_audio_volume));
  if (!SUCCEEDED(hr)) {
    RecordSessionUnduckResult(false);
    return;
  }
  hr = simple_audio_volume->SetMasterVolume(iter->second, nullptr);
  RecordSessionUnduckResult(SUCCEEDED(hr));
}

void AudioDuckerWin::OnDefaultDeviceChanged() {
  // If we're not currently ducking, then do nothing.
  if (!ducked_audio_session_manager_) {
    return;
  }

  // Otherwise, unduck the current sessions (as they're all associated with the
  // previous default device) and start ducking sessions for the new device.
  StopDuckingOtherWindowsApplications();
  StartDuckingOtherWindowsApplications();
}

void AudioDuckerWin::DuckNewAudioSessionsIfNecessary() {
  // If we're not currently ducking, then do nothing.
  if (!ducked_audio_session_manager_) {
    return;
  }

  // Otherwise, loop through all audio sessions and duck them. Note that
  // `StartDuckingAudioSessionIfNecessary()` will not re-duck already ducked
  // sessions, so it's safe to loop through them all to catch new ones.
  // `base::Unretained()` is safe here because this callback is called
  // synchronously.
  ForEachAudioSession(
      ducked_audio_session_manager_,
      base::BindRepeating(&AudioDuckerWin::StartDuckingAudioSessionIfNecessary,
                          base::Unretained(this)));
}

}  // namespace media
