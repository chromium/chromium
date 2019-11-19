// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_internals_ui.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/browser/devtools/service_worker_devtools_agent_host.h"
#include "content/browser/devtools/service_worker_devtools_manager.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/service_worker_context_core_observer.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/grit/content_resources.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/console_message.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/url_constants.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"

using base::DictionaryValue;
using base::ListValue;
using base::Value;
using base::WeakPtr;

namespace content {

namespace {

using GetRegistrationsCallback =
    base::OnceCallback<void(const std::vector<ServiceWorkerRegistrationInfo>&,
                            const std::vector<ServiceWorkerVersionInfo>&,
                            const std::vector<ServiceWorkerRegistrationInfo>&)>;

void OperationCompleteCallback(WeakPtr<ServiceWorkerInternalsUI> internals,
                               int callback_id,
                               blink::ServiceWorkerStatusCode status) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    base::PostTask(FROM_HERE, {BrowserThread::UI},
                   base::BindOnce(OperationCompleteCallback, internals,
                                  callback_id, status));
    return;
  }
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (internals) {
    internals->web_ui()->CallJavascriptFunctionUnsafe(
        "serviceworker.onOperationComplete", Value(static_cast<int>(status)),
        Value(callback_id));
  }
}

std::vector<const Value*> ConvertToRawPtrVector(
    const std::vector<std::unique_ptr<const Value>>& args) {
  std::vector<const Value*> args_rawptrs(args.size());
  std::transform(
      args.begin(), args.end(), args_rawptrs.begin(),
      [](const std::unique_ptr<const Value>& arg) { return arg.get(); });
  return args_rawptrs;
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

void UpdateVersionInfo(const ServiceWorkerVersionInfo& version,
                       DictionaryValue* info) {
  switch (version.running_status) {
    case EmbeddedWorkerStatus::STOPPED:
      info->SetString("running_status", "STOPPED");
      break;
    case EmbeddedWorkerStatus::STARTING:
      info->SetString("running_status", "STARTING");
      break;
    case EmbeddedWorkerStatus::RUNNING:
      info->SetString("running_status", "RUNNING");
      break;
    case EmbeddedWorkerStatus::STOPPING:
      info->SetString("running_status", "STOPPING");
      break;
  }

  switch (version.status) {
    case ServiceWorkerVersion::NEW:
      info->SetString("status", "NEW");
      break;
    case ServiceWorkerVersion::INSTALLING:
      info->SetString("status", "INSTALLING");
      break;
    case ServiceWorkerVersion::INSTALLED:
      info->SetString("status", "INSTALLED");
      break;
    case ServiceWorkerVersion::ACTIVATING:
      info->SetString("status", "ACTIVATING");
      break;
    case ServiceWorkerVersion::ACTIVATED:
      info->SetString("status", "ACTIVATED");
      break;
    case ServiceWorkerVersion::REDUNDANT:
      info->SetString("status", "REDUNDANT");
      break;
  }

  switch (version.fetch_handler_existence) {
    case ServiceWorkerVersion::FetchHandlerExistence::UNKNOWN:
      info->SetString("fetch_handler_existence", "UNKNOWN");
      break;
    case ServiceWorkerVersion::FetchHandlerExistence::EXISTS:
      info->SetString("fetch_handler_existence", "EXISTS");
      break;
    case ServiceWorkerVersion::FetchHandlerExistence::DOES_NOT_EXIST:
      info->SetString("fetch_handler_existence", "DOES_NOT_EXIST");
      break;
  }

  info->SetString("script_url", version.script_url.spec());
  info->SetString("version_id", base::NumberToString(version.version_id));
  info->SetInteger("process_id",
                   static_cast<int>(GetRealProcessId(version.process_id)));
  info->SetInteger("process_host_id", version.process_id);
  info->SetInteger("thread_id", version.thread_id);
  info->SetInteger("devtools_agent_route_id", version.devtools_agent_route_id);

  auto clients = ListValue();
  for (auto& it : version.clients) {
    auto client = DictionaryValue();
    client.SetStringPath("client_id", it.first);
    if (it.second.web_contents_getter) {
      WebContents* web_contents = it.second.web_contents_getter.Run();
      if (web_contents)
        client.SetStringPath("url", web_contents->GetURL().spec());
    }
    clients.Append(std::move(client));
  }
  info->SetPath("clients", std::move(clients));
}

std::unique_ptr<ListValue> GetRegistrationListValue(
    const std::vector<ServiceWorkerRegistrationInfo>& registrations) {
  auto result = std::make_unique<ListValue>();
  for (auto it = registrations.begin(); it != registrations.end(); ++it) {
    const ServiceWorkerRegistrationInfo& registration = *it;
    auto registration_info = std::make_unique<DictionaryValue>();
    registration_info->SetString("scope", registration.scope.spec());
    registration_info->SetString(
        "registration_id", base::NumberToString(registration.registration_id));
    registration_info->SetBoolean("navigation_preload_enabled",
                                  registration.navigation_preload_enabled);
    registration_info->SetInteger(
        "navigation_preload_header_length",
        registration.navigation_preload_header_length);

    if (registration.active_version.version_id !=
        blink::mojom::kInvalidServiceWorkerVersionId) {
      auto active_info = std::make_unique<DictionaryValue>();
      UpdateVersionInfo(registration.active_version, active_info.get());
      registration_info->Set("active", std::move(active_info));
    }

    if (registration.waiting_version.version_id !=
        blink::mojom::kInvalidServiceWorkerVersionId) {
      auto waiting_info = std::make_unique<DictionaryValue>();
      UpdateVersionInfo(registration.waiting_version, waiting_info.get());
      registration_info->Set("waiting", std::move(waiting_info));
    }

    result->Append(std::move(registration_info));
  }
  return result;
}

std::unique_ptr<ListValue> GetVersionListValue(
    const std::vector<ServiceWorkerVersionInfo>& versions) {
  auto result = std::make_unique<ListValue>();
  for (auto it = versions.begin(); it != versions.end(); ++it) {
    auto info = std::make_unique<DictionaryValue>();
    UpdateVersionInfo(*it, info.get());
    result->Append(std::move(info));
  }
  return result;
}

void DidGetStoredRegistrationsOnCoreThread(
    scoped_refptr<ServiceWorkerContextWrapper> context,
    GetRegistrationsCallback callback,
    blink::ServiceWorkerStatusCode status,
    const std::vector<ServiceWorkerRegistrationInfo>& stored_registrations) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(std::move(callback), context->GetAllLiveRegistrationInfo(),
                     context->GetAllLiveVersionInfo(), stored_registrations));
}

