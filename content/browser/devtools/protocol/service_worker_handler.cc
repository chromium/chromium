// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/service_worker_handler.h"

#include <memory>
#include <variant>

#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/notimplemented.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/background_sync/background_sync_context_impl.h"
#include "content/browser/background_sync/background_sync_manager.h"
#include "content/browser/devtools/protocol/service_worker.h"
#include "content/browser/devtools/service_worker_devtools_agent_host.h"
#include "content/browser/devtools/service_worker_devtools_manager.h"
#include "content/browser/devtools/shared_worker_devtools_manager.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/service_worker/service_worker_context_watcher.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/storage_partition_impl_map.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/cpp/request_destination.h"
#include "services/network/public/cpp/request_mode.h"
#include "third_party/blink/public/common/service_worker/embedded_worker_status.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/push_messaging/push_messaging_status.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content::protocol {

namespace {

const std::string GetVersionRunningStatusString(
    blink::EmbeddedWorkerStatus running_status) {
  switch (running_status) {
    case blink::EmbeddedWorkerStatus::kStopped:
      return ServiceWorker::ServiceWorkerVersionRunningStatusEnum::Stopped;
    case blink::EmbeddedWorkerStatus::kStarting:
      return ServiceWorker::ServiceWorkerVersionRunningStatusEnum::Starting;
    case blink::EmbeddedWorkerStatus::kRunning:
      return ServiceWorker::ServiceWorkerVersionRunningStatusEnum::Running;
    case blink::EmbeddedWorkerStatus::kStopping:
      return ServiceWorker::ServiceWorkerVersionRunningStatusEnum::Stopping;
    default:
      NOTREACHED();
  }
}

const std::string GetVersionStatusString(
    content::ServiceWorkerVersion::Status status) {
  switch (status) {
    case content::ServiceWorkerVersion::NEW:
      return ServiceWorker::ServiceWorkerVersionStatusEnum::New;
    case content::ServiceWorkerVersion::INSTALLING:
      return ServiceWorker::ServiceWorkerVersionStatusEnum::Installing;
    case content::ServiceWorkerVersion::INSTALLED:
      return ServiceWorker::ServiceWorkerVersionStatusEnum::Installed;
    case content::ServiceWorkerVersion::ACTIVATING:
      return ServiceWorker::ServiceWorkerVersionStatusEnum::Activating;
    case content::ServiceWorkerVersion::ACTIVATED:
      return ServiceWorker::ServiceWorkerVersionStatusEnum::Activated;
    case content::ServiceWorkerVersion::REDUNDANT:
      return ServiceWorker::ServiceWorkerVersionStatusEnum::Redundant;
    default:
      NOTREACHED();
  }
}

std::unique_ptr<protocol::ServiceWorker::ServiceWorkerRouterSource>
ConvertRouterSource(const blink::ServiceWorkerRouterSource& source) {
  switch (source.type) {
    case network::mojom::ServiceWorkerRouterSourceType::kNetwork:
      return protocol::ServiceWorker::ServiceWorkerRouterSource::Create()
          .SetType(protocol::ServiceWorker::ServiceWorkerRouterSourceTypeEnum::
                       Network)
          .Build();
    case network::mojom::ServiceWorkerRouterSourceType::
        kRaceNetworkAndFetchEvent:
      return protocol::ServiceWorker::ServiceWorkerRouterSource::Create()
          .SetType(protocol::ServiceWorker::ServiceWorkerRouterSourceTypeEnum::
                       RaceNetworkAndFetchHandler)
          .Build();
    case network::mojom::ServiceWorkerRouterSourceType::kFetchEvent:
      return protocol::ServiceWorker::ServiceWorkerRouterSource::Create()
          .SetType(protocol::ServiceWorker::ServiceWorkerRouterSourceTypeEnum::
                       FetchEvent)
          .Build();
    case network::mojom::ServiceWorkerRouterSourceType::kCache:
      if (source.cache_source && source.cache_source->cache_name) {
        return protocol::ServiceWorker::ServiceWorkerRouterSource::Create()
            .SetType(protocol::ServiceWorker::
                         ServiceWorkerRouterSourceTypeEnum::SourceDict)
            .SetSourceDict(
                protocol::ServiceWorker::ServiceWorkerRouterSourceDict::Create()
                    .SetCacheName(*source.cache_source->cache_name)
                    .Build())
            .Build();
      } else {
        return protocol::ServiceWorker::ServiceWorkerRouterSource::Create()
            .SetType(protocol::ServiceWorker::
                         ServiceWorkerRouterSourceTypeEnum::Cache)
            .Build();
      }
    case network::mojom::ServiceWorkerRouterSourceType::kRaceNetworkAndCache:
      return protocol::ServiceWorker::ServiceWorkerRouterSource::Create()
          .SetType(protocol::ServiceWorker::ServiceWorkerRouterSourceTypeEnum::
                       RaceNetworkAndCache)
          .Build();
  }
  NOTREACHED();
}

// Converts blink internal RouterCondition type to that of CDP.
// `c` shall not have nested conditions(`or` and `not`) because they are not yet
// supported on CDP.
// TODO(crbug.com/540469610): Support nested conditions.
std::unique_ptr<protocol::ServiceWorker::ServiceWorkerRouterCondition>
ConvertRouterCondition(const blink::ServiceWorkerRouterCondition& c) {
  auto condition =
      protocol::ServiceWorker::ServiceWorkerRouterCondition::Create().Build();
  const auto& [url_pattern, request, running_status, or_condition,
               not_condition] = c.get();
  DCHECK(!or_condition.has_value() && !not_condition.has_value())
      << "nested conditions are not yet supported.";
  if (url_pattern) {
    condition->SetUrlPattern(SafeURLPatternToString(*url_pattern));
  }
  if (request) {
    if (request->method) {
      condition->SetRequestMethod(*request->method);
    }
    if (request->mode) {
      condition->SetRequestMode(network::RequestModeToString(*request->mode));
    }
    if (request->destination) {
      condition->SetRequestDestination(
          network::RequestDestinationToString(*request->destination));
    }
  }
  if (running_status) {
    switch (running_status->status) {
      case blink::ServiceWorkerRouterRunningStatusCondition::RunningStatusEnum::
          kRunning:
        condition->SetRunningStatus(
            protocol::ServiceWorker::ServiceWorkerVersionRunningStatusEnum::
                Running);
        break;
      case blink::ServiceWorkerRouterRunningStatusCondition::RunningStatusEnum::
          kNotRunning:
        condition->SetRunningStatus(
            protocol::ServiceWorker::ServiceWorkerVersionRunningStatusEnum::
                Stopped);
        break;
    }
  }
  return condition;
}

std::unique_ptr<
    protocol::Array<protocol::ServiceWorker::ServiceWorkerRouterRule>>
ConvertTypedRouterRules(
    const std::vector<content::ServiceWorkerRouterRule>& rules) {
  auto typed_router_rules = std::make_unique<
      protocol::Array<protocol::ServiceWorker::ServiceWorkerRouterRule>>();
  for (const auto& rule : rules) {
    auto protocol_rule =
        protocol::ServiceWorker::ServiceWorkerRouterRule::Create()
            .SetCondition(ConvertRouterCondition(rule.condition))
            .SetSource(ConvertRouterSource(rule.source))
            .SetId(rule.id)
            .Build();
    typed_router_rules->emplace_back(std::move(protocol_rule));
  }
  return typed_router_rules;
}

Response CreateDomainNotEnabledErrorResponse() {
  return Response::ServerError("ServiceWorker domain not enabled");
}

Response CreateContextErrorResponse() {
  return Response::ServerError("Could not connect to the context");
}

Response CreateInvalidVersionIdErrorResponse() {
  return Response::InvalidParams("Invalid version ID");
}

void DidFindRegistrationForDispatchSyncEvent(
    scoped_refptr<BackgroundSyncContextImpl> sync_context,
    const std::string& tag,
    bool last_chance,
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<content::ServiceWorkerRegistration> registration) {
  if (status != blink::ServiceWorkerStatusCode::kOk ||
      !registration->active_version())
    return;
  BackgroundSyncManager* background_sync_manager =
      sync_context->background_sync_manager();
  scoped_refptr<content::ServiceWorkerVersion> version(
      registration->active_version());
  // Keep the registration while dispatching the sync event.
  background_sync_manager->EmulateDispatchSyncEvent(
      tag, std::move(version), last_chance, base::DoNothing());
}

void DidFindRegistrationForDispatchPeriodicSyncEvent(
    scoped_refptr<BackgroundSyncContextImpl> sync_context,
    const std::string& tag,
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<content::ServiceWorkerRegistration> registration) {
  if (status != blink::ServiceWorkerStatusCode::kOk ||
      !registration->active_version()) {
    return;
  }

  BackgroundSyncManager* background_sync_manager =
      sync_context->background_sync_manager();
  scoped_refptr<content::ServiceWorkerVersion> version(
      registration->active_version());
  // Keep the registration while dispatching the sync event.
  background_sync_manager->EmulateDispatchPeriodicSyncEvent(
      tag, std::move(version), base::DoNothing());
}

}  // namespace

