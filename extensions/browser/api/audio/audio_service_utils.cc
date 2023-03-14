// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/audio/audio_service_utils.h"

#include "base/ranges/algorithm.h"

namespace extensions {

api::audio::StreamType ConvertStreamTypeFromMojom(
    crosapi::mojom::StreamType type) {
  switch (type) {
    case crosapi::mojom::StreamType::kNone:
      return api::audio::STREAM_TYPE_NONE;
    case crosapi::mojom::StreamType::kInput:
      return api::audio::STREAM_TYPE_INPUT;
    case crosapi::mojom::StreamType::kOutput:
      return api::audio::STREAM_TYPE_OUTPUT;
  }
  NOTREACHED();
  return api::audio::STREAM_TYPE_NONE;
}

crosapi::mojom::StreamType ConvertStreamTypeToMojom(
    api::audio::StreamType type) {
  switch (type) {
    case api::audio::STREAM_TYPE_NONE:
      return crosapi::mojom::StreamType::kNone;
    case api::audio::STREAM_TYPE_INPUT:
      return crosapi::mojom::StreamType::kInput;
    case api::audio::STREAM_TYPE_OUTPUT:
      return crosapi::mojom::StreamType::kOutput;
  }
  NOTREACHED();
  return crosapi::mojom::StreamType::kNone;
}

api::audio::DeviceType ConvertDeviceTypeFromMojom(
    crosapi::mojom::DeviceType type) {
  switch (type) {
    case crosapi::mojom::DeviceType::kNone:
      return api::audio::DeviceType::DEVICE_TYPE_NONE;
    case crosapi::mojom::DeviceType::kHeadphone:
      return api::audio::DeviceType::DEVICE_TYPE_HEADPHONE;
    case crosapi::mojom::DeviceType::kMic:
      return api::audio::DeviceType::DEVICE_TYPE_MIC;
    case crosapi::mojom::DeviceType::kUsb:
      return api::audio::DeviceType::DEVICE_TYPE_USB;
    case crosapi::mojom::DeviceType::kBluetooth:
      return api::audio::DeviceType::DEVICE_TYPE_BLUETOOTH;
    case crosapi::mojom::DeviceType::kHdmi:
      return api::audio::DeviceType::DEVICE_TYPE_HDMI;
    case crosapi::mojom::DeviceType::kInternalSpeaker:
      return api::audio::DeviceType::DEVICE_TYPE_INTERNAL_SPEAKER;
    case crosapi::mojom::DeviceType::kInternalMic:
      return api::audio::DeviceType::DEVICE_TYPE_INTERNAL_MIC;
    case crosapi::mojom::DeviceType::kFrontMic:
      return api::audio::DeviceType::DEVICE_TYPE_FRONT_MIC;
    case crosapi::mojom::DeviceType::kRearMic:
      return api::audio::DeviceType::DEVICE_TYPE_REAR_MIC;
    case crosapi::mojom::DeviceType::kKeyboardMic:
      return api::audio::DeviceType::DEVICE_TYPE_KEYBOARD_MIC;
    case crosapi::mojom::DeviceType::kHotword:
      return api::audio::DeviceType::DEVICE_TYPE_HOTWORD;
    case crosapi::mojom::DeviceType::kLineout:
      return api::audio::DeviceType::DEVICE_TYPE_LINEOUT;
    case crosapi::mojom::DeviceType::kPostMixLoopback:
      return api::audio::DeviceType::DEVICE_TYPE_POST_MIX_LOOPBACK;
    case crosapi::mojom::DeviceType::kPostDspLoopback:
      return api::audio::DeviceType::DEVICE_TYPE_POST_DSP_LOOPBACK;
    case crosapi::mojom::DeviceType::kAlsaLoopback:
      return api::audio::DeviceType::DEVICE_TYPE_ALSA_LOOPBACK;
    case crosapi::mojom::DeviceType::kOther:
      return api::audio::DeviceType::DEVICE_TYPE_OTHER;
  }
  NOTREACHED();
  return api::audio::DeviceType::DEVICE_TYPE_NONE;
}

crosapi::mojom::DeviceType ConvertDeviceTypeToMojom(
    api::audio::DeviceType type) {
  switch (type) {
    case api::audio::DeviceType::DEVICE_TYPE_NONE:
      return crosapi::mojom::DeviceType::kNone;
    case api::audio::DeviceType::DEVICE_TYPE_HEADPHONE:
      return crosapi::mojom::DeviceType::kHeadphone;
    case api::audio::DeviceType::DEVICE_TYPE_MIC:
      return crosapi::mojom::DeviceType::kMic;
    case api::audio::DeviceType::DEVICE_TYPE_USB:
      return crosapi::mojom::DeviceType::kUsb;
    case api::audio::DeviceType::DEVICE_TYPE_BLUETOOTH:
      return crosapi::mojom::DeviceType::kBluetooth;
    case api::audio::DeviceType::DEVICE_TYPE_HDMI:
      return crosapi::mojom::DeviceType::kHdmi;
    case api::audio::DeviceType::DEVICE_TYPE_INTERNAL_SPEAKER:
      return crosapi::mojom::DeviceType::kInternalSpeaker;
    case api::audio::DeviceType::DEVICE_TYPE_INTERNAL_MIC:
      return crosapi::mojom::DeviceType::kInternalMic;
    case api::audio::DeviceType::DEVICE_TYPE_FRONT_MIC:
      return crosapi::mojom::DeviceType::kFrontMic;
    case api::audio::DeviceType::DEVICE_TYPE_REAR_MIC:
      return crosapi::mojom::DeviceType::kRearMic;
    case api::audio::DeviceType::DEVICE_TYPE_KEYBOARD_MIC:
      return crosapi::mojom::DeviceType::kKeyboardMic;
    case api::audio::DeviceType::DEVICE_TYPE_HOTWORD:
      return crosapi::mojom::DeviceType::kHotword;
    case api::audio::DeviceType::DEVICE_TYPE_LINEOUT:
      return crosapi::mojom::DeviceType::kLineout;
    case api::audio::DeviceType::DEVICE_TYPE_POST_MIX_LOOPBACK:
      return crosapi::mojom::DeviceType::kPostMixLoopback;
    case api::audio::DeviceType::DEVICE_TYPE_POST_DSP_LOOPBACK:
      return crosapi::mojom::DeviceType::kPostDspLoopback;
    case api::audio::DeviceType::DEVICE_TYPE_ALSA_LOOPBACK:
      return crosapi::mojom::DeviceType::kAlsaLoopback;
    case api::audio::DeviceType::DEVICE_TYPE_OTHER:
      return crosapi::mojom::DeviceType::kOther;
  }
  NOTREACHED();
  return crosapi::mojom::DeviceType::kNone;
}

std::unique_ptr<api::audio::DeviceFilter> ConvertDeviceFilterFromMojom(
    const crosapi::mojom::DeviceFilterPtr& filter) {
  if (!filter) {
    return nullptr;
  }

  auto result = std::make_unique<api::audio::DeviceFilter>();
  switch (filter->includedActiveState) {
    case crosapi::mojom::DeviceFilter::ActiveState::kUnset:
      result->is_active = absl::nullopt;
      break;
    case crosapi::mojom::DeviceFilter::ActiveState::kInactive:
      result->is_active = false;
      break;
    case crosapi::mojom::DeviceFilter::ActiveState::kActive:
      result->is_active = true;
      break;
  }

  if (filter->includedStreamTypes) {
    result->stream_types.emplace(filter->includedStreamTypes->size());
    base::ranges::transform(*filter->includedStreamTypes,
                            result->stream_types->begin(),
                            ConvertStreamTypeFromMojom);
  }

  return result;
}

crosapi::mojom::DeviceFilterPtr ConvertDeviceFilterToMojom(
    const api::audio::DeviceFilter* filter) {
  auto result = crosapi::mojom::DeviceFilter::New();

  if (!filter) {
    return result;
  }

  if (filter->is_active) {
    result->includedActiveState =
        *(filter->is_active)
            ? crosapi::mojom::DeviceFilter::ActiveState::kActive
            : crosapi::mojom::DeviceFilter::ActiveState::kInactive;
  } else {
    result->includedActiveState =
        crosapi::mojom::DeviceFilter::ActiveState::kUnset;
  }

  if (filter->stream_types) {
    result->includedStreamTypes =
        std::vector<crosapi::mojom::StreamType>(filter->stream_types->size());
    base::ranges::transform(*filter->stream_types,
                            result->includedStreamTypes->begin(),
                            ConvertStreamTypeToMojom);
  }
  return result;
}

api::audio::AudioDeviceInfo ConvertAudioDeviceInfoFromMojom(
    const crosapi::mojom::AudioDeviceInfoPtr& info) {
  DCHECK(info);
  api::audio::AudioDeviceInfo result;
  result.id = info->id;
  result.stream_type = ConvertStreamTypeFromMojom(info->streamType);
  result.device_type = ConvertDeviceTypeFromMojom(info->deviceType);
  result.display_name = info->displayName;
  result.device_name = info->deviceName;
  result.is_active = info->isActive;
  result.level = info->level;
  result.stable_device_id = info->stableDeviceId;
  return result;
}

crosapi::mojom::AudioDeviceInfoPtr ConvertAudioDeviceInfoToMojom(
    const api::audio::AudioDeviceInfo& info) {
  auto result = crosapi::mojom::AudioDeviceInfo::New();
  result->deviceName = info.device_name;
  result->deviceType = ConvertDeviceTypeToMojom(info.device_type);
  result->displayName = info.display_name;
  result->id = info.id;
  result->isActive = info.is_active;
  result->level = info.level;
  if (info.stable_device_id) {
    result->stableDeviceId = *(info.stable_device_id);
  }
  result->streamType = ConvertStreamTypeToMojom(info.stream_type);
  return result;
}

crosapi::mojom::DeviceIdListsPtr ConvertDeviceIdListsToMojom(
    const DeviceIdList* input_devices,
    const DeviceIdList* output_devives) {
  auto result = crosapi::mojom::DeviceIdLists::New();
  if (input_devices) {
    result->inputs = *input_devices;
  }
  if (output_devives) {
    result->outputs = *output_devives;
  }
  return result;
}

}  // namespace extensions
