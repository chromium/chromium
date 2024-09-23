// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/browser/service_worker/service_worker_internals_ui.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/browser/devtools/service_worker_devtools_agent_host.h"
#include "content/browser/devtools/service_worker_devtools_manager.h"
#include "content/browser/service_worker/service_worker_context_core_observer.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/grit/service_worker_resources.h"
#include "content/grit/service_worker_resources_map.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/console_message.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "third_party/blink/public/common/service_worker/embedded_worker_status.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"

using base::Value;
using base::WeakPtr;

namespace content {

namespace {

using GetRegistrationsCallback =
    base::OnceCallback<void(const std::vector<ServiceWorkerRegistrationInfo>&,
                            const std::vector<ServiceWorkerVersionInfo>&,
                            const std::vector<ServiceWorkerRegistrationInfo>&)>;

void OperationCompleteCallback(WeakPtr<ServiceWorkerInternalsHandler> internals,
                               const std::string& callback_id,
                               blink::ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (internals) {
    internals->OnOperationComplete(static_cast<int>(status), callback_id);
  }
}

base::ProcessId GetRealProcessId(int process_host_id) {
  if (process_host_id == ChildProcessHost::kInvalidUniqueID)
    return base::kNullProcessId;

  RenderProcessHost* rph = RenderProcessHost::FromID(process_host_id);
  if (!rph)
    return base::kNullProcessId;

  base::ProcessHandle handle = rph->GetProcess().Handle();
  if (handle == base::kNullProcessHandle)
    return base::kNullProcessId;
  // TODO(nhiroki): On Windows, |rph->GetHandle()| does not duplicate ownership
  // of the process handle and the render host still retains it. Therefore, we
  // cannot create a base::Process object, which provides a proper way to get a
  // process id, from the handle. For a stopgap, we use this deprecated
  // function that does not require the ownership (http://crbug.com/417532).
  return base::GetProcId(handle);
}

base::Value::Dict UpdateVersionInfo(const ServiceWorkerVersionInfo& version) {
  base::Value::Dict info;
  switch (version.running_status) {
    case blink::EmbeddedWorkerStatus::kStopped:
      info.Set("running_status", "STOPPED");
      break;
    case blink::EmbeddedWorkerStatus::kStarting:
      info.Set("running_status", "STARTING");
      break;
    case blink::EmbeddedWorkerStatus::kRunning:
      info.Set("running_status", "RUNNING");
      break;
    case blink::EmbeddedWorkerStatus::kStopping:
      info.Set("running_status", "STOPPING");
      break;
  }

  switch (version.status) {
    case ServiceWorkerVersion::NEW:
      info.Set("status", "NEW");
      break;
    case ServiceWorkerVersion::INSTALLING:
      info.Set("status", "INSTALLING");
      break;
    case ServiceWorkerVersion::INSTALLED:
      info.Set("status", "INSTALLED");
      break;
    case ServiceWorkerVersion::ACTIVATING:
      info.Set("status", "ACTIVATING");
      break;
    case ServiceWorkerVersion::ACTIVATED:
      info.Set("status", "ACTIVATED");
      break;
    case ServiceWorkerVersion::REDUNDANT:
      info.Set("status", "REDUNDANT");
      break;
  }

  if (version.fetch_handler_type) {
    switch (*version.fetch_handler_type) {
      case ServiceWorkerVersion::FetchHandlerType::kNoHandler:
        info.Set("fetch_handler_existence", "DOES_NOT_EXIST");
        info.Set("fetch_handler_type", "NO_HANDLER");
        break;
      case ServiceWorkerVersion::FetchHandlerType::kNotSkippable:
        info.Set("fetch_handler_existence", "EXISTS");
        info.Set("fetch_handler_type", "NOT_SKIPPABLE");
        break;
      case ServiceWorkerVersion::FetchHandlerType::kEmptyFetchHandler:
        info.Set("fetch_handler_existence", "EXISTS");
        info.Set("fetch_handler_type", "EMPTY_FETCH_HANDLER");
        break;
    }
  } else {
    info.Set("fetch_handler_existence", "UNKNOWN");
    info.Set("fetch_handler_type", "UNKNOWN");
  }

  if (version.router_rules) {
    info.Set("router_rules", *version.router_rules);
  }

  info.Set("script_url", version.script_url.spec());
  info.Set("version_id", base::NumberToString(version.version_id));
  info.Set("process_id",
           static_cast<int>(GetRealProcessId(version.process_id)));
  info.Set("process_host_id", version.process_id);
  info.Set("thread_id", version.thread_id);
  info.Set("devtools_agent_route_id", version.devtools_agent_route_id);

  base::Value::List clients;
  for (auto& it : version.clients) {
    base::Value::Dict client;
    client.Set("client_id", it.first);
    if (absl::holds_alternative<GlobalRenderFrameHostId>(it.second)) {
      RenderFrameHost* render_frame_host = RenderFrameHost::FromID(
          absl::get<GlobalRenderFrameHostId>(it.second));
      if (render_frame_host) {
        client.Set("url", render_frame_host->GetLastCommittedURL().spec());
      }
    }
    clients.Append(std::move(client));
  }
  info.Set("clients", std::move(clients));
  return info;
}

base::Value::List GetRegistrationListValue(
    const std::vector<ServiceWorkerRegistrationInfo>& registrations) {
  base::Value::List result;
  for (const auto& registration : registrations) {
    base::Value::Dict registration_info;
    registration_info.Set("scope", registration.scope.spec());
    registration_info.Set(
        "third_party_storage_partitioning_enabled",
        registration.key.IsThirdPartyStoragePartitioningEnabled());
    registration_info.Set("ancestor_chain_bit",
                          registration.key.ancestor_chain_bit() ==
                                  blink::mojom::AncestorChainBit::kCrossSite
                              ? "CrossSite"
                              : "SameSite");
    registration_info.Set("nonce", registration.key.nonce().has_value()
                                       ? registration.key.nonce()->ToString()
                                       : "<null>");
    registration_info.Set("origin", registration.key.origin().GetDebugString());
    registration_info.Set("top_level_site",
                          registration.key.top_level_site().Serialize());
    registration_info.Set("storage_key", registration.key.Serialize());
    registration_info.Set("registration_id",
                          base::NumberToString(registration.registration_id));
    registration_info.Set("navigation_preload_enabled",
                          registration.navigation_preload_enabled);
    registration_info.Set(
        "navigation_preload_header_length",
        static_cast<int>(registration.navigation_preload_header_length));

    if (registration.active_version.version_id !=
        blink::mojom::kInvalidServiceWorkerVersionId) {
      registration_info.Set("active",
                            UpdateVersionInfo(registration.active_version));
    }

    if (registration.waiting_version.version_id !=
        blink::mojom::kInvalidServiceWorkerVersionId) {
      registration_info.Set("waiting",
                            UpdateVersionInfo(registration.waiting_version));
    }

    result.Append(std::move(registration_info));
  }
  return result;
}

base::Value::List GetVersionListValue(
    const std::vector<ServiceWorkerVersionInfo>& versions) {
  base::Value::List result;
  for (const auto& version : versions) {
    result.Append(UpdateVersionInfo(version));
  }
  return result;
}

void DidGetStoredRegistrations(
    scoped_refptr<ServiceWorkerContextWrapper> context,
    GetRegistrationsCallback callback,
    blink::ServiceWorkerStatusCode status,
    const std::vector<ServiceWorkerRegistrationInfo>& stored_registrations) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::move(callback).Run(context->GetAllLiveRegistrationInfo(),
                          context->GetAllLiveVersionInfo(),
                          stored_registrations);
}

}  // namespace

