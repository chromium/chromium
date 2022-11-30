// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_AUDIO_AUDIO_SERVICE_UTILS_H_
#define EXTENSIONS_BROWSER_API_AUDIO_AUDIO_SERVICE_UTILS_H_

#include "chromeos/crosapi/mojom/audio_service.mojom.h"
#include "extensions/common/api/audio.h"

namespace extensions {

api::audio::StreamType ConvertStreamTypeFromMojom(
    crosapi::mojom::StreamType type);
crosapi::mojom::StreamType ConvertStreamTypeToMojom(
    api::audio::StreamType type);

api::audio::DeviceType ConvertDeviceTypeFromMojom(
    crosapi::mojom::DeviceType type);
crosapi::mojom::DeviceType ConvertDeviceTypeToMojom(
    api::audio::DeviceType type);

std::unique_ptr<api::audio::DeviceFilter> ConvertDeviceFilterFromMojom(
    const crosapi::mojom::DeviceFilterPtr& filter);
crosapi::mojom::DeviceFilterPtr ConvertDeviceFilterToMojom(
    const api::audio::DeviceFilter* filter);

api::audio::AudioDeviceInfo ConvertAudioDeviceInfoFromMojom(
    const crosapi::mojom::AudioDeviceInfoPtr& info);
crosapi::mojom::AudioDeviceInfoPtr ConvertAudioDeviceInfoToMojom(
    const api::audio::AudioDeviceInfo& info);

using DeviceIdList = std::vector<std::string>;
crosapi::mojom::DeviceIdListsPtr ConvertDeviceIdListsToMojom(
    const DeviceIdList* input_devices,
    const DeviceIdList* output_devives);

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_AUDIO_AUDIO_SERVICE_UTILS_H_
