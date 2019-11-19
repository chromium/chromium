// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_function_dispatcher.h"

#include <utility>

#include "base/bind.h"
#include "base/json/json_string_value_serializer.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/process.h"
#include "base/scoped_observer.h"
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
#include "extensions/common/extensions_client.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"

using content::BrowserThread;

namespace extensions {
namespace {

// Notifies the ApiActivityMonitor that an extension API function has been
// called. May be called from any thread.
void NotifyApiFunctionCalled(const std::string& extension_id,
                             const std::string& api_name,
                             const base::ListValue& args,
                             content::BrowserContext* browser_context) {
  activity_monitor::OnApiFunctionCalled(browser_context, extension_id, api_name,
                                        args);
}

bool IsRequestFromServiceWorker(
    const ExtensionHostMsg_Request_Params& request_params) {
  return request_params.service_worker_version_id !=
         blink::mojom::kInvalidServiceWorkerVersionId;
}

void CommonResponseCallback(IPC::Sender* ipc_sender,
                            int routing_id,
                            int worker_thread_id,
                            int request_id,
                            ExtensionFunction::ResponseType type,
                            const base::ListValue& results,
                            const std::string& error) {
  DCHECK(ipc_sender);

  if (type == ExtensionFunction::BAD_MESSAGE) {
    // The renderer will be shut down from ExtensionFunction::SetBadMessage().
    return;
  }

  if (routing_id != MSG_ROUTING_NONE) {
    DCHECK_EQ(kMainThreadId, worker_thread_id);
    ipc_sender->Send(new ExtensionMsg_Response(
        routing_id, request_id, type == ExtensionFunction::SUCCEEDED, results,
        error));
  } else {
    DCHECK_NE(kMainThreadId, worker_thread_id);
    ipc_sender->Send(new ExtensionMsg_ResponseWorker(
        worker_thread_id, request_id, type == ExtensionFunction::SUCCEEDED,
        results, error));
  }
}

}  // namespace

// TODO(http://crbug.com/980774): Simplify this or change the name now that
// IOThreadExtensionFunction is gone.
class ExtensionFunctionDispatcher::UIThreadResponseCallbackWrapper
    : public content::WebContentsObserver {
 public:
  UIThreadResponseCallbackWrapper(
      const base::WeakPtr<ExtensionFunctionDispatcher>& dispatcher,
      content::RenderFrameHost* render_frame_host)
      : content::WebContentsObserver(
            content::WebContents::FromRenderFrameHost(render_frame_host)),
        dispatcher_(dispatcher),
        render_frame_host_(render_frame_host) {}

  ~UIThreadResponseCallbackWrapper() override {}

  // content::WebContentsObserver overrides.
  void RenderFrameDeleted(
      content::RenderFrameHost* render_frame_host) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (render_frame_host != render_frame_host_)
      return;

    if (dispatcher_.get()) {
      dispatcher_->ui_thread_response_callback_wrappers_
          .erase(render_frame_host);
    }
  }

  ExtensionFunction::ResponseCallback CreateCallback(int request_id) {
    return base::Bind(
        &UIThreadResponseCallbackWrapper::OnExtensionFunctionCompleted,
        weak_ptr_factory_.GetWeakPtr(),
        request_id);
  }

 private:
  void OnExtensionFunctionCompleted(int request_id,
                                    ExtensionFunction::ResponseType type,
                                    const base::ListValue& results,
                                    const std::string& error) {
    CommonResponseCallback(render_frame_host_,
                           render_frame_host_->GetRoutingID(), kMainThreadId,
                           request_id, type, results, error);
  }

  base::WeakPtr<ExtensionFunctionDispatcher> dispatcher_;
  content::RenderFrameHost* render_frame_host_;
  base::WeakPtrFactory<UIThreadResponseCallbackWrapper> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(UIThreadResponseCallbackWrapper);
};

