// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/cras/cras_util.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/cras/audio_manager_cras_base.h"

namespace media {

namespace {

const char kInternalInputVirtualDevice[] = "Built-in mic";
const char kInternalOutputVirtualDevice[] = "Built-in speaker";
const char kHeadphoneLineOutVirtualDevice[] = "Headphone/Line Out";
const char kKeyBoardMic[] = "KEYBOARD_MIC";

// Returns if that an input or output audio device is for simple usage like
// playback or recording for user. In contrast, audio device such as loopback,
// always on keyword recognition (HOTWORD), and keyboard mic are not for simple
// usage.
// One special case is ALSA loopback device, which will only exist under
// testing. We want it visible to users for e2e tests.
bool IsForSimpleUsage(uint32_t type) {
  return type == CRAS_NODE_TYPE_INTERNAL_SPEAKER ||
         type == CRAS_NODE_TYPE_HEADPHONE || type == CRAS_NODE_TYPE_HDMI ||
         type == CRAS_NODE_TYPE_LINEOUT || type == CRAS_NODE_TYPE_MIC ||
         type == CRAS_NODE_TYPE_BLUETOOTH_NB_MIC ||
         type == CRAS_NODE_TYPE_USB || type == CRAS_NODE_TYPE_BLUETOOTH ||
         type == CRAS_NODE_TYPE_ALSA_LOOPBACK;
}

// Connects to the CRAS server.
cras_client* CrasConnect() {
  cras_client* client;
  if (cras_client_create(&client)) {
    LOG(ERROR) << "Couldn't create CRAS client.\n";
    return nullptr;
  }
  if (cras_client_connect(client)) {
    LOG(ERROR) << "Couldn't connect CRAS client.\n";
    cras_client_destroy(client);
    return nullptr;
  }
  return client;
}

// Disconnects from the CRAS server.
void CrasDisconnect(cras_client** client) {
  if (*client) {
    cras_client_stop(*client);
    cras_client_destroy(*client);
    *client = nullptr;
  }
}

}  // namespace

CrasDevice::CrasDevice() = default;

CrasDevice::CrasDevice(const cras_ionode_info* node,
                       const cras_iodev_info* dev,
                       DeviceType type)
    : type(type) {
  id = cras_make_node_id(node->iodev_idx, node->ionode_idx);
  active = node->active;
  name = std::string(node->name);
  // If the name of node is not meaningful, use the device name instead.
  if (name.empty() || name == "(default)")
    name = dev->name;
  dev_name = dev->name;
}

// Creates a CrasDevice based on the node list.
// If there is only one node attached to this device, create it directly.
// If there are two nodes, create a virtual device instead.
CrasDevice::CrasDevice(const std::vector<cras_ionode_info>& nodes,
                       const cras_iodev_info* dev,
                       DeviceType type)
    : CrasDevice(&nodes[0], dev, type) {
  if (nodes.size() == 1)
    return;

  if (nodes.size() > 2) {
    LOG(WARNING) << dev->name << " has more than 2 nodes";
    return;
  }

  if (nodes[0].type_enum == CRAS_NODE_TYPE_LINEOUT ||
      nodes[1].type_enum == CRAS_NODE_TYPE_LINEOUT) {
    name = kHeadphoneLineOutVirtualDevice;
  } else if (nodes[0].type_enum == CRAS_NODE_TYPE_INTERNAL_SPEAKER ||
             nodes[1].type_enum == CRAS_NODE_TYPE_INTERNAL_SPEAKER) {
    name = kInternalOutputVirtualDevice;
  } else if (nodes[0].type_enum == CRAS_NODE_TYPE_MIC ||
             nodes[1].type_enum == CRAS_NODE_TYPE_MIC) {
    name = kInternalInputVirtualDevice;
  } else {
    LOG(WARNING) << "Failed to create virtual device for " << dev->name;
  }

  active = nodes[0].active || nodes[1].active;
}

std::vector<CrasDevice> CrasGetAudioDevices(DeviceType type) {
  std::vector<CrasDevice> devices;

  cras_client* client = CrasConnect();
  if (!client)
    return devices;

  struct cras_iodev_info devs[CRAS_MAX_IODEVS];
  struct cras_ionode_info nodes[CRAS_MAX_IONODES];
  size_t num_devs = CRAS_MAX_IODEVS, num_nodes = CRAS_MAX_IONODES;
  int rc;

  if (type == DeviceType::kInput) {
    rc = cras_client_get_input_devices(client, devs, nodes, &num_devs,
                                       &num_nodes);
  } else {
    rc = cras_client_get_output_devices(client, devs, nodes, &num_devs,
                                        &num_nodes);
  }
  if (rc < 0) {
    LOG(ERROR) << "Failed to get devices: " << std::strerror(rc);
    CrasDisconnect(&client);
    return devices;
  }

  for (size_t i = 0; i < num_devs; i++) {
    std::vector<cras_ionode_info> dev_nodes;
    for (size_t j = 0; j < num_nodes; j++) {
      if (!nodes[j].plugged || !IsForSimpleUsage(nodes[j].type_enum))
        continue;
      if (devs[i].idx == nodes[j].iodev_idx)
        dev_nodes.emplace_back(nodes[j]);
    }
    if (dev_nodes.empty())
      continue;
    devices.emplace_back(dev_nodes, &devs[i], type);
  }

  CrasDisconnect(&client);
  return devices;
}

bool CrasHasKeyboardMic() {
  cras_client* client = CrasConnect();
  if (!client)
    return false;

  struct cras_iodev_info devs[CRAS_MAX_IODEVS];
  struct cras_ionode_info nodes[CRAS_MAX_IONODES];
  size_t num_devs = CRAS_MAX_IODEVS, num_nodes = CRAS_MAX_IONODES;

  int rc =
      cras_client_get_input_devices(client, devs, nodes, &num_devs, &num_nodes);
  if (rc < 0) {
    LOG(ERROR) << "Failed to get devices: " << std::strerror(rc);
    CrasDisconnect(&client);
    return false;
  }

  for (size_t i = 0; i < num_nodes; i++) {
    if (std::string(nodes[i].type) == kKeyBoardMic) {
      CrasDisconnect(&client);
      return true;
    }
  }

  CrasDisconnect(&client);
  return false;
}

int CrasGetAecSupported() {
  cras_client* client = CrasConnect();
  if (!client)
    return 0;

  int rc = cras_client_get_aec_supported(client);
  CrasDisconnect(&client);
  return rc;
}

int CrasGetAecGroupId() {
  cras_client* client = CrasConnect();
  if (!client)
    return -1;

  int rc = cras_client_get_aec_group_id(client);
  CrasDisconnect(&client);

  return rc;
}

int CrasGetDefaultOutputBufferSize() {
  cras_client* client = CrasConnect();
  if (!client)
    return -1;

  int rc = cras_client_get_default_output_buffer_size(client);
  CrasDisconnect(&client);

  return rc;
}

}  // namespace media