void GetRegistrationsOnCoreThread(
    scoped_refptr<ServiceWorkerContextWrapper> context,
    GetRegistrationsCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  context->GetAllRegistrations(base::BindOnce(
      DidGetStoredRegistrationsOnCoreThread, context, std::move(callback)));
}

void DidGetRegistrations(
    WeakPtr<ServiceWorkerInternalsUI> internals,
    int partition_id,
    const base::FilePath& context_path,
    const std::vector<ServiceWorkerRegistrationInfo>& live_registrations,
    const std::vector<ServiceWorkerVersionInfo>& live_versions,
    const std::vector<ServiceWorkerRegistrationInfo>& stored_registrations) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!internals)
    return;

  std::vector<std::unique_ptr<const Value>> args;
  args.push_back(GetRegistrationListValue(live_registrations));
  args.push_back(GetVersionListValue(live_versions));
  args.push_back(GetRegistrationListValue(stored_registrations));
  args.push_back(std::make_unique<Value>(partition_id));
  args.push_back(std::make_unique<Value>(context_path.value()));
  internals->web_ui()->CallJavascriptFunctionUnsafe(
      "serviceworker.onPartitionData", ConvertToRawPtrVector(args));
}

}  // namespace

class ServiceWorkerInternalsUI::PartitionObserver
    : public ServiceWorkerContextCoreObserver {
 public:
  PartitionObserver(int partition_id, WebUI* web_ui)
      : partition_id_(partition_id), web_ui_(web_ui) {}
  ~PartitionObserver() override {}
  // ServiceWorkerContextCoreObserver overrides:
  void OnStarting(int64_t version_id) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    web_ui_->CallJavascriptFunctionUnsafe(
        "serviceworker.onRunningStateChanged");
  }
  void OnStarted(int64_t version_id,
                 const GURL& scope,
                 int process_id,
                 const GURL& script_url) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    web_ui_->CallJavascriptFunctionUnsafe(
        "serviceworker.onRunningStateChanged");
  }
  void OnStopping(int64_t version_id) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    web_ui_->CallJavascriptFunctionUnsafe(
        "serviceworker.onRunningStateChanged");
  }
  void OnStopped(int64_t version_id) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    web_ui_->CallJavascriptFunctionUnsafe(
        "serviceworker.onRunningStateChanged");
  }
  void OnVersionStateChanged(int64_t version_id,
                             const GURL& scope,
                             ServiceWorkerVersion::Status) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    web_ui_->CallJavascriptFunctionUnsafe(
        "serviceworker.onVersionStateChanged", Value(partition_id_),
        Value(base::NumberToString(version_id)));
  }
  void OnErrorReported(int64_t version_id,
                       const ErrorInfo& info) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    std::vector<std::unique_ptr<const Value>> args;
    args.push_back(std::make_unique<Value>(partition_id_));
    args.push_back(std::make_unique<Value>(base::NumberToString(version_id)));
    auto value = std::make_unique<DictionaryValue>();
    value->SetString("message", info.error_message);
    value->SetInteger("lineNumber", info.line_number);
    value->SetInteger("columnNumber", info.column_number);
    value->SetString("sourceURL", info.source_url.spec());
    args.push_back(std::move(value));
    web_ui_->CallJavascriptFunctionUnsafe("serviceworker.onErrorReported",
                                          ConvertToRawPtrVector(args));
  }
  void OnReportConsoleMessage(int64_t version_id,
                              const ConsoleMessage& message) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    std::vector<std::unique_ptr<const Value>> args;
    args.push_back(std::make_unique<Value>(partition_id_));
    args.push_back(std::make_unique<Value>(base::NumberToString(version_id)));
    auto value = std::make_unique<DictionaryValue>();
    value->SetInteger("sourceIdentifier", static_cast<int>(message.source));
    value->SetInteger("message_level", static_cast<int>(message.message_level));
    value->SetString("message", message.message);
    value->SetInteger("lineNumber", message.line_number);
    value->SetString("sourceURL", message.source_url.spec());
    args.push_back(std::move(value));
    web_ui_->CallJavascriptFunctionUnsafe(
        "serviceworker.onConsoleMessageReported", ConvertToRawPtrVector(args));
  }
  void OnRegistrationCompleted(int64_t registration_id,
                               const GURL& scope) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    web_ui_->CallJavascriptFunctionUnsafe(
        "serviceworker.onRegistrationCompleted", Value(scope.spec()));
  }
  void OnRegistrationDeleted(int64_t registration_id,
                             const GURL& scope) override {
    web_ui_->CallJavascriptFunctionUnsafe("serviceworker.onRegistrationDeleted",
                                          Value(scope.spec()));
  }
  int partition_id() const { return partition_id_; }

 private:
  const int partition_id_;
  WebUI* const web_ui_;
};