class ExtensionFunctionDispatcher::UIThreadWorkerResponseCallbackWrapper
    : public content::RenderProcessHostObserver {
 public:
  UIThreadWorkerResponseCallbackWrapper(
      const base::WeakPtr<ExtensionFunctionDispatcher>& dispatcher,
      content::RenderProcessHost* render_process_host,
      int worker_thread_id)
      : dispatcher_(dispatcher),
        observer_(this),
        render_process_host_(render_process_host) {
    observer_.Add(render_process_host_);

    DCHECK(ExtensionsClient::Get()
               ->ExtensionAPIEnabledInExtensionServiceWorkers());
  }

  ~UIThreadWorkerResponseCallbackWrapper() override {}

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
    return base::Bind(
        &UIThreadWorkerResponseCallbackWrapper::OnExtensionFunctionCompleted,
        weak_ptr_factory_.GetWeakPtr(), request_id, worker_thread_id);
  }

 private:
  void CleanUp() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (dispatcher_) {
      dispatcher_->RemoveWorkerCallbacksForProcess(
          render_process_host_->GetID());
    }
    // Note: we are deleted here!
  }

  void OnExtensionFunctionCompleted(int request_id,
                                    int worker_thread_id,
                                    ExtensionFunction::ResponseType type,
                                    const base::ListValue& results,
                                    const std::string& error) {
    if (type == ExtensionFunction::BAD_MESSAGE) {
      // The renderer will be shut down from ExtensionFunction::SetBadMessage().
      return;
    }
    render_process_host_->Send(new ExtensionMsg_ResponseWorker(
        worker_thread_id, request_id, type == ExtensionFunction::SUCCEEDED,
        results, error));
  }

  base::WeakPtr<ExtensionFunctionDispatcher> dispatcher_;
  ScopedObserver<content::RenderProcessHost, content::RenderProcessHostObserver>
      observer_{this};
  content::RenderProcessHost* const render_process_host_;
  base::WeakPtrFactory<UIThreadWorkerResponseCallbackWrapper> weak_ptr_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(UIThreadWorkerResponseCallbackWrapper);
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
    const ExtensionHostMsg_Request_Params& params,
    content::RenderFrameHost* render_frame_host,
    int render_process_id) {
  // Kill the renderer if it's an invalid request.
  const bool is_valid_request =
      (!render_frame_host && IsRequestFromServiceWorker(params)) ||
      (render_frame_host && !IsRequestFromServiceWorker(params));
  if (!is_valid_request) {
    bad_message::ReceivedBadMessage(render_process_id,
                                    bad_message::EFD_BAD_MESSAGE);
    return;
  }

  if (render_frame_host) {
    // Extension API from a non Service Worker context, e.g. extension page,
    // background page, content script.
    UIThreadResponseCallbackWrapperMap::const_iterator iter =
        ui_thread_response_callback_wrappers_.find(render_frame_host);
    UIThreadResponseCallbackWrapper* callback_wrapper = nullptr;
    if (iter == ui_thread_response_callback_wrappers_.end()) {
      callback_wrapper =
          new UIThreadResponseCallbackWrapper(AsWeakPtr(), render_frame_host);
      ui_thread_response_callback_wrappers_[render_frame_host] =
          base::WrapUnique(callback_wrapper);
    } else {
      callback_wrapper = iter->second.get();
    }
    DispatchWithCallbackInternal(
        params, render_frame_host, render_process_id,
        callback_wrapper->CreateCallback(params.request_id));
  } else {
    content::RenderProcessHost* rph =
        content::RenderProcessHost::FromID(render_process_id);
    // UIThreadWorkerResponseCallbackWrapper requires render process host to be
    // around.
    if (!rph)
      return;

    WorkerId worker_id{params.extension_id, render_process_id,
                       params.service_worker_version_id,
                       params.worker_thread_id};
    // Ignore if the worker has already stopped.
    if (!ProcessManager::Get(browser_context_)->HasServiceWorker(worker_id))
      return;

    WorkerResponseCallbackMapKey key(render_process_id,
                                     params.service_worker_version_id);
    UIThreadWorkerResponseCallbackWrapperMap::const_iterator iter =
        ui_thread_response_callback_wrappers_for_worker_.find(key);
    UIThreadWorkerResponseCallbackWrapper* callback_wrapper = nullptr;
    if (iter == ui_thread_response_callback_wrappers_for_worker_.end()) {
      callback_wrapper = new UIThreadWorkerResponseCallbackWrapper(
          AsWeakPtr(), rph, params.worker_thread_id);
      ui_thread_response_callback_wrappers_for_worker_[key] =
          base::WrapUnique(callback_wrapper);
    } else {
      callback_wrapper = iter->second.get();
    }
    DispatchWithCallbackInternal(
        params, nullptr, render_process_id,
        callback_wrapper->CreateCallback(params.request_id,
                                         params.worker_thread_id));
  }
}

