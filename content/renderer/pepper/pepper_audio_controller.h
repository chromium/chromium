// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_AUDIO_CONTROLLER_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_AUDIO_CONTROLLER_H_

#include <set>

#include "base/macros.h"

namespace content {

class PepperAudioOutputHost;
class PepperPluginInstanceImpl;
class PPB_Audio_Impl;

/**
 * This class controls all the active audio instances of a Pepper instance.
 * This class can only be a non-shareable member of PepperPluginInstanceImpl.
 */
class PepperAudioController {
 public:
  explicit PepperAudioController(PepperPluginInstanceImpl* instance);
  virtual ~PepperAudioController();

  // Adds an audio instance to the controller.
  void AddInstance(PPB_Audio_Impl* audio);
  void AddInstance(PepperAudioOutputHost* audio_output);

  // Removes an audio instance from the controller.
  void RemoveInstance(PPB_Audio_Impl* audio);
  void RemoveInstance(PepperAudioOutputHost* audio_output);

  // Sets the volume of all audio instances.
  void SetVolume(double volume);

  // The pepper instance has been deleted. This method can only be called
  // once. The controller will be invalidated after this call, and then all
  // other methods will be no-op.
  void OnPepperInstanceDeleted();

 private:
  // Notifies the RenderFrame that the playback has stopped.  This method should
  // only be called when |ppb_audios_| turns from non-empty to empty.
  void NotifyPlaybackStopsOnEmpty();

  // Helper functions to deal with the first and last audio instance.
  void StartPlaybackIfFirstInstance();
  void StopPlaybackIfLastInstance();

  // All active audio instances that are using the old
  // PPB_Audio interface.
  std::set<PPB_Audio_Impl*> ppb_audios_;

  // All active audio output instances that are using the new
  // PPB_AudioOutput interface.
  std::set<PepperAudioOutputHost*> audio_output_hosts_;

  // The Pepper instance which this controller is for. Will be null after
  // OnPepperInstanceDeleted() is called.
  PepperPluginInstanceImpl* instance_;

  DISALLOW_COPY_AND_ASSIGN(PepperAudioController);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_AUDIO_CONTROLLER_H_
