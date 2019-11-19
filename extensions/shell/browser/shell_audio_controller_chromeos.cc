// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_audio_controller_chromeos.h"

#include <algorithm>

#include "chromeos/audio/audio_device.h"

namespace extensions {

namespace {

// Returns a pointer to the device in |devices| with ID |node_id|, or NULL if it
// isn't present.
const chromeos::AudioDevice* GetDevice(const chromeos::AudioDeviceList& devices,
                                       uint64_t node_id) {
  for (chromeos::AudioDeviceList::const_iterator it = devices.begin();
       it != devices.end(); ++it) {
    if (it->id == node_id)
      return &(*it);
  }
  return NULL;
}

}  // namespace

ShellAudioController::ShellAudioController() {
  chromeos::CrasAudioHandler::Get()->AddAudioObserver(this);
  ActivateDevices();
}

ShellAudioController::~ShellAudioController() {
  chromeos::CrasAudioHandler::Get()->RemoveAudioObserver(this);
}

void ShellAudioController::OnAudioNodesChanged() {
  VLOG(1) << "Audio nodes changed";
  ActivateDevices();
}

void ShellAudioController::ActivateDevices() {
  chromeos::CrasAudioHandler* handler = chromeos::CrasAudioHandler::Get();
  chromeos::AudioDeviceList devices;
  handler->GetAudioDevices(&devices);
  sort(devices.begin(), devices.end(), chromeos::AudioDeviceCompare());

  uint64_t best_input = 0, best_output = 0;
  for (chromeos::AudioDeviceList::const_reverse_iterator it = devices.rbegin();
       it != devices.rend() && (!best_input || !best_output); ++it) {
    // TODO(derat): Need to check |plugged_time|?
    if (it->is_input && !best_input)
      best_input = it->id;
    else if (!it->is_input && !best_output)
      best_output = it->id;
  }

  if (best_input && best_input != handler->GetPrimaryActiveInputNode()) {
    const chromeos::AudioDevice* device = GetDevice(devices, best_input);
    DCHECK(device);
    VLOG(1) << "Activating input device: " << device->ToString();
    handler->SwitchToDevice(*device, true,
                            chromeos::CrasAudioHandler::ACTIVATE_BY_USER);
  }
  if (best_output && best_output != handler->GetPrimaryActiveOutputNode()) {
    const chromeos::AudioDevice* device = GetDevice(devices, best_output);
    DCHECK(device);
    VLOG(1) << "Activating output device: " << device->ToString();
    handler->SwitchToDevice(*device, true,
                            chromeos::CrasAudioHandler::ACTIVATE_BY_USER);
  }
}

}  // namespace extensions