class ServiceWorkerInternalsHandler::PartitionObserver
    : public ServiceWorkerContextCoreObserver {
 public:
  PartitionObserver(int partition_id,
                    WeakPtr<ServiceWorkerInternalsHandler> handler)
      : partition_id_(partition_id), handler_(handler) {}
  ~PartitionObserver() override {
    if (handler_) {
      // We need to remove PartitionObserver from the list of
      // ServiceWorkerContextCoreObserver.
      scoped_refptr<ServiceWorkerContextWrapper> context;
      if (handler_->GetServiceWorkerContext(partition_id_, &context))
        context->RemoveObserver(this);
    }
  }
  // ServiceWorkerContextCoreObserver overrides:
  void OnStarting(int64_t version_id) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (handler_) {
      handler_->OnRunningStateChanged();
    }
  }
  void OnStarted(int64_t version_id,
                 const GURL& scope,
                 int process_id,
                 const GURL& script_url,
                 const blink::ServiceWorkerToken& token,
                 const blink::StorageKey& key) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (handler_) {
      handler_->OnRunningStateChanged();
    }
  }
  void OnStopping(int64_t version_id) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (handler_) {
      handler_->OnRunningStateChanged();
    }
  }
  void OnStopped(int64_t version_id) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (handler_) {
      handler_->OnRunningStateChanged();
    }
  }
  void OnVersionStateChanged(int64_t version_id,
                             const GURL& scope,
                             const blink::StorageKey& key,
                             ServiceWorkerVersion::Status) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (handler_) {
      handler_->OnVersionStateChanged(partition_id_, version_id);
    }
  }
  void OnVersionRouterRulesChanged(int64_t, const std::string&) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (handler_) {
      handler_->OnVersionRouterRulesChanged();
    }
  }
  void OnErrorReported(
      int64_t version_id,
      const GURL& scope,
      const blink::StorageKey& key,
      const ServiceWorkerContextObserver::ErrorInfo& info) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (!handler_) {
      return;
    }
    base::Value::Dict details;
    details.Set("message", info.error_message);
    details.Set("lineNumber", info.line_number);
    details.Set("columnNumber", info.column_number);
    details.Set("sourceURL", info.source_url.spec());

    handler_->OnErrorEvent("error-reported", partition_id_, version_id,
                           details);
  }
  void OnReportConsoleMessage(int64_t version_id,
                              const GURL& scope,
                              const blink::StorageKey& key,
                              const ConsoleMessage& message) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (!handler_) {
      return;
    }
    base::Value::Dict details;
    details.Set("sourceIdentifier", static_cast<int>(message.source));
    details.Set("message_level", static_cast<int>(message.message_level));
    details.Set("message", message.message);
    details.Set("lineNumber", message.line_number);
    details.Set("sourceURL", message.source_url.spec());
    handler_->OnErrorEvent("console-message-reported", partition_id_,
                           version_id, details);
  }
  void OnRegistrationCompleted(int64_t registration_id,
                               const GURL& scope,
                               const blink::StorageKey& key) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (handler_) {
      handler_->OnRegistrationEvent("registration-completed", scope);
    }
  }
  void OnRegistrationDeleted(int64_t registration_id,
                             const GURL& scope,
                             const blink::StorageKey& key) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (handler_) {
      handler_->OnRegistrationEvent("registration-deleted", scope);
    }
  }
  int partition_id() const { return partition_id_; }

 private:
  const int partition_id_;
  WeakPtr<ServiceWorkerInternalsHandler> handler_;
};

