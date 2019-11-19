// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/background_service_handler.h"

#include "base/metrics/histogram_functions.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/render_process_host.h"

namespace content {
namespace protocol {

namespace {

devtools::proto::BackgroundService ServiceNameToEnum(
    const std::string& service_name) {
  if (service_name == BackgroundService::ServiceNameEnum::BackgroundFetch) {
    return devtools::proto::BackgroundService::BACKGROUND_FETCH;
  } else if (service_name ==
             BackgroundService::ServiceNameEnum::BackgroundSync) {
    return devtools::proto::BackgroundService::BACKGROUND_SYNC;
  } else if (service_name ==
             BackgroundService::ServiceNameEnum::PushMessaging) {
    return devtools::proto::BackgroundService::PUSH_MESSAGING;
  } else if (service_name ==
             BackgroundService::ServiceNameEnum::Notifications) {
    return devtools::proto::BackgroundService::NOTIFICATIONS;
  } else if (service_name ==
             BackgroundService::ServiceNameEnum::PaymentHandler) {
    return devtools::proto::BackgroundService::PAYMENT_HANDLER;
  } else if (service_name ==
             BackgroundService::ServiceNameEnum::PeriodicBackgroundSync) {
    return devtools::proto::BackgroundService::PERIODIC_BACKGROUND_SYNC;
  }
  return devtools::proto::BackgroundService::UNKNOWN;
}

std::string ServiceEnumToName(devtools::proto::BackgroundService service_enum) {
  switch (service_enum) {
    case devtools::proto::BackgroundService::BACKGROUND_FETCH:
      return BackgroundService::ServiceNameEnum::BackgroundFetch;
    case devtools::proto::BackgroundService::BACKGROUND_SYNC:
      return BackgroundService::ServiceNameEnum::BackgroundSync;
    case devtools::proto::BackgroundService::PUSH_MESSAGING:
      return BackgroundService::ServiceNameEnum::PushMessaging;
    case devtools::proto::BackgroundService::NOTIFICATIONS:
      return BackgroundService::ServiceNameEnum::Notifications;
    case devtools::proto::BackgroundService::PAYMENT_HANDLER:
      return BackgroundService::ServiceNameEnum::PaymentHandler;
    case devtools::proto::BackgroundService::PERIODIC_BACKGROUND_SYNC:
      return BackgroundService::ServiceNameEnum::PeriodicBackgroundSync;
    default:
      NOTREACHED();
  }

  return "invalid";
}

std::unique_ptr<protocol::Array<protocol::BackgroundService::EventMetadata>>
ProtoMapToArray(
    const google::protobuf::Map<std::string, std::string>& event_metadata_map) {
  auto metadata_array = std::make_unique<
      protocol::Array<protocol::BackgroundService::EventMetadata>>();

  for (const auto& entry : event_metadata_map) {
    auto event_metadata = protocol::BackgroundService::EventMetadata::Create()
                              .SetKey(entry.first)
                              .SetValue(entry.second)
                              .Build();
    metadata_array->emplace_back(std::move(event_metadata));
  }

  return metadata_array;
}

std::unique_ptr<protocol::BackgroundService::BackgroundServiceEvent>
ToBackgroundServiceEvent(const devtools::proto::BackgroundServiceEvent& event) {
  base::Time timestamp = base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromMicroseconds(event.timestamp()));
  return protocol::BackgroundService::BackgroundServiceEvent::Create()
      .SetTimestamp(timestamp.ToJsTime() / 1'000)  // milliseconds -> seconds
      .SetOrigin(event.origin())
      .SetServiceWorkerRegistrationId(
          base::NumberToString(event.service_worker_registration_id()))
      .SetService(ServiceEnumToName(event.background_service()))
      .SetEventName(event.event_name())
      .SetInstanceId(event.instance_id())
      .SetEventMetadata(ProtoMapToArray(event.event_metadata()))
      .Build();
}

}  // namespace

BackgroundServiceHandler::BackgroundServiceHandler()
    : DevToolsDomainHandler(BackgroundService::Metainfo::domainName),
      devtools_context_(nullptr) {}

BackgroundServiceHandler::~BackgroundServiceHandler() {
  DCHECK(enabled_services_.empty());
}

void BackgroundServiceHandler::Wire(UberDispatcher* dispatcher) {
  frontend_ =
      std::make_unique<BackgroundService::Frontend>(dispatcher->channel());
  BackgroundService::Dispatcher::wire(dispatcher, this);
}

void BackgroundServiceHandler::SetRenderer(int process_host_id,
                                           RenderFrameHostImpl* frame_host) {
  RenderProcessHost* process_host = RenderProcessHost::FromID(process_host_id);
  if (!process_host) {
    if (devtools_context_ && !enabled_services_.empty())
      devtools_context_->RemoveObserver(this);
    enabled_services_.clear();
    devtools_context_ = nullptr;
    return;
  }

  auto* storage_partition =
      static_cast<StoragePartitionImpl*>(process_host->GetStoragePartition());

  if (devtools_context_) {
    DCHECK_EQ(devtools_context_,
              storage_partition->GetDevToolsBackgroundServicesContext());
    return;
  }

  devtools_context_ = storage_partition->GetDevToolsBackgroundServicesContext();
  DCHECK(devtools_context_);
}

Response BackgroundServiceHandler::Disable() {
  if (!enabled_services_.empty())
    devtools_context_->RemoveObserver(this);
  enabled_services_.clear();
  return Response::OK();
}

void BackgroundServiceHandler::StartObserving(
    const std::string& service,
    std::unique_ptr<StartObservingCallback> callback) {
  DCHECK(devtools_context_);

  auto service_enum = ServiceNameToEnum(service);
  if (service_enum == devtools::proto::BackgroundService::UNKNOWN) {
    callback->sendFailure(Response::InvalidParams("Invalid service name"));
    return;
  }

  if (enabled_services_.count(service_enum)) {
    callback->sendSuccess();
    return;
  }

  if (enabled_services_.empty())
    devtools_context_->AddObserver(this);
  enabled_services_.insert(service_enum);

  bool is_recording = devtools_context_->IsRecording(service_enum);

  DCHECK(frontend_);
  frontend_->RecordingStateChanged(is_recording, service);

  devtools_context_->GetLoggedBackgroundServiceEvents(
      service_enum,
      base::BindOnce(&BackgroundServiceHandler::DidGetLoggedEvents,
                     weak_ptr_factory_.GetWeakPtr(), service_enum,
                     std::move(callback)));
}

Response BackgroundServiceHandler::StopObserving(const std::string& service) {
  auto service_enum = ServiceNameToEnum(service);
  if (service_enum == devtools::proto::BackgroundService::UNKNOWN)
    return Response::InvalidParams("Invalid service name");

  if (!enabled_services_.count(service_enum))
    return Response::OK();

  enabled_services_.erase(service_enum);
  if (enabled_services_.empty())
    devtools_context_->RemoveObserver(this);

  return Response::OK();
}

void BackgroundServiceHandler::DidGetLoggedEvents(
    devtools::proto::BackgroundService service,
    std::unique_ptr<StartObservingCallback> callback,
    std::vector<devtools::proto::BackgroundServiceEvent> events) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // These events won't be duplicated in `OnEventReceived` since we are using
  // sequenced task runners.
  for (const auto& event : events)
    frontend_->BackgroundServiceEventReceived(ToBackgroundServiceEvent(event));

