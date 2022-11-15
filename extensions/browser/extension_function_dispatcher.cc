// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_function_dispatcher.h"

#include <utility>

#include "base/bind.h"
#include "base/debug/crash_logging.h"
#include "base/json/json_string_value_serializer.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/process.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/result_codes.h"
#include "extensions/browser/api_activity_monitor.h"
#include "extensions/browser/bad_message.h"
#include "extensions/browser/content_script_tracker.h"
#include "extensions/browser/extension_function_registry.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_map.h"
#include "extensions/browser/quota_service.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_api.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/extension_urls.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "mojo/public/cpp/bindings/message.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using content::BrowserThread;

namespace extensions {
namespace {

// Notifies the ApiActivityMonitor that an extension API function has been
// called. May be called from any thread.
void NotifyApiFunctionCalled(const ExtensionId& extension_id,
                             const std::string& api_name,
                             const base::Value::List& args,
                             content::BrowserContext* browser_context) {
  activity_monitor::OnApiFunctionCalled(browser_context, extension_id, api_name,
                                        args);
}

bool IsRequestFromServiceWorker(const mojom::RequestParams& request_params) {
  return request_params.service_worker_version_id !=
         blink::mojom::kInvalidServiceWorkerVersionId;
}

// Calls ResponseCallback with an empty result.
void ResponseCallbackOnError(ExtensionFunction::ResponseCallback callback,
                             ExtensionFunction::ResponseType type,
                             const std::string& error) {
  std::move(callback).Run(type, base::Value::List(), error, nullptr);
}

// Returns `true` if `render_process_host` can legitimately claim to send IPC
// messages on behalf of `extension_id`.  `render_frame_host` parameter is
// needed to account for scenarios involving a Chrome Web Store frame.
bool CanRendererActOnBehalfOfExtension(
    const ExtensionId& extension_id,
    content::RenderFrameHost* render_frame_host,
    content::RenderProcessHost& render_process_host) {
  // TODO(lukasza): Some of the checks below can be restricted to specific
  // context types (e.g. an empty `extension_id` should not happen in an
  // extension context;  and the SiteInstance-based check should only be needed
  // for hosted apps).  Consider leveraging ProcessMap::GetMostLikelyContextType
  // to implement this kind of restrictions.  Note that
  // ExtensionFunctionDispatcher::CreateExtensionFunction already calls
  // GetMostLikelyContextType - some refactoring might be needed to avoid
  // duplicating the work.

  // Allow empty extension id (it seems okay to assume that no
  // extension-specific special powers will be granted without an extension id).
  // For instance, WebUI pages may call private APIs like developerPrivate,
  // settingsPrivate, metricsPrivate, and others. In these cases, there is no
  // associated extension ID.
  //
  // TODO(lukasza): Investigate if the exception below can be avoided if
  // `render_process_host` hosts HTTP origins (i.e. if the exception can be
  // restricted to NTP, and/or chrome://... cases.
  if (extension_id.empty())
    return true;

  // Did `render_process_id` run a content script from `extension_id`?
  if (ContentScriptTracker::DidProcessRunContentScriptFromExtension(
          render_process_host, extension_id)) {
    return true;
  }

  // Can `render_process_id` host a chrome-extension:// origin (frame, worker,
  // etc.)?
  if (util::CanRendererHostExtensionOrigin(render_process_host.GetID(),
                                           extension_id)) {
    return true;
  }

  if (render_frame_host) {
    DCHECK_EQ(render_process_host.GetID(),
              render_frame_host->GetProcess()->GetID());
    content::SiteInstance& site_instance =
        *render_frame_host->GetSiteInstance();

    // Chrome Extension APIs can be accessed from some hosted apps.
    //
    // Today this is mostly needed by the Chrome Web Store's hosted app, but the
    // code below doesn't make this assumption and allows *all* hosted apps
    // based on the trustworthy, Browser-side information from the SiteInstance
    // / SiteURL.  This way the code is resilient to future changes + there are
    // concerns that `chrome.test.sendMessage` might already be exposed to
    // hosted apps (but maybe not covered by tests).
    //
    // Note that the condition below allows all extensions (i.e. not just hosted
    // apps), but hosted apps aren't covered by the
    // `CanRendererHostExtensionOrigin` call above (because the process lock of
    // hosted apps is based on a https://, rather than chrome-extension:// url).
    //
    // GuestView is explicitly excluded, because we don't want to allow
    // GuestViews to spoof the extension id of their host.
    if (!site_instance.IsGuest() &&
        extension_id == util::GetExtensionIdForSiteInstance(site_instance)) {
      return true;
    }
  }

  // Disallow any other cases.
  return false;
}

absl::optional<bad_message::BadMessageReason> ValidateRequest(
    const mojom::RequestParams& params,
    content::RenderFrameHost* render_frame_host,
    content::RenderProcessHost& render_process_host) {
  if ((render_frame_host && IsRequestFromServiceWorker(params)) ||
      (!render_frame_host && !IsRequestFromServiceWorker(params))) {
    return bad_message::EFD_BAD_MESSAGE;
  }

  if (!CanRendererActOnBehalfOfExtension(params.extension_id, render_frame_host,
                                         render_process_host)) {
    return bad_message::EFD_INVALID_EXTENSION_ID_FOR_PROCESS;
  }

  // TODO(https://crbug.com/1186447): Validate `params.user_gesture`.

  return absl::nullopt;
}

const char* ToString(bad_message::BadMessageReason bad_message_code) {
  switch (bad_message_code) {
    case bad_message::BadMessageReason::EFD_BAD_MESSAGE:
      return "LocalFrameHost::Request got a bad message.";
    case bad_message::BadMessageReason::EFD_INVALID_EXTENSION_ID_FOR_PROCESS:
      return "LocalFrameHost::Request: renderer never hosted such extension";
    default:
      NOTREACHED();
      return "LocalFrameHost::Request encountered unrecognized validation "
             "error.";
  }
}

// Helper for logging crash keys related to a the IPC payload from
// mojom::RequestParams.
class ScopedRequestParamsCrashKeys {
 public:
  explicit ScopedRequestParamsCrashKeys(const mojom::RequestParams& params)
      : name_(GetNameCrashKey(), params.name),
        extension_id_(GetExtensionIdCrashKey(), params.extension_id) {}

