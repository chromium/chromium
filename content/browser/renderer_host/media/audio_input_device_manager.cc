// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_input_device_manager.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/media_stream_request.h"
#include "media/audio/audio_input_ipc.h"
#include "media/audio/audio_system.h"
#include "media/base/audio_parameters.h"
#include "media/base/channel_layout.h"
#include "media/base/media_switches.h"

#if defined(OS_CHROMEOS)
#include "chromeos/audio/cras_audio_handler.h"
#endif

namespace content {

const int AudioInputDeviceManager::kFakeOpenSessionId = 1;

namespace {

// Starting id for the first capture session.
const int kFirstSessionId = AudioInputDeviceManager::kFakeOpenSessionId + 1;

#if defined(OS_CHROMEOS)
void SetKeyboardMicStreamActiveOnUIThread(bool active) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  chromeos::CrasAudioHandler::Get()->SetKeyboardMicActive(active);
}
#endif

}  // namespace

AudioInputDeviceManager::AudioInputDeviceManager(
    media::AudioSystem* audio_system)
    : next_capture_session_id_(kFirstSessionId),
#if defined(OS_CHROMEOS)
      keyboard_mic_streams_count_(0),
#endif
      audio_system_(audio_system) {
}

AudioInputDeviceManager::~AudioInputDeviceManager() {
}

const MediaStreamDevice* AudioInputDeviceManager::GetOpenedDeviceById(
    int session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  auto device = GetDevice(session_id);
  if (device == devices_.end())
    return nullptr;

  return &(*device);
}

void AudioInputDeviceManager::RegisterListener(
    MediaStreamProviderListener* listener) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(listener);
  listeners_.AddObserver(listener);
}

void AudioInputDeviceManager::UnregisterListener(
    MediaStreamProviderListener* listener) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(listener);
  listeners_.RemoveObserver(listener);
}

int AudioInputDeviceManager::Open(const MediaStreamDevice& device) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Generate a new id for this device.
  int session_id = next_capture_session_id_++;

  // base::Unretained(this) is safe, because AudioInputDeviceManager is
  // destroyed not earlier than on the IO message loop destruction.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseFakeDeviceForMediaStream)) {
    audio_system_->GetAssociatedOutputDeviceID(
        device.id, base::BindOnce(&AudioInputDeviceManager::OpenedOnIOThread,
                                  base::Unretained(this), session_id, device,
                                  base::TimeTicks::Now(),
                                  base::Optional<media::AudioParameters>()));
  } else {
    // TODO(tommi): As is, we hit this code path when device.type is
    // MEDIA_GUM_TAB_AUDIO_CAPTURE and the device id is not a device that
    // the AudioManager can know about. This currently does not fail because
    // the implementation of GetInputStreamParameters returns valid parameters
    // by default for invalid devices. That behavior is problematic because it
    // causes other parts of the code to attempt to open truly invalid or
    // missing devices and falling back on alternate devices (and likely fail
    // twice in a row). Tab audio capture should not pass through here and
    // GetInputStreamParameters should return invalid parameters for invalid
    // devices.

    audio_system_->GetInputDeviceInfo(
        device.id, base::BindOnce(&AudioInputDeviceManager::OpenedOnIOThread,
                                  base::Unretained(this), session_id, device,
                                  base::TimeTicks::Now()));
  }

  return session_id;
}

void AudioInputDeviceManager::Close(int session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  auto device = GetDevice(session_id);
  if (device == devices_.end())
    return;
  const MediaStreamType stream_type = device->type;
  if (session_id != kFakeOpenSessionId)
    devices_.erase(device);

  // Post a callback through the listener on IO thread since
  // MediaStreamManager is expecting the callback asynchronously.
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&AudioInputDeviceManager::ClosedOnIOThread, this,
                     stream_type, session_id));
}

#if defined(OS_CHROMEOS)
AudioInputDeviceManager::KeyboardMicRegistration::KeyboardMicRegistration(
    KeyboardMicRegistration&& other)
    : shared_registration_count_(other.shared_registration_count_) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  other.shared_registration_count_ = nullptr;
}

AudioInputDeviceManager::KeyboardMicRegistration::~KeyboardMicRegistration() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DeregisterIfNeeded();
}

AudioInputDeviceManager::KeyboardMicRegistration::KeyboardMicRegistration(
    int* shared_registration_count)
    : shared_registration_count_(shared_registration_count) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void AudioInputDeviceManager::KeyboardMicRegistration::DeregisterIfNeeded() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (shared_registration_count_) {
    --*shared_registration_count_;
    DCHECK_GE(*shared_registration_count_, 0);
    if (*shared_registration_count_ == 0) {
      base::PostTaskWithTraits(
          FROM_HERE, {BrowserThread::UI},
          base::BindOnce(&SetKeyboardMicStreamActiveOnUIThread, false));
    }
  }

  // Since we removed our registration, we unset the counter pointer to
  // indicate this.
  shared_registration_count_ = nullptr;
}

void AudioInputDeviceManager::RegisterKeyboardMicStream(
    base::OnceCallback<void(KeyboardMicRegistration)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  ++keyboard_mic_streams_count_;
  if (keyboard_mic_streams_count_ == 1) {
    base::PostTaskWithTraitsAndReply(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&SetKeyboardMicStreamActiveOnUIThread, true),
        base::BindOnce(std::move(callback),
                       KeyboardMicRegistration(&keyboard_mic_streams_count_)));
  } else {
    std::move(callback).Run(
        KeyboardMicRegistration(&keyboard_mic_streams_count_));
  }
}
#endif

void AudioInputDeviceManager::OpenedOnIOThread(
    int session_id,
    const MediaStreamDevice& device,
    base::TimeTicks start_time,
    const base::Optional<media::AudioParameters>& input_params,
    const base::Optional<std::string>& matched_output_device_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(GetDevice(session_id) == devices_.end());
  DCHECK(!input_params || input_params->IsValid());
  DCHECK(!matched_output_device_id || !matched_output_device_id->empty());

  UMA_HISTOGRAM_TIMES("Media.AudioInputDeviceManager.OpenOnDeviceThreadTime",
                      base::TimeTicks::Now() - start_time);

  MediaStreamDevice media_stream_device(device.type, device.id, device.name);
  media_stream_device.session_id = session_id;
  media_stream_device.input =
      input_params.value_or(media::AudioParameters::UnavailableDeviceParams());
  media_stream_device.matched_output_device_id = matched_output_device_id;

  DCHECK(media_stream_device.input.IsValid());

  devices_.push_back(media_stream_device);

  for (auto& listener : listeners_)
    listener.Opened(media_stream_device.type, session_id);
}

void AudioInputDeviceManager::ClosedOnIOThread(MediaStreamType stream_type,
                                               int session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  for (auto& listener : listeners_)
    listener.Closed(stream_type, session_id);
}

MediaStreamDevices::iterator AudioInputDeviceManager::GetDevice(
    int session_id) {
  for (auto it = devices_.begin(); it != devices_.end(); ++it) {
    if (it->session_id == session_id)
      return it;
  }

  return devices_.end();
}

}  // namespace content
