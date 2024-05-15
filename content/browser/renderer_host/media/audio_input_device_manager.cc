// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_input_device_manager.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "media/audio/audio_input_ipc.h"
#include "media/audio/audio_system.h"
#include "media/base/audio_parameters.h"
#include "media/base/channel_layout.h"
#include "media/base/media_switches.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-shared.h"

namespace content {

namespace {

void SendAudioLogMessage(const std::string& message) {
  MediaStreamManager::SendMessageToNativeLog("AIDM::" + message);
}

const char* TypeToString(blink::mojom::MediaStreamType type) {
  DCHECK(blink::IsAudioInputMediaType(type));
  switch (type) {
    case blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE:
      return "DEVICE_AUDIO_CAPTURE";
    case blink::mojom::MediaStreamType::GUM_TAB_AUDIO_CAPTURE:
      return "GUM_TAB_AUDIO_CAPTURE";
    case blink::mojom::MediaStreamType::GUM_DESKTOP_AUDIO_CAPTURE:
      return "GUM_DESKTOP_AUDIO_CAPTURE";
    case blink::mojom::MediaStreamType::DISPLAY_AUDIO_CAPTURE:
      return "DISPLAY_AUDIO_CAPTURE";
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return "INVALID";
}

std::string GetOpenLogString(const base::UnguessableToken& session_id,
                             const blink::MediaStreamDevice& device) {
  std::string str = base::StringPrintf("Open({session_id=%s}, ",
                                       session_id.ToString().c_str());
  base::StringAppendF(&str, "{device=[type: %s, ", TypeToString(device.type));
  base::StringAppendF(&str, "id: %s, ", device.id.c_str());
  if (device.group_id.has_value()) {
    base::StringAppendF(&str, "group_id: %s, ",
                        device.group_id.value().c_str());
  }
  if (device.matched_output_device_id.has_value()) {
    base::StringAppendF(&str, "matched_output_device_id: %s, ",
                        device.matched_output_device_id.value().c_str());
  }
  base::StringAppendF(&str, "name: %s", device.name.c_str());
  if (blink::IsAudioInputMediaType(device.type)) {
    base::StringAppendF(&str, ", parameters: [%s",
                        device.input.AsHumanReadableString().c_str());
  }
  str += "]]})";
  return str;
}

}  // namespace

AudioInputDeviceManager::AudioInputDeviceManager(
    media::AudioSystem* audio_system)
    : audio_system_(audio_system) {}

AudioInputDeviceManager::~AudioInputDeviceManager() {
}

const blink::MediaStreamDevice* AudioInputDeviceManager::GetOpenedDeviceById(
    const base::UnguessableToken& session_id) {
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

base::UnguessableToken AudioInputDeviceManager::Open(
    const blink::MediaStreamDevice& device) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Generate a new id for this device.
  auto session_id = base::UnguessableToken::Create();
  SendAudioLogMessage(GetOpenLogString(session_id, device));

  // base::Unretained(this) is safe, because AudioInputDeviceManager is
  // destroyed not earlier than on the IO message loop destruction.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseFakeDeviceForMediaStream)) {
    audio_system_->GetAssociatedOutputDeviceID(
        device.id, base::BindOnce(&AudioInputDeviceManager::OpenedOnIOThread,
                                  base::Unretained(this), session_id, device,
                                  std::optional<media::AudioParameters>()));
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
                                  base::Unretained(this), session_id, device));
  }

  return session_id;
}

void AudioInputDeviceManager::Close(const base::UnguessableToken& session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  SendAudioLogMessage("Close({session_id=" + session_id.ToString() + "})");
  auto device = GetDevice(session_id);
  if (device == devices_.end())
    return;
  const blink::mojom::MediaStreamType stream_type = device->type;
  devices_.erase(device);

  // Post a callback through the listener on IO thread since
  // MediaStreamManager is expecting the callback asynchronously.
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&AudioInputDeviceManager::ClosedOnIOThread,
                                this, stream_type, session_id));
}

void AudioInputDeviceManager::OpenedOnIOThread(
    const base::UnguessableToken& session_id,
    const blink::MediaStreamDevice& device,
    const std::optional<media::AudioParameters>& input_params,
    const std::optional<std::string>& matched_output_device_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(GetDevice(session_id) == devices_.end());
  DCHECK(!input_params || input_params->IsValid());
  DCHECK(!matched_output_device_id || !matched_output_device_id->empty());

  SendAudioLogMessage("Opened({session_id=" + session_id.ToString() + "})");
  blink::MediaStreamDevice media_stream_device(device.type, device.id,
                                               device.name);
  media_stream_device.set_session_id(session_id);
  media_stream_device.input =
      input_params.value_or(media::AudioParameters::UnavailableDeviceParams());
  media_stream_device.matched_output_device_id = matched_output_device_id;

  DCHECK(media_stream_device.input.IsValid());

  devices_.push_back(media_stream_device);

  for (auto& listener : listeners_)
    listener.Opened(media_stream_device.type, session_id);
}

void AudioInputDeviceManager::ClosedOnIOThread(
    blink::mojom::MediaStreamType stream_type,
    const base::UnguessableToken& session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  SendAudioLogMessage("Closed({session_id=" + session_id.ToString() + "})");
  for (auto& listener : listeners_)
    listener.Closed(stream_type, session_id);
}

blink::MediaStreamDevices::iterator AudioInputDeviceManager::GetDevice(
    const base::UnguessableToken& session_id) {
  for (auto it = devices_.begin(); it != devices_.end(); ++it) {
    if (it->session_id() == session_id)
      return it;
  }

  return devices_.end();
}

}  // namespace content