  ~ScopedRequestParamsCrashKeys() = default;

  // No copy constructor and no copy assignment operator.
  ScopedRequestParamsCrashKeys(const ScopedRequestParamsCrashKeys&) = delete;
  ScopedRequestParamsCrashKeys& operator=(const ScopedRequestParamsCrashKeys&) =
      delete;

 private:
  static base::debug::CrashKeyString* GetNameCrashKey() {
    static auto* crash_key = base::debug::AllocateCrashKeyString(
        "RequestParams-name", base::debug::CrashKeySize::Size256);
    return crash_key;
  }

  static base::debug::CrashKeyString* GetExtensionIdCrashKey() {
    static auto* crash_key = base::debug::AllocateCrashKeyString(
        "RequestParams-extension_id", base::debug::CrashKeySize::Size64);
    return crash_key;
  }

  base::debug::ScopedCrashKeyString name_;
  base::debug::ScopedCrashKeyString extension_id_;
};

}  // namespace

class ExtensionFunctionDispatcher::ResponseCallbackWrapper
    : public content::WebContentsObserver {
 public:
  ResponseCallbackWrapper(
      const base::WeakPtr<ExtensionFunctionDispatcher>& dispatcher,
      content::RenderFrameHost* render_frame_host)
      : content::WebContentsObserver(
            content::WebContents::FromRenderFrameHost(render_frame_host)),
        dispatcher_(dispatcher),
        render_frame_host_(render_frame_host) {}

  ResponseCallbackWrapper(const ResponseCallbackWrapper&) = delete;
  ResponseCallbackWrapper& operator=(const ResponseCallbackWrapper&) = delete;

  ~ResponseCallbackWrapper() override = default;

  // content::WebContentsObserver overrides.
  void RenderFrameDeleted(
      content::RenderFrameHost* render_frame_host) override {
    if (render_frame_host != render_frame_host_)
      return;

    if (dispatcher_.get()) {
      dispatcher_->response_callback_wrappers_.erase(render_frame_host);
    }
  }

  ExtensionFunction::ResponseCallback CreateCallback(
      mojom::LocalFrameHost::RequestCallback callback) {
    return base::BindOnce(
        &ResponseCallbackWrapper::OnExtensionFunctionCompleted,
        weak_ptr_factory_.GetWeakPtr(), std::move(callback));
  }

 private:
  // TODO(https://crbug.com/1312686): Change |results| type to
  // base::Value::List.
  void OnExtensionFunctionCompleted(
      mojom::LocalFrameHost::RequestCallback callback,
      ExtensionFunction::ResponseType type,
      base::Value::List results,
      const std::string& error,
      mojom::ExtraResponseDataPtr response_data) {
    std::move(callback).Run(type == ExtensionFunction::SUCCEEDED,
                            std::move(results), error,
                            std::move(response_data));
  }

  base::WeakPtr<ExtensionFunctionDispatcher> dispatcher_;
  raw_ptr<content::RenderFrameHost> render_frame_host_;
  base::WeakPtrFactory<ResponseCallbackWrapper> weak_ptr_factory_{this};
};

class ExtensionFunctionDispatcher::WorkerResponseCallbackWrapper
    : public content::RenderProcessHostObserver {
 public:
  WorkerResponseCallbackWrapper(
      const base::WeakPtr<ExtensionFunctionDispatcher>& dispatcher,
      content::RenderProcessHost* render_process_host,
      int worker_thread_id)
      : dispatcher_(dispatcher),
        render_process_host_(render_process_host) {
    observation_.Observe(render_process_host_.get());
  }

  WorkerResponseCallbackWrapper(const WorkerResponseCallbackWrapper&) = delete;
  WorkerResponseCallbackWrapper& operator=(
      const WorkerResponseCallbackWrapper&) = delete;

  ~WorkerResponseCallbackWrapper() override = default;

  // content::RenderProcessHostObserver override.
  void RenderProcessExited(
      content::RenderProcessHost* rph,
      const content::ChildProcessTerminationInfo& info) override {
    CleanUp();
  }

  // content::RenderProcessHostObserver override.
  void RenderProcessHostDestroyed(content::RenderProcessHost* rph) override {
    CleanUp();
  }

  ExtensionFunction::ResponseCallback CreateCallback(int request_id,
                                                     int worker_thread_id) {
    return base::BindOnce(
        &WorkerResponseCallbackWrapper::OnExtensionFunctionCompleted,
        weak_ptr_factory_.GetWeakPtr(), request_id, worker_thread_id);
  }

 private:
  void CleanUp() {
    if (dispatcher_) {
      dispatcher_->RemoveWorkerCallbacksForProcess(
          render_process_host_->GetID());
    }
    // Note: we are deleted here!
  }

  // TODO(https://crbug.com/1312686): Change |results| type to
  // base::Value::List.
  void OnExtensionFunctionCompleted(int request_id,
                                    int worker_thread_id,
                                    ExtensionFunction::ResponseType type,
                                    base::Value::List results,
                                    const std::string& error,
                                    mojom::ExtraResponseDataPtr extra_data) {
    if (type == ExtensionFunction::BAD_MESSAGE) {
      // The renderer will be shut down from ExtensionFunction::SetBadMessage().
      return;
    }
    ExtensionMsg_ResponseWorkerData response;
    response.results = std::move(results);
    response.extra_data = std::move(extra_data);
    render_process_host_->Send(new ExtensionMsg_ResponseWorker(
        worker_thread_id, request_id, type == ExtensionFunction::SUCCEEDED,
        std::move(response), error));
  }

  base::WeakPtr<ExtensionFunctionDispatcher> dispatcher_;
  base::ScopedObservation<content::RenderProcessHost,
                          content::RenderProcessHostObserver>
      observation_{this};
  const raw_ptr<content::RenderProcessHost> render_process_host_;
  base::WeakPtrFactory<WorkerResponseCallbackWrapper> weak_ptr_factory_{this};
};

struct ExtensionFunctionDispatcher::WorkerResponseCallbackMapKey {
  WorkerResponseCallbackMapKey(int render_process_id,
                               int64_t service_worker_version_id)
      : render_process_id(render_process_id),
        service_worker_version_id(service_worker_version_id) {}

