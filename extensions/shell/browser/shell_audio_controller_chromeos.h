// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_AUDIO_CONTROLLER_CHROMEOS_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_AUDIO_CONTROLLER_CHROMEOS_H_

#include <stdint.h>

#include "chromeos/ash/components/audio/cras_audio_handler.h"

namespace extensions {

// Ensures that the "best" input and output audio devices are always active.
class ShellAudioController : public ash::CrasAudioHandler::AudioObserver {
 public:
  ShellAudioController();

  ShellAudioController(const ShellAudioController&) = delete;
  ShellAudioController& operator=(const ShellAudioController&) = delete;

  ~ShellAudioController() override;

  // ash::CrasAudioHandler::Observer implementation:
  void OnAudioNodesChanged() override;

 private:
  // Gets the current device list from CRAS, chooses the best input and output
  // device, and activates them if they aren't already active.
  void ActivateDevices();
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_AUDIO_CONTROLLER_CHROMEOS_H_
