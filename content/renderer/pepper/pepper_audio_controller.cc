// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_audio_controller.h"

#include "content/renderer/pepper/pepper_audio_output_host.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/pepper/ppb_audio_impl.h"

namespace content {

PepperAudioController::PepperAudioController(
    PepperPluginInstanceImpl* instance)
    : instance_(instance) {
  DCHECK(instance_);
}

PepperAudioController::~PepperAudioController() {
  if (instance_)
    OnPepperInstanceDeleted();
}

void PepperAudioController::AddInstance(PPB_Audio_Impl* audio) {
  if (!instance_)
    return;
  if (ppb_audios_.count(audio))
    return;

  StartPlaybackIfFirstInstance();

  ppb_audios_.insert(audio);
}

void PepperAudioController::AddInstance(PepperAudioOutputHost* audio_output) {
  if (!instance_)
    return;
  if (audio_output_hosts_.count(audio_output))
    return;

  StartPlaybackIfFirstInstance();

  audio_output_hosts_.insert(audio_output);
}

void PepperAudioController::RemoveInstance(PPB_Audio_Impl* audio) {
  if (!instance_)
    return;
  if (!ppb_audios_.count(audio))
    return;

  ppb_audios_.erase(audio);

  StopPlaybackIfLastInstance();
}

void PepperAudioController::RemoveInstance(
    PepperAudioOutputHost* audio_output) {
  if (!instance_)
    return;
  if (!audio_output_hosts_.count(audio_output))
    return;

  audio_output_hosts_.erase(audio_output);

  StopPlaybackIfLastInstance();
}

void PepperAudioController::SetVolume(double volume) {
  if (!instance_)
    return;

  for (PPB_Audio_Impl* ppb_audio : ppb_audios_) {
    ppb_audio->SetVolume(volume);
  }

  for (PepperAudioOutputHost* audio_output_host : audio_output_hosts_) {
    audio_output_host->SetVolume(volume);
  }
}

void PepperAudioController::OnPepperInstanceDeleted() {
  DCHECK(instance_);

  if (!audio_output_hosts_.empty() || !ppb_audios_.empty())
    NotifyPlaybackStopsOnEmpty();

  ppb_audios_.clear();
  audio_output_hosts_.clear();
  instance_ = nullptr;
}

void PepperAudioController::NotifyPlaybackStopsOnEmpty() {
  DCHECK(instance_);

  mojom::PepperPluginInstanceHost* instance_host =
      instance_->GetPepperPluginInstanceHost();
  if (instance_host)
    instance_host->StopsPlayback();
}

void PepperAudioController::StartPlaybackIfFirstInstance() {
  DCHECK(instance_);

  if (audio_output_hosts_.empty() && ppb_audios_.empty()) {
    mojom::PepperPluginInstanceHost* instance_host =
        instance_->GetPepperPluginInstanceHost();
    if (instance_host)
      instance_host->StartsPlayback();
  }
}

void PepperAudioController::StopPlaybackIfLastInstance() {
  DCHECK(instance_);

  if (audio_output_hosts_.empty() && ppb_audios_.empty())
    NotifyPlaybackStopsOnEmpty();
}

}  // namespace content