ServiceWorkerInternalsUI::ServiceWorkerInternalsUI(WebUI* web_ui)
    : WebUIController(web_ui), next_partition_id_(0) {
  WebUIDataSource* source =
      WebUIDataSource::Create(kChromeUIServiceWorkerInternalsHost);
  source->OverrideContentSecurityPolicyScriptSrc(
      "script-src chrome://resources 'self' 'unsafe-eval';");
  source->UseStringsJs();
  source->AddResourcePath("serviceworker_internals.js",
                          IDR_SERVICE_WORKER_INTERNALS_JS);
  source->AddResourcePath("serviceworker_internals.css",
                          IDR_SERVICE_WORKER_INTERNALS_CSS);
  source->SetDefaultResource(IDR_SERVICE_WORKER_INTERNALS_HTML);
  source->DisableDenyXFrameOptions();

  BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  WebUIDataSource::Add(browser_context, source);

  web_ui->RegisterMessageCallback(
      "GetOptions", base::BindRepeating(&ServiceWorkerInternalsUI::GetOptions,
                                        base::Unretained(this)));
  web_ui->RegisterMessageCallback(
      "SetOption", base::BindRepeating(&ServiceWorkerInternalsUI::SetOption,
                                       base::Unretained(this)));
  web_ui->RegisterMessageCallback(
      "getAllRegistrations",
      base::BindRepeating(&ServiceWorkerInternalsUI::GetAllRegistrations,
                          base::Unretained(this)));
  web_ui->RegisterMessageCallback(
      "stop", base::BindRepeating(&ServiceWorkerInternalsUI::StopWorker,
                                  base::Unretained(this)));
  web_ui->RegisterMessageCallback(
      "inspect", base::BindRepeating(&ServiceWorkerInternalsUI::InspectWorker,
                                     base::Unretained(this)));
  web_ui->RegisterMessageCallback(
      "unregister", base::BindRepeating(&ServiceWorkerInternalsUI::Unregister,
                                        base::Unretained(this)));
  web_ui->RegisterMessageCallback(
      "start", base::BindRepeating(&ServiceWorkerInternalsUI::StartWorker,
                                   base::Unretained(this)));
}

