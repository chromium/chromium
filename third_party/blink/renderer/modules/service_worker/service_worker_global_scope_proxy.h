/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SERVICE_WORKER_SERVICE_WORKER_GLOBAL_SCOPE_PROXY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SERVICE_WORKER_SERVICE_WORKER_GLOBAL_SCOPE_PROXY_H_

#include <memory>

#include "base/macros.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/modules/service_worker/web_service_worker_context_proxy.h"
#include "third_party/blink/renderer/core/workers/worker_reporting_proxy.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace blink {

class FetchEvent;
class ParentExecutionContextTaskRunners;
class ServiceWorkerGlobalScope;
class WebEmbeddedWorkerImpl;
class WebServiceWorkerContextClient;
struct WebServiceWorkerError;
class WebServiceWorkerRequest;
class WebURLResponse;

// This class is created and destructed on the main thread, but live most
// of its time as a resident of the worker thread. All methods other than its
// ctor/dtor and Detach() are called on the worker thread.
//
// This implements WebServiceWorkerContextProxy, which connects ServiceWorker's
// WorkerGlobalScope and embedder/chrome, and implements ServiceWorker-specific
// events/upcall methods that are to be called by embedder/chromium,
// e.g. onfetch.
//
// An instance of this class is supposed to outlive until
// workerThreadTerminated() is called by its corresponding
// WorkerGlobalScope.
class ServiceWorkerGlobalScopeProxy final
    : public GarbageCollectedFinalized<ServiceWorkerGlobalScopeProxy>,
      public WebServiceWorkerContextProxy,
      public WorkerReportingProxy {
 public:
  static ServiceWorkerGlobalScopeProxy* Create(WebEmbeddedWorkerImpl&,
                                               WebServiceWorkerContextClient&);
  ~ServiceWorkerGlobalScopeProxy() override;

  // WebServiceWorkerContextProxy overrides:
  void BindServiceWorkerHost(
      mojo::ScopedInterfaceEndpointHandle service_worker_host) override;
  void SetRegistration(WebServiceWorkerRegistrationObjectInfo info) override;
  // Must be called after the above BindServiceWorkerHost() and
  // SetRegistration() got called.
  void ReadyToEvaluateScript() override;
  void DispatchActivateEvent(int) override;
  void DispatchBackgroundFetchAbortEvent(
      int event_id,
      const WebBackgroundFetchRegistration& registration) override;
  void DispatchBackgroundFetchClickEvent(
      int event_id,
      const WebBackgroundFetchRegistration& registration) override;
  void DispatchBackgroundFetchFailEvent(
      int event_id,
      const WebBackgroundFetchRegistration& registration) override;
  void DispatchBackgroundFetchSuccessEvent(
      int event_id,
      const WebBackgroundFetchRegistration& registration) override;
  void DispatchCookieChangeEvent(
      int event_id,
      const WebCanonicalCookie& cookie,
      network::mojom::CookieChangeCause change_cause) override;
  void DispatchExtendableMessageEvent(
      int event_id,
      TransferableMessage,
      const WebSecurityOrigin& source_origin,
      const WebServiceWorkerClientInfo&) override;
  void DispatchExtendableMessageEvent(int event_id,
                                      TransferableMessage,
                                      const WebSecurityOrigin& source_origin,
                                      WebServiceWorkerObjectInfo) override;
  void DispatchFetchEvent(int fetch_event_id,
                          const WebServiceWorkerRequest&,
                          bool navigation_preload_sent) override;
  void DispatchInstallEvent(int) override;
  void DispatchNotificationClickEvent(int,
                                      const WebString& notification_id,
                                      const WebNotificationData&,
                                      int action_index,
                                      const WebString& reply) override;
  void DispatchNotificationCloseEvent(int,
                                      const WebString& notification_id,
                                      const WebNotificationData&) override;
  void DispatchPushEvent(int, const WebString& data) override;
  void DispatchSyncEvent(int, const WebString& tag, bool last_chance) override;
  void DispatchAbortPaymentEvent(int) override;
  void DispatchCanMakePaymentEvent(int,
                                   const WebCanMakePaymentEventData&) override;
  void DispatchPaymentRequestEvent(int,
                                   const WebPaymentRequestEventData&) override;
  bool HasFetchEventHandler() override;
  void OnNavigationPreloadResponse(
      int fetch_event_id,
      std::unique_ptr<WebURLResponse>,
      mojo::ScopedDataPipeConsumerHandle data_pipe) override;
  void OnNavigationPreloadError(
      int fetch_event_id,
      std::unique_ptr<WebServiceWorkerError>) override;
  void OnNavigationPreloadComplete(int fetch_event_id,
                                   TimeTicks completion_time,
                                   int64_t encoded_data_length,
                                   int64_t encoded_body_length,
                                   int64_t decoded_body_length) override;

  // WorkerReportingProxy overrides:
  void CountFeature(WebFeature) override;
  void CountDeprecation(WebFeature) override;
  void ReportException(const String& error_message,
                       std::unique_ptr<SourceLocation>,
                       int exception_id) override;
  void ReportConsoleMessage(MessageSource,
                            MessageLevel,
                            const String& message,
                            SourceLocation*) override;
  void PostMessageToPageInspector(int session_id, const String&) override;
  void DidCreateWorkerGlobalScope(WorkerOrWorkletGlobalScope*) override;
  void DidInitializeWorkerContext() override;
  void DidLoadInstalledScript() override;
  void WillEvaluateClassicScript(size_t script_size,
                                 size_t cached_metadata_size) override;
  void WillEvaluateImportedClassicScript(size_t script_size,
                                         size_t cached_metadata_size) override;
  void WillEvaluateModuleScript() override;
  void DidEvaluateClassicScript(bool success) override;
  void DidEvaluateModuleScript(bool success) override;
  void DidCloseWorkerGlobalScope() override;
  void WillDestroyWorkerGlobalScope() override;
  void DidTerminateWorkerThread() override;

  void Trace(blink::Visitor*);

  // Detaches this proxy object entirely from the outside world, clearing out
  // all references.
  //
  // It is called on the main thread during WebEmbeddedWorkerImpl finalization
  // _after_ the worker thread using the proxy has been terminated.
  void Detach();

  void TerminateWorkerContext();

 private:
  ServiceWorkerGlobalScopeProxy(WebEmbeddedWorkerImpl&,
                                WebServiceWorkerContextClient&);

  WebServiceWorkerContextClient& Client() const;
  ServiceWorkerGlobalScope* WorkerGlobalScope() const;

  // Non-null until the WebEmbeddedWorkerImpl explicitly detach()es
  // as part of its finalization.
  WebEmbeddedWorkerImpl* embedded_worker_;

  Member<ParentExecutionContextTaskRunners>
      parent_execution_context_task_runners_;

  // The worker thread uses this map to track |FetchEvent|s created
  // on the worker thread (heap.) But as the proxy object is created
  // on the main thread & its heap, we must use a cross-heap reference
  // to each |FetchEvent| so as to obey the "per-thread heap rule" that
  // a heap should only have per-thread heap references. Keeping a
  // cross-heap reference requires the use of a CrossThreadPersistent<>
  // to remain safe and sound.
  //
  HashMap<int, CrossThreadPersistent<FetchEvent>> pending_preload_fetch_events_;

  WebServiceWorkerContextClient* client_;

  CrossThreadPersistent<ServiceWorkerGlobalScope> worker_global_scope_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerGlobalScopeProxy);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SERVICE_WORKER_SERVICE_WORKER_GLOBAL_SCOPE_PROXY_H_