  bool operator<(const WorkerResponseCallbackMapKey& other) const {
    return std::tie(render_process_id, service_worker_version_id) <
           std::tie(other.render_process_id, other.service_worker_version_id);
  }

  int render_process_id;
  int64_t service_worker_version_id;
};

WindowController*
ExtensionFunctionDispatcher::Delegate::GetExtensionWindowController() const {
  return nullptr;
}

content::WebContents*
ExtensionFunctionDispatcher::Delegate::GetAssociatedWebContents() const {
  return nullptr;
}

content::WebContents*
ExtensionFunctionDispatcher::Delegate::GetVisibleWebContents() const {
  return GetAssociatedWebContents();
}

ExtensionFunctionDispatcher::ExtensionFunctionDispatcher(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context), delegate_(nullptr) {}

ExtensionFunctionDispatcher::~ExtensionFunctionDispatcher() {
}

void ExtensionFunctionDispatcher::Dispatch(
    mojom::RequestParamsPtr params,
    content::RenderFrameHost& frame,
    mojom::LocalFrameHost::RequestCallback callback) {
  ScopedRequestParamsCrashKeys request_params_crash_keys(*params);
  SCOPED_CRASH_KEY_STRING256(
      "extensions", "frame.GetSiteInstance()",
      frame.GetSiteInstance()->GetSiteURL().possibly_invalid_spec());

  if (auto bad_message_code =
          ValidateRequest(*params, &frame, *frame.GetProcess())) {
    // Kill the renderer if it's an invalid request.
    const char* msg = ToString(*bad_message_code);
    std::move(callback).Run(ExtensionFunction::FAILED, base::Value::List(), msg,
                            nullptr);
    mojo::ReportBadMessage(msg);
    return;
  }

  // TODO(https://crbug.com/1227812): Validate (or remove) `params.source_url`.

  // Extension API from a non Service Worker context, e.g. extension page,
  // background page, content script.
  std::unique_ptr<ResponseCallbackWrapper>& callback_wrapper =
      response_callback_wrappers_[&frame];
  if (!callback_wrapper) {
    callback_wrapper = std::make_unique<ResponseCallbackWrapper>(
        weak_ptr_factory_.GetWeakPtr(), &frame);
  }

  DispatchWithCallbackInternal(
      *params, &frame, frame.GetProcess()->GetID(),
      callback_wrapper->CreateCallback(std::move(callback)));
}

void ExtensionFunctionDispatcher::DispatchForServiceWorker(
    const mojom::RequestParams& params,
    int render_process_id) {
  ScopedRequestParamsCrashKeys request_params_crash_keys(params);

  // The IPC might race with RenderProcessHost destruction.  This may only
  // happen in scenarios that are already inherently racey, so dropping the IPC
  // is okay and won't lead to any additional risk of data loss.  Continuing is
  // impossible, because WorkerResponseCallbackWrapper requires render process
  // host to be around.
  content::RenderProcessHost* rph =
      content::RenderProcessHost::FromID(render_process_id);
  if (!rph)
    return;

  if (auto bad_message_code = ValidateRequest(params, nullptr, *rph)) {
    // Kill the renderer if it's an invalid request.
    bad_message::ReceivedBadMessage(render_process_id, *bad_message_code);
    return;
  }

  WorkerId worker_id{params.extension_id, render_process_id,
                     params.service_worker_version_id, params.worker_thread_id};
  // Ignore if the worker has already stopped.
  if (!ProcessManager::Get(browser_context_)->HasServiceWorker(worker_id))
    return;

  WorkerResponseCallbackMapKey key(render_process_id,
                                   params.service_worker_version_id);
  std::unique_ptr<WorkerResponseCallbackWrapper>& callback_wrapper =
      response_callback_wrappers_for_worker_[key];
  if (!callback_wrapper) {
    callback_wrapper = std::make_unique<WorkerResponseCallbackWrapper>(
        weak_ptr_factory_.GetWeakPtr(), rph, params.worker_thread_id);
  }

  DispatchWithCallbackInternal(params, nullptr, render_process_id,
                               callback_wrapper->CreateCallback(
                                   params.request_id, params.worker_thread_id));
}

void ExtensionFunctionDispatcher::DispatchWithCallbackInternal(
    const mojom::RequestParams& params,
    content::RenderFrameHost* render_frame_host,
    int render_process_id,
    ExtensionFunction::ResponseCallback callback) {
  ProcessMap* process_map = ProcessMap::Get(browser_context_);
  if (!process_map) {
    constexpr char kProcessNotFound[] =
        "The process for the extension is not found.";
    ResponseCallbackOnError(std::move(callback), ExtensionFunction::FAILED,
                            kProcessNotFound);
    return;
  }

  const GURL* rfh_url = nullptr;
  if (render_frame_host) {
    rfh_url = &render_frame_host->GetLastCommittedURL();
    DCHECK_EQ(render_process_id, render_frame_host->GetProcess()->GetID());
  }

  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context_);
  const Extension* extension =
      registry->enabled_extensions().GetByID(params.extension_id);
  // Check if the call is from a hosted app. Hosted apps can only make call from
  // render frames, so we can use `rfh_url`.
  // TODO(devlin): Isn't `params.extension_id` still populated for hosted app
  // calls?
  if (!extension && rfh_url) {
    extension = registry->enabled_extensions().GetHostedAppByURL(*rfh_url);
  }

