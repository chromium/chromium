/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SERVICE_WORKER_SERVICE_WORKER_GLOBAL_SCOPE_CLIENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SERVICE_WORKER_SERVICE_WORKER_GLOBAL_SCOPE_CLIENT_H_

#include <memory>

#include "base/macros.h"
#include "third_party/blink/public/common/messaging/transferable_message.h"
#include "third_party/blink/public/mojom/service_worker/service_worker.mojom-blink.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_event_status.mojom-blink.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_stream_handle.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/core/workers/worker_clients.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

struct WebPaymentHandlerResponse;
class ExecutionContext;
class KURL;
class ScriptPromiseResolver;
class WebServiceWorkerContextClient;
class WebServiceWorkerResponse;
class WorkerClients;

// See WebServiceWorkerContextClient for documentation for the methods in this
// class.
class MODULES_EXPORT ServiceWorkerGlobalScopeClient final
    : public GarbageCollectedFinalized<ServiceWorkerGlobalScopeClient>,
      public Supplement<WorkerClients> {
  USING_GARBAGE_COLLECTED_MIXIN(ServiceWorkerGlobalScopeClient);

 public:
  using ClaimCallback = mojom::blink::ServiceWorkerHost::ClaimClientsCallback;
  using SkipWaitingCallback =
      mojom::blink::ServiceWorkerHost::SkipWaitingCallback;
  using GetClientCallback = mojom::blink::ServiceWorkerHost::GetClientCallback;
  using GetClientsCallback =
      mojom::blink::ServiceWorkerHost::GetClientsCallback;
  using FocusCallback = mojom::blink::ServiceWorkerHost::FocusClientCallback;

  static const char kSupplementName[];

  explicit ServiceWorkerGlobalScopeClient(WebServiceWorkerContextClient&);

  // Called from ServiceWorkerClients.
  void GetClient(const String&, GetClientCallback);
  void GetClients(mojom::blink::ServiceWorkerClientQueryOptionsPtr,
                  GetClientsCallback);
  void OpenWindowForClients(const KURL&, ScriptPromiseResolver*);
  void OpenWindowForPaymentHandler(const KURL&, ScriptPromiseResolver*);
  void SetCachedMetadata(const KURL&, const char*, size_t);
  void ClearCachedMetadata(const KURL&);
  void PostMessageToClient(const String& client_uuid, BlinkTransferableMessage);
  void SkipWaiting(SkipWaitingCallback);
  void Claim(ClaimCallback);
  void Focus(const String& client_uuid, FocusCallback);
  void Navigate(const String& client_uuid, const KURL&, ScriptPromiseResolver*);

  void DidHandleActivateEvent(int event_id,
                              mojom::ServiceWorkerEventStatus,
                              base::TimeTicks event_dispatch_time);
  void DidHandleBackgroundFetchAbortEvent(int event_id,
                                          mojom::ServiceWorkerEventStatus,
                                          base::TimeTicks event_dispatch_time);
  void DidHandleBackgroundFetchClickEvent(int event_id,
                                          mojom::ServiceWorkerEventStatus,
                                          base::TimeTicks event_dispatch_time);
  void DidHandleBackgroundFetchFailEvent(int event_id,
                                         mojom::ServiceWorkerEventStatus,
                                         base::TimeTicks event_dispatch_time);
  void DidHandleBackgroundFetchSuccessEvent(
      int event_id,
      mojom::ServiceWorkerEventStatus,
      base::TimeTicks event_dispatch_time);
  void DidHandleCookieChangeEvent(int event_id,
                                  mojom::ServiceWorkerEventStatus,
                                  base::TimeTicks event_dispatch_time);
  void DidHandleExtendableMessageEvent(int event_id,
                                       mojom::ServiceWorkerEventStatus,
                                       base::TimeTicks event_dispatch_time);
  void RespondToFetchEventWithNoResponse(
      int fetch_event_id,
      base::TimeTicks event_dispatch_time,
      base::TimeTicks respond_with_settled_time);
  void RespondToFetchEvent(int fetch_event_id,
                           const WebServiceWorkerResponse&,
                           base::TimeTicks event_dispatch_time,
                           base::TimeTicks respond_with_settled_time);
  void RespondToFetchEventWithResponseStream(
      int fetch_event_id,
      const WebServiceWorkerResponse&,
      WebServiceWorkerStreamHandle*,
      base::TimeTicks event_dispatch_time,
      base::TimeTicks respond_with_settled_time);
  void RespondToAbortPaymentEvent(int event_id,
                                  bool abort_payment,
                                  base::TimeTicks event_dispatch_time);
  void RespondToCanMakePaymentEvent(int event_id,
                                    bool can_make_payment,
                                    base::TimeTicks event_dispatch_time);
  void RespondToPaymentRequestEvent(int event_id,
                                    const WebPaymentHandlerResponse&,
                                    base::TimeTicks event_dispatch_time);
  void DidHandleFetchEvent(int fetch_event_id,
                           mojom::ServiceWorkerEventStatus,
                           base::TimeTicks event_dispatch_time);
  void DidHandleInstallEvent(int install_event_id,
                             mojom::ServiceWorkerEventStatus,
                             base::TimeTicks event_dispatch_time);
  void DidHandleNotificationClickEvent(int event_id,
                                       mojom::ServiceWorkerEventStatus,
                                       base::TimeTicks event_dispatch_time);
  void DidHandleNotificationCloseEvent(int event_id,
                                       mojom::ServiceWorkerEventStatus,
                                       base::TimeTicks event_dispatch_time);
  void DidHandlePushEvent(int push_event_id,
                          mojom::ServiceWorkerEventStatus,
                          base::TimeTicks event_dispatch_time);
  void DidHandleSyncEvent(int sync_event_id,
                          mojom::ServiceWorkerEventStatus,
                          base::TimeTicks event_dispatch_time);
  void DidHandleAbortPaymentEvent(int abort_payment_event_id,
                                  mojom::ServiceWorkerEventStatus,
                                  base::TimeTicks event_dispatch_time);
  void DidHandleCanMakePaymentEvent(int payment_request_event_id,
                                    mojom::ServiceWorkerEventStatus,
                                    base::TimeTicks event_dispatch_time);
  void DidHandlePaymentRequestEvent(int payment_request_event_id,
                                    mojom::ServiceWorkerEventStatus,
                                    base::TimeTicks event_dispatch_time);

  void BindServiceWorkerHost(
      mojom::blink::ServiceWorkerHostAssociatedPtrInfo service_worker_host);

  void WillDestroyWorkerContext();

  static ServiceWorkerGlobalScopeClient* From(ExecutionContext*);

  void Trace(blink::Visitor*) override;

 private:
  WebServiceWorkerContextClient& client_;

  // Lives on the service worker thread, is bound by BindServiceWorkerHost()
  // which is triggered by the first Mojo call received on the service worker
  // thread content::mojom::ServiceWorker::InitializeGlobalScope(), and is
  // closed by WillDestroyWorkerContext().
  mojom::blink::ServiceWorkerHostAssociatedPtr service_worker_host_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerGlobalScopeClient);
};

MODULES_EXPORT void ProvideServiceWorkerGlobalScopeClientToWorker(
    WorkerClients*,
    ServiceWorkerGlobalScopeClient*);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SERVICE_WORKER_SERVICE_WORKER_GLOBAL_SCOPE_CLIENT_H_
