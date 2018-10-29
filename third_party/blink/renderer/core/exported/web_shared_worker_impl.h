/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EXPORTED_WEB_SHARED_WORKER_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EXPORTED_WEB_SHARED_WORKER_IMPL_H_

#include "third_party/blink/public/web/web_shared_worker.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "services/service_manager/public/mojom/interface_provider.mojom-blink.h"
#include "third_party/blink/public/common/privacy_preferences.h"
#include "third_party/blink/public/mojom/net/ip_address_space.mojom-shared.h"
#include "third_party/blink/public/platform/web_content_security_policy.h"
#include "third_party/blink/public/web/web_shared_worker_client.h"
#include "third_party/blink/public/web/worker_content_settings_proxy.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/exported/worker_shadow_page.h"
#include "third_party/blink/renderer/core/workers/shared_worker_reporting_proxy.h"
#include "third_party/blink/renderer/core/workers/worker_clients.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"

namespace base {
class SingleThreadTaskRunner;
};

namespace network {
class SharedURLLoaderFactory;
};

namespace blink {

class WebApplicationCacheHost;
class WebApplicationCacheHostClient;
class WebSharedWorkerClient;
class WebString;
class WebURL;
class WorkerClassicScriptLoader;
class WorkerInspectorProxy;

// This class is used by the worker process code to talk to the SharedWorker
// implementation. This is basically accessed on the main thread, but some
// methods must be called from a worker thread. Such methods are suffixed with
// *OnWorkerThread or have header comments.
//
// Owned by WebSharedWorkerClient.
class CORE_EXPORT WebSharedWorkerImpl final : public WebSharedWorker,
                                              public WorkerShadowPage::Client {
 public:
  explicit WebSharedWorkerImpl(WebSharedWorkerClient*);
  ~WebSharedWorkerImpl() override;

  // WorkerShadowPage::Client overrides.
  std::unique_ptr<WebApplicationCacheHost> CreateApplicationCacheHost(
      WebApplicationCacheHostClient*) override;
  void OnShadowPageInitialized() override;

  // WebDevToolsAgentImpl::Client overrides.
  void ResumeStartup() override;
  const base::UnguessableToken& GetDevToolsWorkerToken() override;

  // WebSharedWorker methods:
  void StartWorkerContext(
      const WebURL&,
      const WebString& name,
      const WebString& content_security_policy,
      WebContentSecurityPolicyType,
      mojom::IPAddressSpace,
      const base::UnguessableToken& devtools_worker_token,
      PrivacyPreferences privacy_preferences,
      scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
      mojo::ScopedMessagePipeHandle content_settings_handle,
      mojo::ScopedMessagePipeHandle interface_provider) override;
  void Connect(MessagePortChannel) override;
  void TerminateWorkerContext() override;
  void PauseWorkerContextOnStart() override;
  void BindDevToolsAgent(
      mojo::ScopedInterfaceEndpointHandle devtools_agent_host_ptr_info,
      mojo::ScopedInterfaceEndpointHandle devtools_agent_request) override;
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(TaskType) override;

  // Callback methods for SharedWorkerReportingProxy.
  void CountFeature(WebFeature);
  void PostMessageToPageInspector(int session_id, const String& message);
  void DidCloseWorkerGlobalScope();
  void DidTerminateWorkerThread();

 private:
  WorkerThread* GetWorkerThread() { return worker_thread_.get(); }

  // Shuts down the worker thread.
  void TerminateWorkerThread();

  void DidReceiveScriptLoaderResponse();
  void OnScriptLoaderFinished();
  void ContinueOnScriptLoaderFinished();

  void ConnectTaskOnWorkerThread(MessagePortChannel);

  std::unique_ptr<WorkerShadowPage> shadow_page_;
  // Unique worker token used by DevTools to attribute different instrumentation
  // to the same worker.
  base::UnguessableToken devtools_worker_token_;

  Persistent<WorkerInspectorProxy> worker_inspector_proxy_;

  Persistent<SharedWorkerReportingProxy> reporting_proxy_;
  std::unique_ptr<WorkerThread> worker_thread_;
  mojom::blink::WorkerContentSettingsProxyPtrInfo content_settings_info_;

  // |client_| owns |this|.
  WebSharedWorkerClient* client_;

  bool asked_to_terminate_ = false;
  bool pause_worker_context_on_start_ = false;
  bool is_paused_on_start_ = false;

  // Kept around only while main script loading is ongoing.
  scoped_refptr<WorkerClassicScriptLoader> main_script_loader_;

  WebURL script_request_url_;
  WebString name_;
  mojom::IPAddressSpace creation_address_space_;

  service_manager::mojom::blink::InterfaceProviderPtrInfo
      pending_interface_provider_;

  // SharedWorker can sometimes run tasks that are initiated by/associated with
  // a document's frame but these documents can be from a different process. So
  // we intentionally populate the task runners with default task runners of the
  // main thread. Note that |shadow_page_| should not be used as it's a dummy
  // document for loading that doesn't represent the frame of any associated
  // document.
  Persistent<ParentExecutionContextTaskRunners>
      parent_execution_context_task_runners_;

  base::WeakPtrFactory<WebSharedWorkerImpl> weak_ptr_factory_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EXPORTED_WEB_SHARED_WORKER_IMPL_H_