  const bool is_worker_request = IsRequestFromServiceWorker(params);

  scoped_refptr<ExtensionFunction> function = CreateExtensionFunction(
      params, extension, render_process_id, is_worker_request, rfh_url,
      *process_map, ExtensionAPI::GetSharedInstance(), std::move(callback));
  if (!function.get())
    return;

  function->set_worker_thread_id(params.worker_thread_id);
  if (is_worker_request) {
    function->set_service_worker_version_id(params.service_worker_version_id);
  } else {
    function->SetRenderFrameHost(render_frame_host);
  }
  function->SetDispatcher(weak_ptr_factory_.GetWeakPtr());
  if (extension &&
      ExtensionsBrowserClient::Get()->CanExtensionCrossIncognito(
          extension, browser_context_)) {
    function->set_include_incognito_information(true);
  }

  if (!extension) {
    if (function->source_context_type() == Feature::WEBUI_CONTEXT) {
      base::UmaHistogramSparse("Extensions.Functions.WebUICalls",
                               function->histogram_value());
    } else if (function->source_context_type() ==
               Feature::WEBUI_UNTRUSTED_CONTEXT) {
      base::UmaHistogramSparse("Extensions.Functions.WebUIUntrustedCalls",
                               function->histogram_value());
    }

    // Skip the quota, event page, activity logging stuff if there
    // isn't an extension, e.g. if the function call was from WebUI.
    function->RunWithValidation()->Execute();
    return;
  }

