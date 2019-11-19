// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_service_listener.h"

#include <utility>

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/default_tick_clock.h"
#include "content/browser/media/audio_log_factory.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/audio/public/mojom/constants.mojom.h"
#include "services/audio/public/mojom/log_factory_manager.mojom.h"

namespace content {

AudioServiceListener::Metrics::Metrics(const base::TickClock* clock)
    : clock_(clock), initial_downtime_start_(clock_->NowTicks()) {}

AudioServiceListener::Metrics::~Metrics() = default;

void AudioServiceListener::Metrics::ServiceAlreadyRunning(
    service_manager::mojom::InstanceState state) {
  LogServiceStartStatus(ServiceStartStatus::kAlreadyStarted);
  initial_downtime_start_ = base::TimeTicks();
  if (state == service_manager::mojom::InstanceState::kStarted)
    started_ = clock_->NowTicks();
}

void AudioServiceListener::Metrics::ServiceCreated() {
  DCHECK(created_.is_null());
  created_ = clock_->NowTicks();
}

void AudioServiceListener::Metrics::ServiceStarted() {
  started_ = clock_->NowTicks();

  // |created_| is uninitialized if OnServiceCreated() was called before the
  // listener is initialized with OnInit() call.
  if (!created_.is_null()) {
    LogServiceStartStatus(ServiceStartStatus::kSuccess);
    UMA_HISTOGRAM_TIMES("Media.AudioService.ObservedStartupTime",
                        started_ - created_);
    created_ = base::TimeTicks();
  }

  if (!initial_downtime_start_.is_null()) {
    UMA_HISTOGRAM_CUSTOM_TIMES("Media.AudioService.ObservedInitialDowntime",
                               started_ - initial_downtime_start_,
                               base::TimeDelta(), base::TimeDelta::FromDays(7),
                               50);
    initial_downtime_start_ = base::TimeTicks();
  }

  if (!stopped_.is_null()) {
    UMA_HISTOGRAM_CUSTOM_TIMES("Media.AudioService.ObservedDowntime2",
                               started_ - stopped_, base::TimeDelta(),
                               base::TimeDelta::FromDays(7), 50);
    stopped_ = base::TimeTicks();
  }
}

void AudioServiceListener::Metrics::ServiceFailedToStart() {
  LogServiceStartStatus(ServiceStartStatus::kFailure);
  created_ = base::TimeTicks();
}

void AudioServiceListener::Metrics::ServiceStopped() {
  stopped_ = clock_->NowTicks();

  DCHECK(!started_.is_null());
  UMA_HISTOGRAM_CUSTOM_TIMES("Media.AudioService.ObservedUptime",
                             stopped_ - started_, base::TimeDelta(),
                             base::TimeDelta::FromDays(7), 50);

  created_ = base::TimeTicks();
  started_ = base::TimeTicks();
}

void AudioServiceListener::Metrics::LogServiceStartStatus(
    Metrics::ServiceStartStatus status) {
  UMA_HISTOGRAM_ENUMERATION("Media.AudioService.ObservedStartStatus", status);
}

AudioServiceListener::AudioServiceListener(
    std::unique_ptr<service_manager::Connector> connector)
    : connector_(std::move(connector)),
      metrics_(base::DefaultTickClock::GetInstance()) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  if (!connector_)
    return;  // Happens in unittests.

  mojo::Remote<service_manager::mojom::ServiceManager> service_manager;
  connector_->Connect(service_manager::mojom::kServiceName,
                      service_manager.BindNewPipeAndPassReceiver());
  mojo::PendingRemote<service_manager::mojom::ServiceManagerListener> listener;
  mojo::PendingReceiver<service_manager::mojom::ServiceManagerListener> request(
      listener.InitWithNewPipeAndPassReceiver());
  service_manager->AddListener(std::move(listener));
  receiver_.Bind(std::move(request));
}

AudioServiceListener::~AudioServiceListener() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
}