ServiceWorkerHandler::ServiceWorkerHandler()
    : DevToolsDomainHandler(ServiceWorker::Metainfo::domainName),
      enabled_(false),
      browser_context_(nullptr),
      storage_partition_(nullptr) {}

ServiceWorkerHandler::~ServiceWorkerHandler() = default;

void ServiceWorkerHandler::Wire(UberDispatcher* dispatcher) {
  frontend_ = std::make_unique<ServiceWorker::Frontend>(dispatcher->channel());
  ServiceWorker::Dispatcher::wire(dispatcher, this);
}

void ServiceWorkerHandler::SetRenderer(int process_host_id,
                                       RenderFrameHostImpl* frame_host) {
  RenderProcessHost* process_host = RenderProcessHost::FromID(process_host_id);
  // Do not call UpdateHosts yet, wait for load to commit.
  if (!process_host) {
    ClearForceUpdate();
    context_ = nullptr;
    return;
  }

  storage_partition_ =
      static_cast<StoragePartitionImpl*>(process_host->GetStoragePartition());
  CHECK(storage_partition_, base::NotFatalUntil::M159);
  browser_context_ = process_host->GetBrowserContext();
  context_ = static_cast<ServiceWorkerContextWrapper*>(
      storage_partition_->GetServiceWorkerContext());
}

