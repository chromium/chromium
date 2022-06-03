// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_service_listener.h"

#include <utility>

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
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
  ServiceProcessHost::AddObserver(this);
  Init(ServiceProcessHost::GetRunningProcessInfo());
}

AudioServiceListener::~AudioServiceListener() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  ServiceProcessHost::RemoveObserver(this);
}

base::ProcessId AudioServiceListener::GetProcessId() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  return process_id_;
}

void AudioServiceListener::Init(
    std::vector<ServiceProcessInfo> running_service_processes) {
  for (const auto& info : running_service_processes) {
    if (info.IsService<audio::mojom::AudioService>()) {
      process_id_ = info.pid;
      MaybeSetLogFactory();
      break;
    }
  }
}

void AudioServiceListener::OnServiceProcessLaunched(
    const ServiceProcessInfo& info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  if (!info.IsService<audio::mojom::AudioService>())
    return;

  process_id_ = info.pid;
  MaybeSetLogFactory();
}

void AudioServiceListener::OnServiceProcessTerminatedNormally(
    const ServiceProcessInfo& info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  if (!info.IsService<audio::mojom::AudioService>())
    return;

  process_id_ = base::kNullProcessId;
  log_factory_is_set_ = false;
}

void AudioServiceListener::OnServiceProcessCrashed(
    const ServiceProcessInfo& info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  if (!info.IsService<audio::mojom::AudioService>())
    return;

  process_id_ = base::kNullProcessId;
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

}  // namespace content
