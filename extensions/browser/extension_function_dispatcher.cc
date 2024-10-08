// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_function_dispatcher.h"

#include <optional>
#include <utility>

#include "base/debug/crash_logging.h"
#include "base/functional/bind.h"
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
#include "base/ranges/algorithm.h"
#include "base/scoped_observation.h"
#include "base/trace_event/typed_macros.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"
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
#include "content/public/common/url_constants.h"
#include "extensions/browser/api_activity_monitor.h"
#include "extensions/browser/bad_message.h"
#include "extensions/browser/extension_function_registry.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_map.h"
#include "extensions/browser/quota_service.h"
#include "extensions/browser/script_injection_tracker.h"
#include "extensions/browser/service_worker/service_worker_keepalive.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_api.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/common/trace_util.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "mojo/public/cpp/bindings/message.h"

using content::BrowserThread;
using perfetto::protos::pbzero::ChromeTrackEvent;

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

std::optional<bad_message::BadMessageReason> ValidateRequest(
    const mojom::RequestParams& params,
    content::RenderFrameHost* render_frame_host,
    content::RenderProcessHost& render_process_host) {
  if ((render_frame_host && IsRequestFromServiceWorker(params)) ||
      (!render_frame_host && !IsRequestFromServiceWorker(params))) {
    return bad_message::EFD_BAD_MESSAGE;
  }

  if (!util::CanRendererActOnBehalfOfExtension(
          params.extension_id, render_frame_host, render_process_host,
          /*include_user_scripts=*/true)) {
    return bad_message::EFD_INVALID_EXTENSION_ID_FOR_PROCESS;
  }

  // TODO(crbug.com/40055124): Validate `params.user_gesture`.

  return std::nullopt;
}