Response ServiceWorkerHandler::Enable() {
  if (enabled_)
    return Response::Success();
  if (!context_)
    return CreateContextErrorResponse();
  enabled_ = true;

  context_watcher_ = base::MakeRefCounted<ServiceWorkerContextWatcher>(
      context_,
      base::BindRepeating(&ServiceWorkerHandler::OnWorkerRegistrationUpdated,
                          weak_factory_.GetWeakPtr()),
      base::BindRepeating(&ServiceWorkerHandler::OnWorkerVersionUpdated,
                          weak_factory_.GetWeakPtr()),
      base::BindRepeating(&ServiceWorkerHandler::OnErrorReported,
                          weak_factory_.GetWeakPtr()));
  context_watcher_->Start();

  return Response::Success();
}

Response ServiceWorkerHandler::Disable() {
  if (!enabled_)
    return Response::Success();
  enabled_ = false;

  ClearForceUpdate();
  CHECK(context_watcher_, base::NotFatalUntil::M159);
  context_watcher_->Stop();
  context_watcher_ = nullptr;
  return Response::Success();
}

Response ServiceWorkerHandler::Unregister(const std::string& scope_url) {
  if (!enabled_)
    return CreateDomainNotEnabledErrorResponse();
  if (!context_)
    return CreateContextErrorResponse();
  GURL url(scope_url);
  const blink::StorageKey key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(url));
  context_->UnregisterServiceWorker(url, key, base::DoNothing());
  return Response::Success();
}

Response ServiceWorkerHandler::StartWorker(const std::string& scope_url) {
  if (!enabled_)
    return CreateDomainNotEnabledErrorResponse();
  if (!context_)
    return CreateContextErrorResponse();
  context_->StartActiveServiceWorker(
      GURL(scope_url),
      blink::StorageKey::CreateFirstParty(url::Origin::Create(GURL(scope_url))),
      base::DoNothing());
  return Response::Success();
}

