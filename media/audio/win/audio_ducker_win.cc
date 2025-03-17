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
#include "base/win/scoped_co_mem.h"
#include "media/base/media_switches.h"

using Microsoft::WRL::ComPtr;

namespace {

// Runs the given callback with each audio session on the default audio device.
// Returns `true` if it is able to iterate over all sessions with no failures
// reported by the Windows APIs.
bool ForEachAudioSession(
    base::RepeatingCallback<void(ComPtr<IAudioSessionControl2>& session)>
        callback) {
  ComPtr<IMMDeviceEnumerator> device_enumerator;
  HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                                CLSCTX_ALL, IID_PPV_ARGS(&device_enumerator));
  if (!SUCCEEDED(hr)) {
    return false;
  }
  ComPtr<IMMDevice> device;
  hr = device_enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
  if (!SUCCEEDED(hr)) {
    return false;
  }
  ComPtr<IAudioSessionManager2> audio_session_manager;
  hr = device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr,
                        &audio_session_manager);
  if (!SUCCEEDED(hr)) {
    return false;
  }
  ComPtr<IAudioSessionEnumerator> session_enumerator;
  hr = audio_session_manager->GetSessionEnumerator(&session_enumerator);
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

AudioDuckerWin::AudioDuckerWin(ShouldDuckProcessCallback callback)
    : should_duck_process_callback_(callback) {}

AudioDuckerWin::~AudioDuckerWin() {
  StopDuckingOtherWindowsApplications();
}

void AudioDuckerWin::StartDuckingOtherWindowsApplications() {
  // `base::Unretained()` is safe here because this callback is called
  // synchronously.
  ForEachAudioSession(
      base::BindRepeating(&AudioDuckerWin::StartDuckingAudioSessionIfNecessary,
                          base::Unretained(this)));
}

void AudioDuckerWin::StopDuckingOtherWindowsApplications() {
  if (ducked_applications_.empty()) {
    return;
  }

  // `base::Unretained()` is safe here because this callback is called
  // synchronously.
  bool no_errors = ForEachAudioSession(
      base::BindRepeating(&AudioDuckerWin::StopDuckingAudioSessionIfNecessary,
                          base::Unretained(this)));

  base::UmaHistogramBoolean("Media.AudioDuckerWin.UnduckSessionIterationResult",
                            no_errors);

  ducked_applications_.clear();
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