const char* ToString(bad_message::BadMessageReason bad_message_code) {
  switch (bad_message_code) {
    case bad_message::BadMessageReason::EFD_BAD_MESSAGE:
      return "LocalFrameHost::Request got a bad message.";
    case bad_message::BadMessageReason::EFD_INVALID_EXTENSION_ID_FOR_PROCESS:
      return "LocalFrameHost::Request: renderer never hosted such extension";
    default:
      NOTREACHED_IN_MIGRATION();
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
  content::RenderProcessHost& process = *frame.GetProcess();
  TRACE_EVENT("extensions", "ExtensionFunctionDispatcher::Dispatch",
              ChromeTrackEvent::kRenderProcessHost, process,
              ChromeTrackEvent::kChromeExtensionId,
              ExtensionIdForTracing(params->extension_id));

  ScopedRequestParamsCrashKeys request_params_crash_keys(*params);
  SCOPED_CRASH_KEY_STRING256(
      "extensions", "frame.GetSiteInstance()",
      frame.GetSiteInstance()->GetSiteURL().possibly_invalid_spec());

  if (auto bad_message_code = ValidateRequest(*params, &frame, process)) {
    // Kill the renderer if it's an invalid request.
    debug::ScopedScriptInjectionTrackerFailureCrashKeys tracker_keys(
        frame, params->extension_id);
    bad_message::ReceivedBadMessage(&process, *bad_message_code);
    std::move(callback).Run(ExtensionFunction::FAILED, base::Value::List(),
                            ToString(*bad_message_code), nullptr);
    return;
  }

  // TODO(crbug.com/40056469): Validate (or remove) `params.source_url`.
  DispatchWithCallbackInternal(
      *params, &frame, *frame.GetProcess(),
      base::BindOnce(
          [](mojom::LocalFrameHost::RequestCallback callback,
             ExtensionFunction::ResponseType type, base::Value::List results,
             const std::string& error,
             mojom::ExtraResponseDataPtr response_data) {
            std::move(callback).Run(type == ExtensionFunction::SUCCEEDED,
                                    std::move(results), error,
                                    std::move(response_data));
          },
          std::move(callback)));
}

void ExtensionFunctionDispatcher::DispatchForServiceWorker(
    mojom::RequestParamsPtr params,
    int render_process_id,
    mojom::ServiceWorkerHost::RequestWorkerCallback callback) {
  ScopedRequestParamsCrashKeys request_params_crash_keys(*params);

  // The IPC might race with RenderProcessHost destruction.  This may only
  // happen in scenarios that are already inherently racey, so dropping the IPC
  // is okay and won't lead to any additional risk of data loss.  Continuing is
  // impossible, because WorkerResponseCallbackWrapper requires render process
  // host to be around.
  content::RenderProcessHost* rph =
      content::RenderProcessHost::FromID(render_process_id);
  if (!rph) {
    std::move(callback).Run(ExtensionFunction::FAILED, base::Value::List(),
                            "No RPH", nullptr);
    return;
  }

  TRACE_EVENT("extensions",
              "ExtensionFunctionDispatcher::DispatchForServiceWorker",
              ChromeTrackEvent::kRenderProcessHost, *rph,
              ChromeTrackEvent::kChromeExtensionId,
              ExtensionIdForTracing(params->extension_id));
  if (auto bad_message_code = ValidateRequest(*params, nullptr, *rph)) {
    // Kill the renderer if it's an invalid request.
    bad_message::ReceivedBadMessage(render_process_id, *bad_message_code);
    std::move(callback).Run(ExtensionFunction::FAILED, base::Value::List(),
                            ToString(*bad_message_code), nullptr);
    return;
  }

  WorkerId worker_id{params->extension_id, render_process_id,
                     params->service_worker_version_id,
                     params->worker_thread_id};
  // Ignore if the worker has already stopped.
  if (!ProcessManager::Get(browser_context_)->HasServiceWorker(worker_id)) {
    std::move(callback).Run(ExtensionFunction::FAILED, base::Value::List(),
                            "No SW", nullptr);
    return;
  }

  DispatchWithCallbackInternal(
      *params, nullptr, *rph,
      base::BindOnce(
          [](mojom::ServiceWorkerHost::RequestWorkerCallback callback,
             ExtensionFunction::ResponseType type, base::Value::List results,
             const std::string& error,
             mojom::ExtraResponseDataPtr response_data) {
            std::move(callback).Run(type == ExtensionFunction::SUCCEEDED,
                                    std::move(results), error,
                                    std::move(response_data));
          },
          std::move(callback)));
}

void ExtensionFunctionDispatcher::DispatchWithCallbackInternal(
    const mojom::RequestParams& params,
    content::RenderFrameHost* render_frame_host,
    content::RenderProcessHost& render_process_host,
    ExtensionFunction::ResponseCallback callback) {
  ProcessMap* process_map = ProcessMap::Get(browser_context_);
  if (!process_map) {
    constexpr char kProcessNotFound[] =
        "The process for the extension is not found.";
    ResponseCallbackOnError(std::move(callback), ExtensionFunction::FAILED,
                            kProcessNotFound);
    return;
  }

  const int render_process_id = render_process_host.GetID();

  const GURL* render_frame_host_url = nullptr;
  if (render_frame_host) {
    render_frame_host_url = &render_frame_host->GetLastCommittedURL();
    DCHECK_EQ(render_process_id, render_frame_host->GetProcess()->GetID());
  }

  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context_);
  const Extension* extension =
      registry->enabled_extensions().GetByID(params.extension_id);
  // Check if the call is from a hosted app. Hosted apps can only make call from
  // render frames, so we can use `render_frame_host_url`.
  // TODO(devlin): Isn't `params.extension_id` still populated for hosted app
  // calls?
  if (!extension && render_frame_host_url) {
    extension = registry->enabled_extensions().GetHostedAppByURL(
        *render_frame_host_url);
  }

  if (!process_map->CanProcessHostContextType(extension, render_process_host,
                                              params.context_type)) {
    // TODO(crbug.com/40055126): Ideally, we'd be able to mark some
    // of these as bad messages. We can't do that in all cases because there
    // are times some of these might legitimately fail (for instance, during
    // extension unload), but there are others that should never, ever happen
    // (privileged extension contexts in web processes).
    static constexpr char kInvalidContextType[] =
        "Invalid context type provided.";
    ResponseCallbackOnError(std::move(callback), ExtensionFunction::FAILED,
                            kInvalidContextType);
    return;
  }

  if (params.context_type == mojom::ContextType::kUntrustedWebUi) {
    // TODO(crbug.com/40265193): We should, at minimum, be using an
    // origin here. It'd be even better if we could have a more robust way of
    // checking that a process can host untrusted webui.
    if (extension || !render_frame_host_url ||
        !render_frame_host_url->SchemeIs(content::kChromeUIUntrustedScheme)) {
      constexpr char kInvalidWebUiUntrustedContext[] =
          "Context indicated it was untrusted webui, but is invalid.";
      ResponseCallbackOnError(std::move(callback), ExtensionFunction::FAILED,
                              kInvalidWebUiUntrustedContext);
      return;
    }
  }

  const bool is_worker_request = IsRequestFromServiceWorker(params);

  scoped_refptr<ExtensionFunction> function = CreateExtensionFunction(
      params, extension, render_process_id, is_worker_request,
      render_frame_host_url, params.context_type,
      ExtensionAPI::GetSharedInstance(), std::move(callback),
      render_frame_host);
  if (!function.get()) {
    return;
  }

  if (extension &&
      ExtensionsBrowserClient::Get()->CanExtensionCrossIncognito(
          extension, browser_context_)) {
    function->set_include_incognito_information(true);
  }

  if (!extension) {
    if (function->source_context_type() == mojom::ContextType::kWebUi) {
      base::UmaHistogramSparse("Extensions.Functions.WebUICalls",
                               function->histogram_value());
    } else if (function->source_context_type() ==
               mojom::ContextType::kUntrustedWebUi) {
      base::UmaHistogramSparse("Extensions.Functions.WebUIUntrustedCalls",
                               function->histogram_value());
    } else if (function->source_context_type() ==
               mojom::ContextType::kWebPage) {
      base::UmaHistogramSparse("Extensions.Functions.NonExtensionWebPageCalls",
                               function->histogram_value());
    }

    // Skip the quota, event page, activity logging stuff if there
    // isn't an extension, e.g. if the function call was from WebUI.
    function->RunWithValidation().Execute();
    return;
  }

  // Fetch the ProcessManager before |this| is possibly invalidated.
  ProcessManager* process_manager = ProcessManager::Get(browser_context_);

  ExtensionSystem* extension_system = ExtensionSystem::Get(browser_context_);
  QuotaService* quota = extension_system->quota_service();
  std::string violation_error =
      quota->Assess(extension->id(), function.get(), params.arguments,
                    base::TimeTicks::Now());

  function->set_request_uuid(base::Uuid::GenerateRandomV4());

  // Increment the keepalive to ensure the extension doesn't shut down while
  // it's executing an API function. This is balanced in
  // `OnExtensionFunctionCompleted()`.
  if (IsRequestFromServiceWorker(params)) {
    CHECK(function->worker_id());
    content::ServiceWorkerExternalRequestTimeoutType timeout_type =
        function->ShouldKeepWorkerAliveIndefinitely()
            ? content::ServiceWorkerExternalRequestTimeoutType::kDoesNotTimeout
            : content::ServiceWorkerExternalRequestTimeoutType::kDefault;
    function->set_service_worker_keepalive(
        std::make_unique<ServiceWorkerKeepalive>(
            browser_context_, *function->worker_id(), timeout_type,
            Activity::API_FUNCTION, function->name()));
  } else {
    process_manager->IncrementLazyKeepaliveCount(
        function->extension(), Activity::API_FUNCTION, function->name());
  }

  if (violation_error.empty()) {
    // See crbug.com/39178.
    ExtensionsBrowserClient::Get()->PermitExternalProtocolHandler();
    NotifyApiFunctionCalled(extension->id(), params.name, params.arguments,
                            browser_context_);

    // Since sandboxed frames listed in the manifest don't get access to the
    // extension APIs, this will only be true in an extension frame in an iframe
    // with the sandbox attribute specified, or served with a CSP header.
    bool is_sandboxed =
        render_frame_host && render_frame_host->IsSandboxed(
                                 network::mojom::WebSandboxFlags::kOrigin);
    // Note: Deliberately don't include external component extensions here -
    // this lets us differentiate between "built-in" extension calls and
    // external extension calls
    if (extension->location() == mojom::ManifestLocation::kComponent) {
      base::UmaHistogramSparse("Extensions.Functions.ComponentExtensionCalls",
                               function->histogram_value());
      if (is_sandboxed) {
        base::UmaHistogramBoolean(
            "Extensions.Functions.DidSandboxedComponentExtensionAPICall", true);
      }
    } else {
      base::UmaHistogramSparse("Extensions.Functions.ExtensionCalls",
                               function->histogram_value());
      if (is_sandboxed) {
        base::UmaHistogramBoolean(
            "Extensions.Functions.DidSandboxedExtensionAPICall", true);
      }
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
    function->RunWithValidation().Execute();
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
  // if `function->RunWithValidation()` resulted in closing the execution
  // context for this function.
}

void ExtensionFunctionDispatcher::OnExtensionFunctionCompleted(
    ExtensionFunction& extension_function) {
  if (!extension_function.extension()) {
    // The function had no associated extension; nothing to clean up.
    return;
  }

  if (!extension_function.browser_context()) {
    // The ExtensionFunction's browser context is null'ed out when the browser
    // context is being shut down. If this happens, there's nothing to clean up.
    return;
  }

  if (!ExtensionRegistry::Get(browser_context_)
           ->enabled_extensions()
           .GetByID(extension_function.extension()->id())) {
    // The extension may have been unloaded (the ExtensionFunction holds a
    // reference to it, so it's still safe to access). If so, there's nothing to
    // // clean up.
    return;
  }

  ProcessManager* process_manager = ProcessManager::Get(browser_context_);
  if (extension_function.is_from_service_worker()) {
    CHECK(extension_function.request_uuid().is_valid());
    CHECK(extension_function.worker_id());

    extension_function.ResetServiceWorkerKeepalive();
  } else {
    process_manager->DecrementLazyKeepaliveCount(extension_function.extension(),
                                                 Activity::API_FUNCTION,
                                                 extension_function.name());
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

void ExtensionFunctionDispatcher::AddResponseTarget(ExtensionFunction* func) {
  response_targets_.insert(func);
}

void ExtensionFunctionDispatcher::ProcessResponseAck(
    const base::Uuid& request_uuid) {
  auto iter = base::ranges::find_if(
      response_targets_, [request_uuid](ExtensionFunction* function) {
        return function->request_uuid() == request_uuid;
      });
  if (iter == response_targets_.end()) {
    return;
  }
  // Calling this may cause the instance to delete itself, so no
  // referencing it after this!
  (*iter)->OnResponseAck();
  response_targets_.erase(iter);
}

scoped_refptr<ExtensionFunction>
ExtensionFunctionDispatcher::CreateExtensionFunction(
    const mojom::RequestParams& params,
    const Extension* extension,
    int requesting_process_id,
    bool is_worker_request,
    const GURL* render_frame_host_url,
    mojom::ContextType context_type,
    ExtensionAPI* api,
    ExtensionFunction::ResponseCallback callback,
    content::RenderFrameHost* render_frame_host) {
  constexpr char kCreationFailed[] = "Access to extension API denied.";

  scoped_refptr<ExtensionFunction> function =
      ExtensionFunctionRegistry::GetInstance().NewFunction(params.name);
  if (!function) {
    LOG(ERROR) << "Unknown Extension API - " << params.name;
    ResponseCallbackOnError(std::move(callback), ExtensionFunction::FAILED,
                            kCreationFailed);
    return nullptr;
  }

  function->SetArgs(params.arguments.Clone());

  // Determine the source URL. When possible, prefer fetching this value from
  // the RenderFrameHost, but fallback to the value in the `params` object if
  // necessary.
  // We can't use the frame URL in the case of a worker-based request (where
  // there is no frame).
  if (is_worker_request) {
    // TODO(crbug.com/40056469): Validate this URL further. Or, better,
    // remove it from `mojom::RequestParams`.
    function->set_source_url(params.source_url);
  } else {
    DCHECK(render_frame_host_url);
    function->set_source_url(*render_frame_host_url);
  }

  function->set_has_callback(params.has_callback);
  function->set_user_gesture(params.user_gesture);
  function->set_extension(extension);
  if (params.js_callstack.has_value()) {
    function->set_js_callstack(*params.js_callstack);
  }
  function->set_response_callback(std::move(callback));
  function->set_source_context_type(context_type);
  function->set_source_process_id(requesting_process_id);
  if (is_worker_request) {
    CHECK(extension);
    WorkerId worker_id;
    worker_id.thread_id = params.worker_thread_id;
    worker_id.version_id = params.service_worker_version_id;
    worker_id.render_process_id = requesting_process_id;
    worker_id.extension_id = extension->id();
    function->set_worker_id(std::move(worker_id));
  } else {
    function->SetRenderFrameHost(render_frame_host);
  }

  // Note: `SetDispatcher()` also initializes the `browser_context_` member
  // for `ExtensionFunction`, which is necessary for properly performing
  // permission checks.
  function->SetDispatcher(weak_ptr_factory_.GetWeakPtr());

  if (!function->HasPermission()) {
    LOG(ERROR) << "Permission denied for " << params.name;
    function->RespondWithError(kCreationFailed);
    return nullptr;
  }

  return function;
}
}  // namespace extensions