  // Fetch the ProcessManager before |this| is possibly invalidated.
  ProcessManager* process_manager = ProcessManager::Get(browser_context_);

  ExtensionSystem* extension_system = ExtensionSystem::Get(browser_context_);
  QuotaService* quota = extension_system->quota_service();
  std::string violation_error =
      quota->Assess(extension->id(), function.get(), params.arguments,
                    base::TimeTicks::Now());

  if (violation_error.empty()) {
    // See crbug.com/39178.
    ExtensionsBrowserClient::Get()->PermitExternalProtocolHandler();
    NotifyApiFunctionCalled(extension->id(), params.name, params.arguments,
                            browser_context_);

    // Note: Deliberately don't include external component extensions here -
    // this lets us differentiate between "built-in" extension calls and
    // external extension calls
    if (extension->location() == mojom::ManifestLocation::kComponent) {
      base::UmaHistogramSparse("Extensions.Functions.ComponentExtensionCalls",
                               function->histogram_value());
    } else {
      base::UmaHistogramSparse("Extensions.Functions.ExtensionCalls",
                               function->histogram_value());
    }

    if (IsRequestFromServiceWorker(params)) {
      base::UmaHistogramSparse(
          "Extensions.Functions.ExtensionServiceWorkerCalls",
          function->histogram_value());
    }

    if (extension->manifest_version() == 3) {
      base::UmaHistogramSparse("Extensions.Functions.ExtensionMV3Calls",
                               function->histogram_value());
    }

    base::ElapsedTimer timer;
    function->RunWithValidation()->Execute();
    // TODO(devlin): Once we have a baseline metric for how long functions take,
    // we can create a handful of buckets and record the function name so that
    // we can find what the fastest/slowest are.
    // Note: Many functions execute finish asynchronously, so this time is not
    // always a representation of total time taken. See also
    // Extensions.Functions.TotalExecutionTime.
    UMA_HISTOGRAM_TIMES("Extensions.Functions.SynchronousExecutionTime",
                        timer.Elapsed());
  } else {
    function->OnQuotaExceeded(violation_error);
  }

  // Note: do not access |this| after this point. We may have been deleted
  // if function->Run() ended up closing the tab that owns us.

  // Check if extension was uninstalled by management.uninstall.
  if (!registry->enabled_extensions().GetByID(params.extension_id))
    return;

  if (!IsRequestFromServiceWorker(params)) {
    // Increment ref count for non-service worker extension API. Ref count for
    // service worker extension API is handled separately on IO thread via IPC.
    process_manager->IncrementLazyKeepaliveCount(
        function->extension(), Activity::API_FUNCTION, function->name());
  }
}