ServiceWorkerInternalsUI::~ServiceWorkerInternalsUI() {
  BrowserContext* browser_context =
      web_ui()->GetWebContents()->GetBrowserContext();
  // Safe to use base::Unretained(this) because
  // ForEachStoragePartition is synchronous.
  BrowserContext::StoragePartitionCallback remove_observer_cb =
      base::BindRepeating(
          &ServiceWorkerInternalsUI::RemoveObserverFromStoragePartition,
          base::Unretained(this));
  BrowserContext::ForEachStoragePartition(browser_context,
                                          std::move(remove_observer_cb));
}

void ServiceWorkerInternalsUI::GetOptions(const ListValue* args) {
  DictionaryValue options;
  options.SetBoolean("debug_on_start",
                     ServiceWorkerDevToolsManager::GetInstance()
                         ->debug_service_worker_on_start());
  web_ui()->CallJavascriptFunctionUnsafe("serviceworker.onOptions", options);
}

void ServiceWorkerInternalsUI::SetOption(const ListValue* args) {
  std::string option_name;
  bool option_boolean;
  if (!args->GetString(0, &option_name) || option_name != "debug_on_start" ||
      !args->GetBoolean(1, &option_boolean)) {
    return;
  }
  ServiceWorkerDevToolsManager::GetInstance()
      ->set_debug_service_worker_on_start(option_boolean);
}

void ServiceWorkerInternalsUI::GetAllRegistrations(const ListValue* args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserContext* browser_context =
      web_ui()->GetWebContents()->GetBrowserContext();
  // Safe to use base::Unretained(this) because
  // ForEachStoragePartition is synchronous.
  BrowserContext::StoragePartitionCallback add_context_cb = base::BindRepeating(
      &ServiceWorkerInternalsUI::AddContextFromStoragePartition,
      base::Unretained(this));
  BrowserContext::ForEachStoragePartition(browser_context,
                                          std::move(add_context_cb));
}

void ServiceWorkerInternalsUI::AddContextFromStoragePartition(
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
    auto new_observer =
        std::make_unique<PartitionObserver>(partition_id, web_ui());
    context->AddObserver(new_observer.get());
    observers_[reinterpret_cast<uintptr_t>(partition)] =
        std::move(new_observer);
  }

  RunOrPostTaskOnThread(
      FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
      base::BindOnce(
          GetRegistrationsOnCoreThread, context,
          base::BindOnce(DidGetRegistrations, AsWeakPtr(), partition_id,
                         context->is_incognito() ? base::FilePath()
                                                 : partition->GetPath())));
}

void ServiceWorkerInternalsUI::RemoveObserverFromStoragePartition(
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

void ServiceWorkerInternalsUI::FindContext(
    int partition_id,
    StoragePartition** result_partition,
    StoragePartition* storage_partition) const {
  auto it = observers_.find(reinterpret_cast<uintptr_t>(storage_partition));
  if (it != observers_.end() && partition_id == it->second->partition_id()) {
    *result_partition = storage_partition;
  }
}

bool ServiceWorkerInternalsUI::GetServiceWorkerContext(
    int partition_id,
    scoped_refptr<ServiceWorkerContextWrapper>* context) const {
  BrowserContext* browser_context =
      web_ui()->GetWebContents()->GetBrowserContext();
  StoragePartition* result_partition(nullptr);
  BrowserContext::StoragePartitionCallback find_context_cb =
      base::BindRepeating(&ServiceWorkerInternalsUI::FindContext,
                          base::Unretained(this), partition_id,
                          &result_partition);
  BrowserContext::ForEachStoragePartition(browser_context,
                                          std::move(find_context_cb));
  if (!result_partition)
    return false;
  *context = static_cast<ServiceWorkerContextWrapper*>(
      result_partition->GetServiceWorkerContext());
  return true;
}

void ServiceWorkerInternalsUI::StopWorker(const ListValue* args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  int callback_id;
  const DictionaryValue* cmd_args = nullptr;
  int partition_id;
  scoped_refptr<ServiceWorkerContextWrapper> context;
  std::string version_id_string;
  int64_t version_id = 0;
  if (!args->GetInteger(0, &callback_id) ||
      !args->GetDictionary(1, &cmd_args) ||
      !cmd_args->GetInteger("partition_id", &partition_id) ||
      !GetServiceWorkerContext(partition_id, &context) ||
      !cmd_args->GetString("version_id", &version_id_string) ||
      !base::StringToInt64(version_id_string, &version_id)) {
    return;
  }

  base::OnceCallback<void(blink::ServiceWorkerStatusCode)> callback =
      base::BindOnce(OperationCompleteCallback, AsWeakPtr(), callback_id);
  StopWorkerWithId(context, version_id, std::move(callback));
}

void ServiceWorkerInternalsUI::InspectWorker(const ListValue* args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  int callback_id;
  const DictionaryValue* cmd_args = nullptr;
  int process_host_id = 0;
  int devtools_agent_route_id = 0;
  if (!args->GetInteger(0, &callback_id) ||
      !args->GetDictionary(1, &cmd_args) ||
      !cmd_args->GetInteger("process_host_id", &process_host_id) ||
      !cmd_args->GetInteger("devtools_agent_route_id",
                            &devtools_agent_route_id)) {
    return;
  }
  base::OnceCallback<void(blink::ServiceWorkerStatusCode)> callback =
      base::BindOnce(OperationCompleteCallback, AsWeakPtr(), callback_id);
  scoped_refptr<ServiceWorkerDevToolsAgentHost> agent_host(
      ServiceWorkerDevToolsManager::GetInstance()
          ->GetDevToolsAgentHostForWorker(process_host_id,
                                          devtools_agent_route_id));
  if (!agent_host.get()) {
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kErrorNotFound);
    return;
  }
  agent_host->Inspect();
  std::move(callback).Run(blink::ServiceWorkerStatusCode::kOk);
}