base::ProcessId AudioServiceListener::GetProcessId() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  return process_id_;
}

void AudioServiceListener::OnInit(
    std::vector<service_manager::mojom::RunningServiceInfoPtr>
        running_services) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  for (const service_manager::mojom::RunningServiceInfoPtr& instance :
       running_services) {
    if (instance->identity.name() == audio::mojom::kServiceName &&
        instance->state !=
            service_manager::mojom::InstanceState::kUnreachable) {
      current_instance_identity_ = instance->identity;
      current_instance_state_ = instance->state;
      metrics_.ServiceAlreadyRunning(instance->state);
      MaybeSetLogFactory();

      // NOTE: This may not actually be a valid PID yet. If not, we will
      // receive OnServicePIDReceived soon.
      process_id_ = instance->pid;
      break;
    }
  }
}

void AudioServiceListener::OnServiceCreated(
    service_manager::mojom::RunningServiceInfoPtr service) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  if (service->identity.name() != audio::mojom::kServiceName)
    return;

  if (current_instance_identity_) {
    // If we were already tracking an instance of the service, it must be dying
    // soon. We'll start tracking the new instance instead now, so simulate
    // stoppage of the old one.
    DCHECK(service->identity != current_instance_identity_);
    if (current_instance_state_ ==
        service_manager::mojom::InstanceState::kCreated) {
      OnServiceFailedToStart(*current_instance_identity_);
    } else {
      DCHECK_EQ(service_manager::mojom::InstanceState::kStarted,
                *current_instance_state_);
      OnServiceStopped(*current_instance_identity_);
    }
  }

  current_instance_identity_ = service->identity;
  current_instance_state_ = service_manager::mojom::InstanceState::kCreated;
  metrics_.ServiceCreated();
  MaybeSetLogFactory();
}

void AudioServiceListener::OnServiceStarted(
    const ::service_manager::Identity& identity,
    uint32_t pid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  if (identity.name() != audio::mojom::kServiceName)
    return;

  DCHECK(identity == current_instance_identity_);
  DCHECK(current_instance_state_ ==
         service_manager::mojom::InstanceState::kCreated);
  current_instance_state_ = service_manager::mojom::InstanceState::kStarted;
  metrics_.ServiceStarted();
}

void AudioServiceListener::OnServicePIDReceived(
    const ::service_manager::Identity& identity,
    uint32_t pid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  if (identity != current_instance_identity_)
    return;

  process_id_ = pid;
}

void AudioServiceListener::OnServiceFailedToStart(
    const ::service_manager::Identity& identity) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  if (identity != current_instance_identity_)
    return;

  metrics_.ServiceFailedToStart();
  current_instance_identity_.reset();
  current_instance_state_.reset();
  process_id_ = base::kNullProcessId;
  log_factory_is_set_ = false;
}

void AudioServiceListener::OnServiceStopped(
    const ::service_manager::Identity& identity) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  if (identity != current_instance_identity_)
    return;

  metrics_.ServiceStopped();
  current_instance_identity_.reset();
  current_instance_state_.reset();
  process_id_ = base::kNullProcessId;
  log_factory_is_set_ = false;
}

void AudioServiceListener::MaybeSetLogFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  DCHECK(current_instance_identity_);
  if (!base::FeatureList::IsEnabled(features::kAudioServiceOutOfProcess) ||
      !connector_ || log_factory_is_set_)
    return;

  mojo::PendingRemote<media::mojom::AudioLogFactory> audio_log_factory;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<AudioLogFactory>(),
      audio_log_factory.InitWithNewPipeAndPassReceiver());
  mojo::Remote<audio::mojom::LogFactoryManager> log_factory_manager;
  connector_->Connect(*current_instance_identity_,
                      log_factory_manager.BindNewPipeAndPassReceiver());
  log_factory_manager->SetLogFactory(std::move(audio_log_factory));
  log_factory_is_set_ = true;
}

}  // namespace content
