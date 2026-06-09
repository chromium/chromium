// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_service_listener.h"

#include <utility>

#include "base/feature_list.h"
#include "base/process/process.h"
#include "base/time/default_tick_clock.h"
#include "content/browser/media/audio_log_factory.h"
#include "content/public/browser/audio_service.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/audio/public/mojom/audio_service.mojom.h"
#include "services/audio/public/mojom/log_factory_manager.mojom.h"

namespace content {

AudioServiceListener::AudioServiceListener() {
  AddAudioServiceProcessObserver(this);
}

AudioServiceListener::~AudioServiceListener() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  RemoveAudioServiceProcessObserver(this);
}

base::Process AudioServiceListener::GetProcess() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  if (!audio_process_.IsValid())
    return base::Process();
  return audio_process_.Duplicate();
}

void AudioServiceListener::OnServiceLaunched(const ServiceProcessInfo& info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  audio_process_ = info.GetProcess().Duplicate();
  MaybeSetLogFactory();
}

void AudioServiceListener::OnServiceTerminatedNormally(
    const ServiceProcessInfo& info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  audio_process_ = base::Process();
  log_factory_is_set_ = false;
}

void AudioServiceListener::OnServiceCrashed(const ServiceProcessInfo& info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  audio_process_ = base::Process();
  log_factory_is_set_ = false;
}

void AudioServiceListener::MaybeSetLogFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  if (!base::FeatureList::IsEnabled(features::kAudioServiceOutOfProcess) ||
      log_factory_is_set_)
    return;

  mojo::PendingRemote<media::mojom::AudioLogFactory> audio_log_factory;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<AudioLogFactory>(),
      audio_log_factory.InitWithNewPipeAndPassReceiver());
  mojo::Remote<audio::mojom::LogFactoryManager> log_factory_manager;
  GetAudioService().BindLogFactoryManager(
      log_factory_manager.BindNewPipeAndPassReceiver());
  log_factory_manager->SetLogFactory(std::move(audio_log_factory));
  log_factory_is_set_ = true;
}

void AudioServiceListener::ResetForTesting() {  // IN-TEST
  audio_process_ = base::Process();
  log_factory_is_set_ = false;
  DETACH_FROM_SEQUENCE(owning_sequence_);
}

}  // namespace content