ServiceWorkerInternalsUI::ServiceWorkerInternalsUI(WebUI* web_ui)
    : WebUIController(web_ui) {
  WebUIDataSource* source = WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      kChromeUIServiceWorkerInternalsHost);
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources 'self' 'unsafe-eval';");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types jstemplate;");
  source->UseStringsJs();
  source->AddResourcePaths(
      base::make_span(kServiceWorkerResources, kServiceWorkerResourcesSize));
  source->SetDefaultResource(IDR_SERVICE_WORKER_SERVICEWORKER_INTERNALS_HTML);

  source->DisableDenyXFrameOptions();

  web_ui->AddMessageHandler(std::make_unique<ServiceWorkerInternalsHandler>());
}

ServiceWorkerInternalsUI::~ServiceWorkerInternalsUI() = default;

ServiceWorkerInternalsHandler::ServiceWorkerInternalsHandler() = default;

void ServiceWorkerInternalsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "GetOptions",
      base::BindRepeating(&ServiceWorkerInternalsHandler::HandleGetOptions,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "SetOption",
      base::BindRepeating(&ServiceWorkerInternalsHandler::HandleSetOption,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getAllRegistrations",
      base::BindRepeating(
          &ServiceWorkerInternalsHandler::HandleGetAllRegistrations,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "stop",
      base::BindRepeating(&ServiceWorkerInternalsHandler::HandleStopWorker,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "inspect",
      base::BindRepeating(&ServiceWorkerInternalsHandler::HandleInspectWorker,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "unregister",
      base::BindRepeating(&ServiceWorkerInternalsHandler::HandleUnregister,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "start",
      base::BindRepeating(&ServiceWorkerInternalsHandler::HandleStartWorker,
                          base::Unretained(this)));
}

void ServiceWorkerInternalsHandler::OnJavascriptDisallowed() {
  BrowserContext* browser_context =
      web_ui()->GetWebContents()->GetBrowserContext();
  // Safe to use base::Unretained(this) because ForEachLoadedStoragePartition is
  // synchronous.
  browser_context->ForEachLoadedStoragePartition(
      [this](StoragePartition* partition) {
        RemoveObserverFromStoragePartition(partition);
      });
  weak_ptr_factory_.InvalidateWeakPtrs();
}

ServiceWorkerInternalsHandler::~ServiceWorkerInternalsHandler() {
  // ServiceWorkerInternalsHandler can be destroyed without
  // OnJavascriptDisallowed() ever being called (https://crbug.com/1199198).
  // Call it to ensure that `this` is removed as an observer.
  OnJavascriptDisallowed();
}

void ServiceWorkerInternalsHandler::OnRunningStateChanged() {
  FireWebUIListener("running-state-changed");
}

void ServiceWorkerInternalsHandler::OnVersionStateChanged(int partition_id,
                                                          int64_t version_id) {
  FireWebUIListener("version-state-changed", base::Value(partition_id),
                    base::Value(base::NumberToString(version_id)));
}

void ServiceWorkerInternalsHandler::OnVersionRouterRulesChanged() {
  FireWebUIListener("version-router-rules-changed");
}

void ServiceWorkerInternalsHandler::OnErrorEvent(
    const std::string& event_name,
    int partition_id,
    int64_t version_id,
    const base::Value::Dict& details) {
  FireWebUIListener(event_name, base::Value(partition_id),
                    base::Value(base::NumberToString(version_id)), details);
}

void ServiceWorkerInternalsHandler::OnRegistrationEvent(
    const std::string& event_name,
    const GURL& scope) {
  FireWebUIListener(event_name, base::Value(scope.spec()));
}

void ServiceWorkerInternalsHandler::OnDidGetRegistrations(
    int partition_id,
    const base::FilePath& context_path,
    const std::vector<ServiceWorkerRegistrationInfo>& live_registrations,
    const std::vector<ServiceWorkerVersionInfo>& live_versions,
    const std::vector<ServiceWorkerRegistrationInfo>& stored_registrations) {
  base::Value::Dict registrations;
  registrations.Set("liveRegistrations",
                    GetRegistrationListValue(live_registrations));
  registrations.Set("liveVersions", GetVersionListValue(live_versions));
  registrations.Set("storedRegistrations",
                    GetRegistrationListValue(stored_registrations));
  FireWebUIListener("partition-data", registrations, base::Value(partition_id),
                    base::Value(context_path.AsUTF8Unsafe()));
}

void ServiceWorkerInternalsHandler::OnOperationComplete(
    int status,
    const std::string& callback_id) {
  ResolveJavascriptCallback(base::Value(callback_id), base::Value(status));
}

void ServiceWorkerInternalsHandler::HandleGetOptions(const Value::List& args) {
  CHECK(args.size() != 0);
  CHECK(args[0].is_string());
  std::string callback_id = args[0].GetString();
  AllowJavascript();
  base::Value::Dict options;
  options.Set("debug_on_start", ServiceWorkerDevToolsManager::GetInstance()
                                    ->debug_service_worker_on_start());
  ResolveJavascriptCallback(base::Value(callback_id), options);
}

void ServiceWorkerInternalsHandler::HandleSetOption(
    const Value::List& args_list) {
  if (args_list.size() < 2) {
    return;
  }

  if (!args_list[0].is_string()) {
    return;
  }
  if (args_list[0].GetString() != "debug_on_start") {
    return;
  }

  if (!args_list[1].is_bool()) {
    return;
  }
  ServiceWorkerDevToolsManager::GetInstance()
      ->set_debug_service_worker_on_start(args_list[1].GetBool());
}

void ServiceWorkerInternalsHandler::HandleGetAllRegistrations(
    const Value::List& args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Allow Javascript here too, because these messages are sent back to back.
  AllowJavascript();
  BrowserContext* browser_context =
      web_ui()->GetWebContents()->GetBrowserContext();
  browser_context->ForEachLoadedStoragePartition(
      [this](StoragePartition* partition) {
        AddContextFromStoragePartition(partition);
      });
}

void ServiceWorkerInternalsHandler::AddContextFromStoragePartition(
    StoragePartition* partition) {
  int partition_id = 0;
  scoped_refptr<ServiceWorkerContextWrapper> context =
      static_cast<ServiceWorkerContextWrapper*>(
          partition->GetServiceWorkerContext());
  auto it = observers_.find(reinterpret_cast<uintptr_t>(partition));
  if (it != observers_.end()) {
    partition_id = it->second->partition_id();
  } else {
    partition_id = next_partition_id_++;
    auto new_observer = std::make_unique<PartitionObserver>(
        partition_id, weak_ptr_factory_.GetWeakPtr());
    context->AddObserver(new_observer.get());
    observers_[reinterpret_cast<uintptr_t>(partition)] =
        std::move(new_observer);
  }

  context->GetAllRegistrations(base::BindOnce(
      &DidGetStoredRegistrations, context,
      base::BindOnce(
          &ServiceWorkerInternalsHandler::OnDidGetRegistrations,
          weak_ptr_factory_.GetWeakPtr(), partition_id,
          context->is_incognito() ? base::FilePath() : partition->GetPath())));
}

void ServiceWorkerInternalsHandler::RemoveObserverFromStoragePartition(
    StoragePartition* partition) {
  auto it = observers_.find(reinterpret_cast<uintptr_t>(partition));
  if (it == observers_.end())
    return;
  std::unique_ptr<PartitionObserver> observer = std::move(it->second);
  observers_.erase(it);
  scoped_refptr<ServiceWorkerContextWrapper> context =
      static_cast<ServiceWorkerContextWrapper*>(
          partition->GetServiceWorkerContext());
  context->RemoveObserver(observer.get());
}

bool ServiceWorkerInternalsHandler::GetServiceWorkerContext(
    int partition_id,
    scoped_refptr<ServiceWorkerContextWrapper>* context) {
  BrowserContext* browser_context =
      web_ui()->GetWebContents()->GetBrowserContext();
  StoragePartition* result_partition(nullptr);
  browser_context->ForEachLoadedStoragePartition(
      [&](StoragePartition* partition) {
        auto it = observers_.find(reinterpret_cast<uintptr_t>(partition));
        if (it != observers_.end() &&
            partition_id == it->second->partition_id()) {
          result_partition = partition;
        }
      });
  if (!result_partition)
    return false;
  *context = static_cast<ServiceWorkerContextWrapper*>(
      result_partition->GetServiceWorkerContext());
  return true;
}

void ServiceWorkerInternalsHandler::HandleStopWorker(const Value::List& args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (args.size() < 2 || !args[0].is_string())
    return;
  std::string callback_id = args[0].GetString();

  if (!args[1].is_dict())
    return;
  const base::Value::Dict& cmd_args = args[1].GetDict();

  std::optional<int> partition_id = cmd_args.FindInt("partition_id");
  scoped_refptr<ServiceWorkerContextWrapper> context;
  int64_t version_id = 0;
  const std::string* version_id_string = cmd_args.FindString("version_id");
  if (!partition_id || !GetServiceWorkerContext(*partition_id, &context) ||
      !version_id_string ||
      !base::StringToInt64(*version_id_string, &version_id)) {
    return;
  }

  base::OnceCallback<void(blink::ServiceWorkerStatusCode)> callback =
      base::BindOnce(OperationCompleteCallback, weak_ptr_factory_.GetWeakPtr(),
                     callback_id);
  StopWorkerWithId(context, version_id, std::move(callback));
}

void ServiceWorkerInternalsHandler::HandleInspectWorker(
    const Value::List& args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (args.size() < 2 || !args[0].is_string())
    return;
  std::string callback_id = args[0].GetString();

  if (!args[1].is_dict())
    return;
  const base::Value::Dict& cmd_args = args[1].GetDict();

  std::optional<int> process_host_id = cmd_args.FindInt("process_host_id");
  std::optional<int> devtools_agent_route_id =
      cmd_args.FindInt("devtools_agent_route_id");
  if (!process_host_id || !devtools_agent_route_id) {
    return;
  }
  base::OnceCallback<void(blink::ServiceWorkerStatusCode)> callback =
      base::BindOnce(OperationCompleteCallback, weak_ptr_factory_.GetWeakPtr(),
                     callback_id);
  scoped_refptr<ServiceWorkerDevToolsAgentHost> agent_host(
      ServiceWorkerDevToolsManager::GetInstance()
          ->GetDevToolsAgentHostForWorker(*process_host_id,
                                          *devtools_agent_route_id));
  if (!agent_host.get()) {
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kErrorNotFound);
    return;
  }
  agent_host->Inspect();
  std::move(callback).Run(blink::ServiceWorkerStatusCode::kOk);
}

void ServiceWorkerInternalsHandler::HandleUnregister(const Value::List& args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (args.size() < 2 || !args[0].is_string())
    return;
  std::string callback_id = args[0].GetString();

  if (!args[1].is_dict())
    return;
  const base::Value::Dict& cmd_args = args[1].GetDict();

  std::optional<int> partition_id = cmd_args.FindInt("partition_id");
  scoped_refptr<ServiceWorkerContextWrapper> context;
  const std::string* scope_string = cmd_args.FindString("scope");
  const std::string* storage_key_string = cmd_args.FindString("storage_key");
  if (!partition_id || !GetServiceWorkerContext(*partition_id, &context) ||
      !scope_string || !storage_key_string) {
    return;
  }

  std::optional<blink::StorageKey> storage_key =
      blink::StorageKey::Deserialize(*storage_key_string);
  if (!storage_key) {
    return;
  }

  base::OnceCallback<void(blink::ServiceWorkerStatusCode)> callback =
      base::BindOnce(OperationCompleteCallback, weak_ptr_factory_.GetWeakPtr(),
                     callback_id);
  UnregisterWithScope(context, GURL(*scope_string), storage_key.value(),
                      std::move(callback));
}

void ServiceWorkerInternalsHandler::HandleStartWorker(const Value::List& args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (args.size() < 2 || !args[0].is_string())
    return;
  std::string callback_id = args[0].GetString();

  if (!args[1].is_dict())
    return;
  const base::Value::Dict& cmd_args = args[1].GetDict();

  std::optional<int> partition_id = cmd_args.FindInt("partition_id");
  scoped_refptr<ServiceWorkerContextWrapper> context;
  const std::string* scope_string = cmd_args.FindString("scope");
  const std::string* storage_key_string = cmd_args.FindString("storage_key");
  if (!partition_id || !GetServiceWorkerContext(*partition_id, &context) ||
      !scope_string || !storage_key_string) {
    return;
  }

  std::optional<blink::StorageKey> storage_key =
      blink::StorageKey::Deserialize(*storage_key_string);
  if (!storage_key) {
    return;
  }

  base::OnceCallback<void(blink::ServiceWorkerStatusCode)> callback =
      base::BindOnce(OperationCompleteCallback, weak_ptr_factory_.GetWeakPtr(),
                     callback_id);
  context->StartActiveServiceWorker(GURL(*scope_string), storage_key.value(),
                                    std::move(callback));
}

void ServiceWorkerInternalsHandler::StopWorkerWithId(
    scoped_refptr<ServiceWorkerContextWrapper> context,
    int64_t version_id,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  scoped_refptr<ServiceWorkerVersion> version =
      context->GetLiveVersion(version_id);
  if (!version.get()) {
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kErrorNotFound);
    return;
  }

  // ServiceWorkerVersion::StopWorker() takes a base::OnceClosure for argument,
  // so bind blink::ServiceWorkerStatusCode::kOk to callback here.
  version->StopWorker(
      base::BindOnce(std::move(callback), blink::ServiceWorkerStatusCode::kOk));
}

void ServiceWorkerInternalsHandler::UnregisterWithScope(
    scoped_refptr<ServiceWorkerContextWrapper> context,
    const GURL& scope,
    blink::StorageKey& storage_key,
    ServiceWorkerInternalsHandler::StatusCallback callback) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!context->context()) {
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kErrorAbort);
    return;
  }

  // ServiceWorkerContextWrapper::UnregisterServiceWorker doesn't work here
  // because that reduces a status code to boolean.
  context->context()->UnregisterServiceWorker(scope, storage_key,
                                              /*is_immediate=*/false,
                                              std::move(callback));
}

}  // namespace content