void ExtensionFunctionDispatcher::RemoveWorkerCallbacksForProcess(
    int render_process_id) {
  WorkerResponseCallbackWrapperMap& map =
      response_callback_wrappers_for_worker_;
  for (auto it = map.begin(); it != map.end();) {
    if (it->first.render_process_id == render_process_id) {
      it = map.erase(it);
      continue;
    }
    ++it;
  }
}

void ExtensionFunctionDispatcher::OnExtensionFunctionCompleted(
    const Extension* extension,
    bool is_from_service_worker,
    const char* name) {
  if (extension && !is_from_service_worker) {
    // Decrement ref count for non-service worker extension API. Service
    // worker extension API ref counts are handled separately on IO thread
    // directly via IPC.
    ProcessManager::Get(browser_context_)
        ->DecrementLazyKeepaliveCount(extension, Activity::API_FUNCTION, name);
  }
}

WindowController*
ExtensionFunctionDispatcher::GetExtensionWindowController() const {
  return delegate_ ? delegate_->GetExtensionWindowController() : nullptr;
}

content::WebContents*
ExtensionFunctionDispatcher::GetAssociatedWebContents() const {
  return delegate_ ? delegate_->GetAssociatedWebContents() : nullptr;
}

content::WebContents*
ExtensionFunctionDispatcher::GetVisibleWebContents() const {
  return delegate_ ? delegate_->GetVisibleWebContents() :
      GetAssociatedWebContents();
}

void ExtensionFunctionDispatcher::AddWorkerResponseTarget(
    ExtensionFunction* func) {
  DCHECK(func->is_from_service_worker());
  worker_response_targets_.insert(func);
}

void ExtensionFunctionDispatcher::ProcessServiceWorkerResponse(
    int request_id,
    int64_t service_worker_version_id) {
  for (auto it = worker_response_targets_.begin();
       it != worker_response_targets_.end(); ++it) {
    ExtensionFunction* func = *it;
    if (func->request_id() == request_id &&
        func->service_worker_version_id() == service_worker_version_id) {
      // Calling this may cause the instance to delete itself, so no
      // referencing it after this!
      func->OnServiceWorkerAck();
      worker_response_targets_.erase(it);
      break;
    }
  }
}

// static
scoped_refptr<ExtensionFunction>
ExtensionFunctionDispatcher::CreateExtensionFunction(
    const mojom::RequestParams& params,
    const Extension* extension,
    int requesting_process_id,
    bool is_worker_request,
    const GURL* rfh_url,
    const ProcessMap& process_map,
    ExtensionAPI* api,
    ExtensionFunction::ResponseCallback callback) {
  constexpr char kCreationFailed[] = "Access to extension API denied.";

  scoped_refptr<ExtensionFunction> function =
      ExtensionFunctionRegistry::GetInstance().NewFunction(params.name);
  if (!function) {
    LOG(ERROR) << "Unknown Extension API - " << params.name;
    ResponseCallbackOnError(std::move(callback), ExtensionFunction::FAILED,
                            kCreationFailed);
    return nullptr;
  }

  function->SetArgs(base::Value(params.arguments.Clone()));

  const Feature::Context context_type = process_map.GetMostLikelyContextType(
      extension, requesting_process_id, rfh_url);

  // Determine the source URL. When possible, prefer fetching this value from
  // the RenderFrameHost, but fallback to the value in the `params` object if
  // necessary.
  // We can't use the frame URL in the case of a worker-based request (where
  // there is no frame).
  if (is_worker_request) {
    // TODO(https://crbug.com/1227812): Validate this URL further. Or, better,
    // remove it from `mojom::RequestParams`.
    function->set_source_url(params.source_url);
  } else {
    DCHECK(rfh_url);
    function->set_source_url(*rfh_url);
  }

  function->set_request_id(params.request_id);
  function->set_has_callback(params.has_callback);
  function->set_user_gesture(params.user_gesture);
  function->set_extension(extension);
  function->set_response_callback(std::move(callback));
  function->set_source_context_type(context_type);
  function->set_source_process_id(requesting_process_id);

  if (!function->HasPermission()) {
    LOG(ERROR) << "Permission denied for " << params.name;
    function->RespondWithError(kCreationFailed);
    return nullptr;
  }

  return function;
}
}  // namespace extensions