Response ServiceWorkerHandler::SkipWaiting(const std::string& scope_url) {
  if (!enabled_)
    return CreateDomainNotEnabledErrorResponse();
  if (!context_)
    return CreateContextErrorResponse();
  context_->SkipWaitingWorker(GURL(scope_url),
                              blink::StorageKey::CreateFirstParty(
                                  url::Origin::Create(GURL(scope_url))));
  return Response::Success();
}

Response ServiceWorkerHandler::StopWorker(const std::string& version_id) {
  if (!enabled_)
    return CreateDomainNotEnabledErrorResponse();
  if (!context_)
    return CreateContextErrorResponse();
  int64_t id = 0;
  if (!base::StringToInt64(version_id, &id))
    return CreateInvalidVersionIdErrorResponse();

  if (content::ServiceWorkerVersion* version = context_->GetLiveVersion(id)) {
    version->StopWorker(base::DoNothing());
  }

  return Response::Success();
}

void ServiceWorkerHandler::StopAllWorkers(
    std::unique_ptr<StopAllWorkersCallback> callback) {
  if (!enabled_) {
    callback->sendFailure(CreateDomainNotEnabledErrorResponse());
    return;
  }
  if (!context_) {
    callback->sendFailure(CreateContextErrorResponse());
    return;
  }
  context_->StopAllServiceWorkers(base::BindOnce(
      &StopAllWorkersCallback::sendSuccess, std::move(callback)));
}

Response ServiceWorkerHandler::UpdateRegistration(
    const std::string& scope_url) {
  if (!enabled_)
    return CreateDomainNotEnabledErrorResponse();
  if (!context_)
    return CreateContextErrorResponse();
  context_->UpdateRegistration(GURL(scope_url),
                               blink::StorageKey::CreateFirstParty(
                                   url::Origin::Create(GURL(scope_url))));
  return Response::Success();
}

Response ServiceWorkerHandler::SetForceUpdateOnPageLoad(
    bool force_update_on_page_load) {
  if (!context_)
    return CreateContextErrorResponse();
  context_->SetForceUpdateOnPageLoad(force_update_on_page_load);
  return Response::Success();
}

Response ServiceWorkerHandler::DeliverPushMessage(
    const std::string& origin,
    const std::string& registration_id,
    const std::string& data) {
  if (!enabled_)
    return CreateDomainNotEnabledErrorResponse();
  if (!browser_context_)
    return CreateContextErrorResponse();
  int64_t id = 0;
  if (!base::StringToInt64(registration_id, &id))
    return CreateInvalidVersionIdErrorResponse();
  std::optional<std::string> payload;
  if (data.size() > 0)
    payload = data;
  browser_context_->DeliverPushMessage(
      GURL(origin), id,
      /* message_id= */ std::string(), std::move(payload),
      /* record_network_requests=  */ false, base::DoNothing());

  return Response::Success();
}

Response ServiceWorkerHandler::DispatchSyncEvent(
    const std::string& origin,
    const std::string& registration_id,
    const std::string& tag,
    bool last_chance) {
  if (!enabled_)
    return CreateDomainNotEnabledErrorResponse();
  if (!storage_partition_)
    return CreateContextErrorResponse();
  int64_t id = 0;
  if (!base::StringToInt64(registration_id, &id))
    return CreateInvalidVersionIdErrorResponse();

  scoped_refptr<BackgroundSyncContextImpl> sync_context =
      base::WrapRefCounted(storage_partition_->GetBackgroundSyncContext());

  context_->FindReadyRegistrationForId(
      id,
      blink::StorageKey::CreateFirstParty(url::Origin::Create(GURL(origin))),
      base::BindOnce(&DidFindRegistrationForDispatchSyncEvent,
                     std::move(sync_context), tag, last_chance));

  return Response::Success();
}

Response ServiceWorkerHandler::DispatchPeriodicSyncEvent(
    const std::string& origin,
    const std::string& registration_id,
    const std::string& tag) {
  if (!enabled_)
    return CreateDomainNotEnabledErrorResponse();
  if (!storage_partition_)
    return CreateContextErrorResponse();
  int64_t id = 0;
  if (!base::StringToInt64(registration_id, &id))
    return CreateInvalidVersionIdErrorResponse();

  scoped_refptr<BackgroundSyncContextImpl> sync_context =
      base::WrapRefCounted(storage_partition_->GetBackgroundSyncContext());

  context_->FindReadyRegistrationForId(
      id,
      blink::StorageKey::CreateFirstParty(url::Origin::Create(GURL(origin))),
      base::BindOnce(&DidFindRegistrationForDispatchPeriodicSyncEvent,
                     std::move(sync_context), tag));

  return Response::Success();
}