void ExtensionFunctionDispatcher::DispatchWithCallbackInternal(
    const ExtensionHostMsg_Request_Params& params,
    content::RenderFrameHost* render_frame_host,
    int render_process_id,
    const ExtensionFunction::ResponseCallback& callback) {
  ProcessMap* process_map = ProcessMap::Get(browser_context_);
  if (!process_map)
    return;

  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context_);
  const Extension* extension =
      registry->enabled_extensions().GetByID(params.extension_id);
  if (!extension) {
    extension =
        registry->enabled_extensions().GetHostedAppByURL(params.source_url);
  }

  if (render_frame_host)
    DCHECK_EQ(render_process_id, render_frame_host->GetProcess()->GetID());

  scoped_refptr<ExtensionFunction> function = CreateExtensionFunction(
      params, extension, render_process_id, *process_map,
      ExtensionAPI::GetSharedInstance(), browser_context_, callback);
  if (!function.get())
    return;

  function->set_worker_thread_id(params.worker_thread_id);
  if (IsRequestFromServiceWorker(params)) {
    function->set_service_worker_version_id(params.service_worker_version_id);
  } else {
    function->SetRenderFrameHost(render_frame_host);
  }
  function->set_dispatcher(AsWeakPtr());
  function->set_browser_context(browser_context_);
  if (extension &&
      ExtensionsBrowserClient::Get()->CanExtensionCrossIncognito(
          extension, browser_context_)) {
    function->set_include_incognito_information(true);
  }

  if (!CheckPermissions(function.get(), params, callback))
    return;

  if (!extension) {
    if (function->source_context_type() == Feature::WEBUI_CONTEXT) {
      base::UmaHistogramSparse("Extensions.Functions.WebUICalls",
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
  std::string violation_error = quota->Assess(extension->id(),
                                              function.get(),
                                              &params.arguments,
                                              base::TimeTicks::Now());

  if (violation_error.empty()) {
    // See crbug.com/39178.
    ExtensionsBrowserClient::Get()->PermitExternalProtocolHandler();
    NotifyApiFunctionCalled(extension->id(), params.name, params.arguments,
                            browser_context_);

    // Note: Deliberately don't include external component extensions here -
    // this lets us differentiate between "built-in" extension calls and
    // external extension calls
    if (extension->location() == Manifest::COMPONENT) {
      base::UmaHistogramSparse("Extensions.Functions.ComponentExtensionCalls",
                               function->histogram_value());
    } else {
      base::UmaHistogramSparse("Extensions.Functions.ExtensionCalls",
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
  UIThreadWorkerResponseCallbackWrapperMap& map =
      ui_thread_response_callback_wrappers_for_worker_;
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

// static
bool ExtensionFunctionDispatcher::CheckPermissions(
    ExtensionFunction* function,
    const ExtensionHostMsg_Request_Params& params,
    const ExtensionFunction::ResponseCallback& callback) {
  if (!function->HasPermission()) {
    LOG(ERROR) << "Permission denied for " << params.name;
    SendAccessDenied(callback);
    return false;
  }
  return true;
}

// static
scoped_refptr<ExtensionFunction>
ExtensionFunctionDispatcher::CreateExtensionFunction(
    const ExtensionHostMsg_Request_Params& params,
    const Extension* extension,
    int requesting_process_id,
    const ProcessMap& process_map,
    ExtensionAPI* api,
    void* profile_id,
    const ExtensionFunction::ResponseCallback& callback) {
  scoped_refptr<ExtensionFunction> function =
      ExtensionFunctionRegistry::GetInstance().NewFunction(params.name);
  if (!function) {
    LOG(ERROR) << "Unknown Extension API - " << params.name;
    SendAccessDenied(callback);
    return nullptr;
  }

  function->SetArgs(params.arguments.Clone());
  function->set_source_url(params.source_url);
  function->set_request_id(params.request_id);
  function->set_has_callback(params.has_callback);
  function->set_user_gesture(params.user_gesture);
  function->set_extension(extension);
  function->set_profile_id(profile_id);
  function->set_response_callback(callback);
  function->set_source_context_type(
      process_map.GetMostLikelyContextType(extension, requesting_process_id));
  function->set_source_process_id(requesting_process_id);

  return function;
}

// static
void ExtensionFunctionDispatcher::SendAccessDenied(
    const ExtensionFunction::ResponseCallback& callback) {
  base::ListValue empty_list;
  callback.Run(ExtensionFunction::FAILED, empty_list,
               "Access to extension API denied.");
}

}  // namespace extensions