void ServiceWorkerInternalsUI::Unregister(const ListValue* args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  int callback_id;
  int partition_id;
  std::string scope_string;
  const DictionaryValue* cmd_args = nullptr;
  scoped_refptr<ServiceWorkerContextWrapper> context;
  if (!args->GetInteger(0, &callback_id) ||
      !args->GetDictionary(1, &cmd_args) ||
      !cmd_args->GetInteger("partition_id", &partition_id) ||
      !GetServiceWorkerContext(partition_id, &context) ||
      !cmd_args->GetString("scope", &scope_string)) {
    return;
  }

  base::OnceCallback<void(blink::ServiceWorkerStatusCode)> callback =
      base::BindOnce(OperationCompleteCallback, AsWeakPtr(), callback_id);
  UnregisterWithScope(context, GURL(scope_string), std::move(callback));
}

void ServiceWorkerInternalsUI::StartWorker(const ListValue* args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  int callback_id;
  int partition_id;
  std::string scope_string;
  const DictionaryValue* cmd_args = nullptr;
  scoped_refptr<ServiceWorkerContextWrapper> context;
  if (!args->GetInteger(0, &callback_id) ||
      !args->GetDictionary(1, &cmd_args) ||
      !cmd_args->GetInteger("partition_id", &partition_id) ||
      !GetServiceWorkerContext(partition_id, &context) ||
      !cmd_args->GetString("scope", &scope_string)) {
    return;
  }
  base::OnceCallback<void(blink::ServiceWorkerStatusCode)> callback =
      base::BindOnce(OperationCompleteCallback, AsWeakPtr(), callback_id);
  context->StartServiceWorker(GURL(scope_string), std::move(callback));
}

void ServiceWorkerInternalsUI::StopWorkerWithId(
    scoped_refptr<ServiceWorkerContextWrapper> context,
    int64_t version_id,
    StatusCallback callback) {
  if (!BrowserThread::CurrentlyOn(ServiceWorkerContext::GetCoreThreadId())) {
    base::PostTask(FROM_HERE, {ServiceWorkerContext::GetCoreThreadId()},
                   base::BindOnce(&ServiceWorkerInternalsUI::StopWorkerWithId,
                                  base::Unretained(this), context, version_id,
                                  std::move(callback)));
    return;
  }

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

void ServiceWorkerInternalsUI::UnregisterWithScope(
    scoped_refptr<ServiceWorkerContextWrapper> context,
    const GURL& scope,
    ServiceWorkerInternalsUI::StatusCallback callback) const {
  if (!BrowserThread::CurrentlyOn(ServiceWorkerContext::GetCoreThreadId())) {
    base::PostTask(
        FROM_HERE, {ServiceWorkerContext::GetCoreThreadId()},
        base::BindOnce(&ServiceWorkerInternalsUI::UnregisterWithScope,
                       base::Unretained(this), context, scope,
                       std::move(callback)));
    return;
  }

  if (!context->context()) {
    std::move(callback).Run(blink::ServiceWorkerStatusCode::kErrorAbort);
    return;
  }

  // ServiceWorkerContextWrapper::UnregisterServiceWorker doesn't work here
  // because that reduces a status code to boolean.
  context->context()->UnregisterServiceWorker(
      scope, base::AdaptCallbackForRepeating(std::move(callback)));
}

}  // namespace content
