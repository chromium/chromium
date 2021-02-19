// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_AUDIO_CONTROLLER_CHROMEOS_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_AUDIO_CONTROLLER_CHROMEOS_H_

#include <stdint.h>

#include "ash/components/audio/cras_audio_handler.h"
#include "base/macros.h"

namespace extensions {

// Ensures that the "best" input and output audio devices are always active.
class ShellAudioController : public ash::CrasAudioHandler::AudioObserver {
 public:
  ShellAudioController();
  ~ShellAudioController() override;

  // ash::CrasAudioHandler::Observer implementation:
  void OnAudioNodesChanged() override;

 private:
  // Gets the current device list from CRAS, chooses the best input and output
  // device, and activates them if they aren't already active.
  void ActivateDevices();

  DISALLOW_COPY_AND_ASSIGN(ShellAudioController);
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_AUDIO_CONTROLLER_CHROMEOS_H_