  return callback->sendSuccess();
}

Response BackgroundServiceHandler::SetRecording(bool should_record,
                                                const std::string& service) {
  DCHECK(devtools_context_);

  auto service_enum = ServiceNameToEnum(service);
  if (service_enum == devtools::proto::BackgroundService::UNKNOWN)
    return Response::InvalidParams("Invalid service name");

  if (should_record) {
    devtools_context_->StartRecording(service_enum);
    base::UmaHistogramEnumeration("DevTools.BackgroundService.StartRecording",
                                  service_enum, devtools::proto::COUNT);
  } else {
    devtools_context_->StopRecording(service_enum);
  }

  return Response::OK();
}

Response BackgroundServiceHandler::ClearEvents(const std::string& service) {
  DCHECK(devtools_context_);

  auto service_enum = ServiceNameToEnum(service);
  if (service_enum == devtools::proto::BackgroundService::UNKNOWN)
    return Response::InvalidParams("Invalid service name");

  devtools_context_->ClearLoggedBackgroundServiceEvents(service_enum);
  return Response::OK();
}

void BackgroundServiceHandler::OnEventReceived(
    const devtools::proto::BackgroundServiceEvent& event) {
  if (!enabled_services_.count(event.background_service()))
    return;

  frontend_->BackgroundServiceEventReceived(ToBackgroundServiceEvent(event));
}

void BackgroundServiceHandler::OnRecordingStateChanged(
    bool should_record,
    devtools::proto::BackgroundService service) {
  if (!enabled_services_.count(service))
    return;

  frontend_->RecordingStateChanged(should_record, ServiceEnumToName(service));
}

}  // namespace protocol
}  // namespace content