void ServiceWorkerHandler::OnWorkerRegistrationUpdated(
    const std::vector<ServiceWorkerRegistrationInfo>& registrations) {
  using Registration = ServiceWorker::ServiceWorkerRegistration;
  auto result = std::make_unique<protocol::Array<Registration>>();
  for (const auto& registration : registrations) {
    result->emplace_back(
        Registration::Create()
            .SetRegistrationId(
                base::NumberToString(registration.registration_id))
            .SetScopeURL(registration.scope.spec())
            .SetIsDeleted(registration.delete_flag ==
                          ServiceWorkerRegistrationInfo::IS_DELETED)
            .Build());
  }
  frontend_->WorkerRegistrationUpdated(std::move(result));
}

void ServiceWorkerHandler::OnWorkerVersionUpdated(
    const std::vector<ServiceWorkerVersionInfo>& versions) {
  using Version = ServiceWorker::ServiceWorkerVersion;
  auto result = std::make_unique<protocol::Array<Version>>();
  for (const auto& version : versions) {
    base::flat_set<std::string> client_set;

    for (const auto& client : version.clients) {
      if (std::holds_alternative<GlobalRenderFrameHostId>(client.second)) {
        WebContents* web_contents = WebContentsImpl::FromRenderFrameHostID(
            std::get<GlobalRenderFrameHostId>(client.second));
        // There is a possibility that the frame is already deleted
        // because of the thread hopping.
        if (!web_contents)
          continue;
        client_set.insert(
            DevToolsAgentHost::GetOrCreateFor(web_contents)->GetId());
      }
    }
    auto clients = std::make_unique<protocol::Array<std::string>>();
    for (std::string& client : client_set)
      clients->emplace_back(std::move(client));

    std::unique_ptr<Version> version_value =
        Version::Create()
            .SetVersionId(base::NumberToString(version.version_id))
            .SetRegistrationId(base::NumberToString(version.registration_id))
            .SetScriptURL(version.script_url.spec())
            .SetRunningStatus(
                GetVersionRunningStatusString(version.running_status))
            .SetStatus(GetVersionStatusString(version.status))
            .SetScriptLastModified(
                version.script_last_modified.InSecondsFSinceUnixEpoch())
            .SetScriptResponseTime(
                version.script_response_time.InSecondsFSinceUnixEpoch())
            .SetControlledClients(std::move(clients))
            .Build();
    scoped_refptr<DevToolsAgentHostImpl> host(
        ServiceWorkerDevToolsManager::GetInstance()
            ->GetDevToolsAgentHostForWorker(version.process_id,
                                            version.devtools_agent_route_id));
    if (host) {
      version_value->SetTargetId(host->GetId());
    }
    if (version.router_rules) {
      version_value->SetRouterRules(*version.router_rules);
    }
    if (!version.typed_router_rules.empty()) {
      version_value->SetTypedRouterRules(
          ConvertTypedRouterRules(version.typed_router_rules));
    }
    result->emplace_back(std::move(version_value));
  }
  frontend_->WorkerVersionUpdated(std::move(result));
}

void ServiceWorkerHandler::OnErrorReported(
    int64_t registration_id,
    int64_t version_id,
    const ServiceWorkerContextObserver::ErrorInfo& info) {
  frontend_->WorkerErrorReported(
      ServiceWorker::ServiceWorkerErrorMessage::Create()
          .SetErrorMessage(base::UTF16ToUTF8(info.error_message))
          .SetRegistrationId(base::NumberToString(registration_id))
          .SetVersionId(base::NumberToString(version_id))
          .SetSourceURL(info.source_url.spec())
          .SetLineNumber(info.line_number)
          .SetColumnNumber(info.column_number)
          .Build());
}

void ServiceWorkerHandler::ClearForceUpdate() {
  if (context_)
    context_->SetForceUpdateOnPageLoad(false);
}

}  // namespace content::protocol
