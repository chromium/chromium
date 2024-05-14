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
      return api::audio::StreamType::kNone;
    case crosapi::mojom::StreamType::kInput:
      return api::audio::StreamType::kInput;
    case crosapi::mojom::StreamType::kOutput:
      return api::audio::StreamType::kOutput;
  }
  NOTREACHED_IN_MIGRATION();
  return api::audio::StreamType::kNone;
}

crosapi::mojom::StreamType ConvertStreamTypeToMojom(
    api::audio::StreamType type) {
  switch (type) {
    case api::audio::StreamType::kNone:
      return crosapi::mojom::StreamType::kNone;
    case api::audio::StreamType::kInput:
      return crosapi::mojom::StreamType::kInput;
    case api::audio::StreamType::kOutput:
      return crosapi::mojom::StreamType::kOutput;
  }
  NOTREACHED_IN_MIGRATION();
  return crosapi::mojom::StreamType::kNone;
}

api::audio::DeviceType ConvertDeviceTypeFromMojom(
    crosapi::mojom::DeviceType type) {
  switch (type) {
    case crosapi::mojom::DeviceType::kNone:
      return api::audio::DeviceType::kNone;
    case crosapi::mojom::DeviceType::kHeadphone:
      return api::audio::DeviceType::kHeadphone;
    case crosapi::mojom::DeviceType::kMic:
      return api::audio::DeviceType::kMic;
    case crosapi::mojom::DeviceType::kUsb:
      return api::audio::DeviceType::kUsb;
    case crosapi::mojom::DeviceType::kBluetooth:
      return api::audio::DeviceType::kBluetooth;
    case crosapi::mojom::DeviceType::kHdmi:
      return api::audio::DeviceType::kHdmi;
    case crosapi::mojom::DeviceType::kInternalSpeaker:
      return api::audio::DeviceType::kInternalSpeaker;
    case crosapi::mojom::DeviceType::kInternalMic:
      return api::audio::DeviceType::kInternalMic;
    case crosapi::mojom::DeviceType::kFrontMic:
      return api::audio::DeviceType::kFrontMic;
    case crosapi::mojom::DeviceType::kRearMic:
      return api::audio::DeviceType::kRearMic;
    case crosapi::mojom::DeviceType::kKeyboardMic:
      return api::audio::DeviceType::kKeyboardMic;
    case crosapi::mojom::DeviceType::kHotword:
      return api::audio::DeviceType::kHotword;
    case crosapi::mojom::DeviceType::kLineout:
      return api::audio::DeviceType::kLineout;
    case crosapi::mojom::DeviceType::kPostMixLoopback:
      return api::audio::DeviceType::kPostMixLoopback;
    case crosapi::mojom::DeviceType::kPostDspLoopback:
      return api::audio::DeviceType::kPostDspLoopback;
    case crosapi::mojom::DeviceType::kAlsaLoopback:
      return api::audio::DeviceType::kAlsaLoopback;
    case crosapi::mojom::DeviceType::kOther:
      return api::audio::DeviceType::kOther;
  }
  NOTREACHED_IN_MIGRATION();
  return api::audio::DeviceType::kNone;
}

crosapi::mojom::DeviceType ConvertDeviceTypeToMojom(
    api::audio::DeviceType type) {
  switch (type) {
    case api::audio::DeviceType::kNone:
      return crosapi::mojom::DeviceType::kNone;
    case api::audio::DeviceType::kHeadphone:
      return crosapi::mojom::DeviceType::kHeadphone;
    case api::audio::DeviceType::kMic:
      return crosapi::mojom::DeviceType::kMic;
    case api::audio::DeviceType::kUsb:
      return crosapi::mojom::DeviceType::kUsb;
    case api::audio::DeviceType::kBluetooth:
      return crosapi::mojom::DeviceType::kBluetooth;
    case api::audio::DeviceType::kHdmi:
      return crosapi::mojom::DeviceType::kHdmi;
    case api::audio::DeviceType::kInternalSpeaker:
      return crosapi::mojom::DeviceType::kInternalSpeaker;
    case api::audio::DeviceType::kInternalMic:
      return crosapi::mojom::DeviceType::kInternalMic;
    case api::audio::DeviceType::kFrontMic:
      return crosapi::mojom::DeviceType::kFrontMic;
    case api::audio::DeviceType::kRearMic:
      return crosapi::mojom::DeviceType::kRearMic;
    case api::audio::DeviceType::kKeyboardMic:
      return crosapi::mojom::DeviceType::kKeyboardMic;
    case api::audio::DeviceType::kHotword:
      return crosapi::mojom::DeviceType::kHotword;
    case api::audio::DeviceType::kLineout:
      return crosapi::mojom::DeviceType::kLineout;
    case api::audio::DeviceType::kPostMixLoopback:
      return crosapi::mojom::DeviceType::kPostMixLoopback;
    case api::audio::DeviceType::kPostDspLoopback:
      return crosapi::mojom::DeviceType::kPostDspLoopback;
    case api::audio::DeviceType::kAlsaLoopback:
      return crosapi::mojom::DeviceType::kAlsaLoopback;
    case api::audio::DeviceType::kOther:
      return crosapi::mojom::DeviceType::kOther;
  }
  NOTREACHED_IN_MIGRATION();
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
      result->is_active = std::nullopt;
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
