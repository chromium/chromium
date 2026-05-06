// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/win/device_enumeration_win.h"

#include <objbase.h>

#include <MMDeviceAPI.h>

#include <Functiondiscoverykeys_devpkey.h>
#include <devicetopology.h>
#include <mmsystem.h>
#include <stddef.h>
#include <wrl/client.h>

#include <string_view>

#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_propvariant.h"
#include "media/audio/win/audio_manager_win.h"
#include "media/audio/win/core_audio_util_win.h"

using base::win::ScopedCoMem;
using base::win::ScopedPropVariant;
using Microsoft::WRL::ComPtr;

// Taken from Mmddk.h.
#define DRV_RESERVED 0x0800
#define DRV_QUERYFUNCTIONINSTANCEID (DRV_RESERVED + 17)
#define DRV_QUERYFUNCTIONINSTANCEIDSIZE (DRV_RESERVED + 18)

namespace media {
namespace {

// Returns the device instance id for `audio_device`.  Returns empty string
// after a failure. Example output for a USB audio device would be:
//
// USB\VID_046D&PID_09A6&MI_02\6&318d810e&1&0002
//
// A Bluetooth audio device returns something like:
//
// BTHHFENUM\BthHFPAudio\8&39e29755&0&97
//
// Looks at the device topology to fetch the PKEY_Device_InstanceId of the
// associated physical audio device.
std::string GetDeviceInstanceId(IMMDevice* audio_device,
                                IMMDeviceEnumerator* enumerator) {
  ComPtr<IDeviceTopology> topology;
  ComPtr<IConnector> connector;
  ScopedCoMem<WCHAR> filter_id;
  if (FAILED(audio_device->Activate(__uuidof(IDeviceTopology), CLSCTX_ALL, NULL,
                                    &topology)) ||
      // For our purposes checking the first connected device should be enough
      // and if there are cases where there are more than one device connected
      // we're not sure how to handle that anyway. So we pass 0.
      FAILED(topology->GetConnector(0, &connector)) ||
      FAILED(connector->GetDeviceIdConnectedTo(&filter_id))) {
    return std::string();
  }

  // Now look at the properties of the connected device node and fetch the
  // instance id (PKEY_Device_InstanceId) of the device node.
  ComPtr<IMMDevice> device_node;
  ComPtr<IPropertyStore> properties;
  ScopedPropVariant instance_id;
  if (FAILED(enumerator->GetDevice(filter_id, &device_node)) ||
      FAILED(device_node->OpenPropertyStore(STGM_READ, &properties)) ||
      FAILED(properties->GetValue(PKEY_Device_InstanceId,
                                  instance_id.Receive())) ||
      instance_id.get().vt != VT_LPWSTR) {
    return std::string();
  }

  return base::WideToUTF8(instance_id.get().pwszVal);
}

// Converts a COM error into a human-readable string.
std::string ErrorToString(HRESULT hresult) {
  return CoreAudioUtil::ErrorToString(hresult);
}

}  // namespace

static bool GetDeviceNamesWinImpl(
    EDataFlow data_flow,
    AudioDeviceNames* device_names,
    const media::AudioManager::LogCallback& log_callback) {
  const char* func_name = __func__;
  auto send_log = [&](const std::string& message) {
    if (!log_callback.is_null()) {
      log_callback.Run(base::StrCat({func_name, message}));
    }
  };

  // It is assumed that this method is called from a COM thread, i.e.,
  // CoInitializeEx() is not called here again to avoid STA/MTA conflicts.
  Microsoft::WRL::ComPtr<IMMDeviceEnumerator> enumerator =
      CoreAudioUtil::CreateDeviceEnumerator();
  if (!enumerator) {
    send_log(
        " => (ERROR: CreateDeviceEnumerator=[Failed to create "
        "IMMDeviceEnumerator])");
    return false;
  }

  // Generate a collection of active audio endpoint devices.
  // This method will succeed even if all devices are disabled.
  Microsoft::WRL::ComPtr<IMMDeviceCollection> collection;
  HRESULT hr = enumerator->EnumAudioEndpoints(data_flow, DEVICE_STATE_ACTIVE,
                                              &collection);
  if (FAILED(hr)) {
    send_log(
        base::StrCat({" => (ERROR: IMMDeviceCollection::EnumAudioEndpoints=[",
                      ErrorToString(hr), "])"}));
    return false;
  }

  // Retrieve the number of active devices.
  UINT number_of_active_devices = 0;
  collection->GetCount(&number_of_active_devices);
  if (number_of_active_devices == 0) {
    send_log(" => (WARNING: no active devices are found)");
    return true;
  }

  // Iterate over all active devices to extract their unique ID and friendly
  // name. Devices that fail to provide a valid unique ID or friendly name
  // are skipped and not added to the |device_names| list.
  for (UINT i = 0; i < number_of_active_devices; ++i) {
    AudioDeviceName device;
    // Retrieve unique name of endpoint device.
    // Example: "{0.0.1.00000000}.{8db6020f-18e3-4f25-b6f5-7726c9122574}".
    Microsoft::WRL::ComPtr<IMMDevice> audio_device;
    hr = collection->Item(i, &audio_device);
    if (FAILED(hr)) {
      send_log(base::StrCat({" => (ERROR: IMMDeviceCollection::Item=[",
                             ErrorToString(hr), "])"}));
      continue;
    }

    // Store the unique name.
    ScopedCoMem<WCHAR> endpoint_device_id;
    hr = audio_device->GetId(&endpoint_device_id);
    if (FAILED(hr)) {
      send_log(base::StrCat(
          {" => (ERROR: IMMDevice::GetId=[", ErrorToString(hr), "])"}));
      continue;
    }
    device.unique_id =
        base::WideToUTF8(static_cast<WCHAR*>(endpoint_device_id));

    // Retrieve user-friendly name of endpoint device.
    // Example: "Microphone (Realtek High Definition Audio)".
    Microsoft::WRL::ComPtr<IPropertyStore> properties;
    hr = audio_device->OpenPropertyStore(STGM_READ, &properties);
    if (FAILED(hr)) {
      send_log(base::StrCat({" => (ERROR: IMMDevice::OpenPropertyStore=[",
                             ErrorToString(hr), "])"}));
      continue;
    }

    ScopedPropVariant friendly_name;
    hr =
        properties->GetValue(PKEY_Device_FriendlyName, friendly_name.Receive());
    if (FAILED(hr)) {
      send_log(base::StrCat(
          {" => (ERROR: IPropertyStore::GetValue=[", ErrorToString(hr), "])"}));
      continue;
    }

    // Store the user-friendly name.
    if (friendly_name.get().vt == VT_LPWSTR && friendly_name.get().pwszVal) {
      device.device_name = base::WideToUTF8(friendly_name.get().pwszVal);
    } else {
      send_log(" => (WARNING: friendly name is not a valid string)");
      continue;
    }

    // Append a suffix to USB and Bluetooth devices.  For USB devices, the
    // suffix contains the vendor id and product id using the format
    // (VID:PID). For example: (045e:0810).  For Bluetooth devices, the suffix
    // is (Bluetooth).
    const std::string device_instance_id =
        GetDeviceInstanceId(audio_device.Get(), enumerator.Get());
    if (device_instance_id.empty()) {
      send_log(" => (ERROR: failed to get instance ID)");
    } else {
      std::string suffix = GetDeviceSuffixWin(device_instance_id);
      if (!suffix.empty())
        device.device_name += suffix;
    }

    // Add combination of user-friendly and unique name to the output list.
    device_names->push_back(device);
  }

  if (device_names->size() != number_of_active_devices) {
    send_log(base::StrCat({" => (WARNING: device count mismatch, expected=[",
                           base::NumberToString(number_of_active_devices),
                           "], actual=[",
                           base::NumberToString(device_names->size()), "])"}));
  }

  return true;
}
bool GetInputDeviceNamesWin(
    AudioDeviceNames* device_names,
    const media::AudioManager::LogCallback& log_callback) {
  return GetDeviceNamesWinImpl(eCapture, device_names, log_callback);
}

bool GetOutputDeviceNamesWin(
    AudioDeviceNames* device_names,
    const media::AudioManager::LogCallback& log_callback) {
  return GetDeviceNamesWinImpl(eRender, device_names, log_callback);
}

std::string GetDeviceSuffixWin(std::string_view controller_id) {
  if (controller_id.size() >= 21 &&
      base::StartsWith(controller_id, "USB\\VID_") &&
      controller_id.substr(12, 5) == "&PID_") {
    return base::StrCat({" (", base::ToLowerASCII(controller_id.substr(8, 4)),
                         ":", base::ToLowerASCII(controller_id.substr(17, 4)),
                         ")"});
  }

  if ((controller_id.size() >= 22 &&
       base::StartsWith(controller_id, "BTHHFENUM\\BthHFPAudio\\")) ||
      (controller_id.size() >= 8 &&
       base::StartsWith(controller_id, "BTHENUM\\"))) {
    return " (Bluetooth)";
  }
  return std::string();
}

}  // namespace media
