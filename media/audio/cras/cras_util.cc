// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/cras/cras_util.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/cras/audio_manager_cras_base.h"
#include "media/base/media_switches.h"

namespace media {

namespace {

constexpr char kInternalInputVirtualDevice[] = "Built-in mic";
constexpr char kInternalOutputVirtualDevice[] = "Built-in speaker/headphone";
constexpr char kHeadphoneLineOutVirtualDevice[] = "Headphone/Line Out";

// Names below are from the node_type_to_str function in CRAS server.
// https://chromium.googlesource.com/chromiumos/third_party/adhd/+/refs/heads/main/cras/src/server/cras_iodev_list.c
constexpr char kInternalSpeaker[] = "INTERNAL_SPEAKER";
constexpr char kHeadphone[] = "HEADPHONE";
constexpr char kHDMI[] = "HDMI";
constexpr char kLineout[] = "LINEOUT";
constexpr char kMic[] = "MIC";
constexpr char kInternalMic[] = "INTERNAL_MIC";
constexpr char kFrontMic[] = "FRONT_MIC";
constexpr char kRearMic[] = "REAR_MIC";
constexpr char kBluetoothNBMic[] = "BLUETOOTH_NB_MIC";
constexpr char kUSB[] = "USB";
constexpr char kBluetooth[] = "BLUETOOTH";
constexpr char kAlsaLoopback[] = "ALSA_LOOPBACK";

// Returns if that an input or output audio device is for simple usage like
// playback or recording for user. In contrast, audio device such as loopback,
// always on keyword recognition (HOTWORD), and keyboard mic are not for simple
// usage.
// One special case is ALSA loopback device, which will only exist under
// testing. We want it visible to users for e2e tests.
bool IsForSimpleUsage(std::string type) {
  return type == kInternalSpeaker || type == kHeadphone || type == kHDMI ||
         type == kLineout || type == kMic || type == kInternalMic ||
         type == kFrontMic || type == kRearMic || type == kBluetoothNBMic ||
         type == kUSB || type == kBluetooth || type == kAlsaLoopback;
}

bool IsInternalMic(std::string type) {
  return type == kInternalMic || type == kFrontMic || type == kRearMic;
}

// Connects to the CRAS server.
libcras_client* CrasConnect() {
  libcras_client* client;

  client = libcras_client_create();
  if (!client) {
    LOG(ERROR) << "Couldn't create CRAS client.\n";
    return nullptr;
  }
  if (libcras_client_connect(client)) {
    LOG(ERROR) << "Couldn't connect CRAS client.\n";
    libcras_client_destroy(client);
    return nullptr;
  }
  return client;
}

// Disconnects from the CRAS server.
void CrasDisconnect(libcras_client** client) {
  if (*client) {
    libcras_client_stop(*client);
    libcras_client_destroy(*client);
    *client = nullptr;
  }
}

}  // namespace

CrasDevice::CrasDevice() = default;

CrasDevice::CrasDevice(struct libcras_node_info* node, DeviceType type)
    : type(type) {
  int rc;
  rc = libcras_node_info_get_id(node, &id);
  if (rc) {
    LOG(ERROR) << "Failed to get the node id: " << rc;
    id = 0;
  }

  rc = libcras_node_info_get_dev_idx(node, &dev_idx);
  if (rc) {
    LOG(ERROR) << "Failed to get the dev idx: " << rc;
    dev_idx = 0;
  }

  rc = libcras_node_info_is_plugged(node, &plugged);
  if (rc) {
    LOG(ERROR) << "Failed to get if the node is plugged: " << rc;
    plugged = false;
  }

  rc = libcras_node_info_is_active(node, &active);
  if (rc) {
    LOG(ERROR) << "Failed to get if the node is active: " << rc;
    active = false;
  }

  char* type_str;
  rc = libcras_node_info_get_type(node, &type_str);
  if (rc) {
    LOG(ERROR) << "Failed to get the node type: " << rc;
    node_type = nullptr;
  }
  node_type = type_str;

  char* node_name;
  rc = libcras_node_info_get_node_name(node, &node_name);
  if (rc) {
    LOG(ERROR) << "Failed to get the node name: " << rc;
    node_name = nullptr;
  }

  char* device_name;
  rc = libcras_node_info_get_dev_name(node, &device_name);
  if (rc) {
    LOG(ERROR) << "Failed to get the dev name: " << rc;
    device_name = nullptr;
  }

  rc = libcras_node_info_get_max_supported_channels(node,
                                                    &max_supported_channels);
  if (rc) {
    LOG(ERROR) << "Failed to get max supported channels: " << rc;
    max_supported_channels = 0;
  }

  name = std::string(node_name);
  if (name.empty() || name == "(default)") {
    name = device_name;
  }
  dev_name = device_name;
}

CrasDevice::CrasDevice(DeviceType type,
                       uint64_t id,
                       uint32_t dev_idx,
                       uint32_t max_supported_channels,
                       bool plugged,
                       bool active,
                       std::string node_type,
                       std::string name,
                       std::string dev_name)
    : type(type),
      id(id),
      dev_idx(dev_idx),
      max_supported_channels(max_supported_channels),
      plugged(plugged),
      active(active),
      node_type(node_type),
      name(name),
      dev_name(dev_name) {}

void mergeDevices(CrasDevice& old_dev, CrasDevice& new_dev) {
  if (old_dev.node_type == kLineout || new_dev.node_type == kLineout) {
    old_dev.name = kHeadphoneLineOutVirtualDevice;
    old_dev.node_type = "";
  } else if (old_dev.node_type == kInternalSpeaker ||
             new_dev.node_type == kInternalSpeaker) {
    old_dev.name = kInternalOutputVirtualDevice;
    old_dev.node_type = "";
  } else if (IsInternalMic(old_dev.node_type) ||
             IsInternalMic(new_dev.node_type)) {
    old_dev.name = kInternalInputVirtualDevice;
    old_dev.node_type = "";
  } else {
    LOG(WARNING) << "Failed to create virtual device for " << old_dev.name;
  }
  old_dev.active |= new_dev.active;
}

CrasUtil::CrasUtil() = default;

CrasUtil::~CrasUtil() = default;

bool CrasUtil::CacheEffects() {
  libcras_client* client = CrasConnect();
  if (!client) {
    LOG(ERROR) << "Failed to cache effects";
    return false;
  }
  if (libcras_client_get_aec_supported(client, &aec_supported_) < 0) {
    LOG(ERROR) << "Fail to query AEC supported";
    aec_supported_ = false;
  }
  if (libcras_client_get_agc_supported(client, &agc_supported_) < 0) {
    LOG(ERROR) << "Fail to query AGC supported";
    agc_supported_ = false;
  }
  if (libcras_client_get_ns_supported(client, &ns_supported_) < 0) {
    LOG(ERROR) << "Fail to query NS supported";
    ns_supported_ = false;
  }
  if (base::FeatureList::IsEnabled(media::kCrOSSystemVoiceIsolationOption)) {
    if (libcras_client_get_voice_isolation_supported(
            client, &voice_isolation_supported_) < 0) {
      LOG(ERROR) << "Fail to query VoiceIsolation supported";
      voice_isolation_supported_ = false;
    }
  }
  if (libcras_client_get_aec_group_id(client, &aec_group_id_) < 0) {
    LOG(ERROR) << "Fail to query AEC group ID";
    aec_group_id_ = -1;  // The default group ID is -1
  }
  if (libcras_client_get_default_output_buffer_size(
          client, &default_output_buffer_size_) < 0) {
    LOG(ERROR) << "Fail to query default output buffer size";
    default_output_buffer_size_ = 0;
  }
  CrasDisconnect(&client);
  return true;
}

std::vector<CrasDevice> CrasUtil::CrasGetAudioDevices(DeviceType type) {
  std::vector<CrasDevice> devices;

  libcras_client* client = CrasConnect();
  if (!client) {
    return devices;
  }

  int rc;

  struct libcras_node_info** nodes;
  size_t num_nodes;

  if (type == DeviceType::kInput) {
    rc =
        libcras_client_get_nodes(client, CRAS_STREAM_INPUT, &nodes, &num_nodes);
  } else {
    rc = libcras_client_get_nodes(client, CRAS_STREAM_OUTPUT, &nodes,
                                  &num_nodes);
  }

  if (rc < 0) {
    LOG(ERROR) << "Failed to get devices: " << std::strerror(rc);
    CrasDisconnect(&client);
    return devices;
  }

  for (size_t i = 0; i < num_nodes; i++) {
    auto new_dev = CrasDevice(nodes[i], type);
    if (!new_dev.plugged || !IsForSimpleUsage(new_dev.node_type)) {
      continue;
    }
    bool added = false;
    for (auto& dev : devices) {
      if (dev.dev_idx == new_dev.dev_idx) {
        mergeDevices(dev, new_dev);
        added = true;
        break;
      }
    }
    if (!added) {
      devices.emplace_back(new_dev);
    }
  }

  libcras_node_info_array_destroy(nodes, num_nodes);

  CrasDisconnect(&client);
  return devices;
}

int CrasUtil::CrasGetAecSupported() {
  if (!cras_effects_cached_) {
    cras_effects_cached_ = CacheEffects();
  }
  return aec_supported_;
}

int CrasUtil::CrasGetAgcSupported() {
  if (!cras_effects_cached_) {
    cras_effects_cached_ = CacheEffects();
  }
  return agc_supported_;
}

int CrasUtil::CrasGetNsSupported() {
  if (!cras_effects_cached_) {
    cras_effects_cached_ = CacheEffects();
  }
  return ns_supported_;
}

int CrasUtil::CrasGetVoiceIsolationSupported() {
  if (!cras_effects_cached_) {
    cras_effects_cached_ = CacheEffects();
  }
  return voice_isolation_supported_;
}

int CrasUtil::CrasGetAecGroupId() {
  if (!cras_effects_cached_) {
    cras_effects_cached_ = CacheEffects();
  }
  return aec_group_id_;
}

int CrasUtil::CrasGetDefaultOutputBufferSize() {
  if (!cras_effects_cached_) {
    cras_effects_cached_ = CacheEffects();
  }
  return default_output_buffer_size_;
}

}  // namespace media
