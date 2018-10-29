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

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_SERVICE_WORKER_WEB_SERVICE_WORKER_CONTEXT_PROXY_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_SERVICE_WORKER_WEB_SERVICE_WORKER_CONTEXT_PROXY_H_

#include "base/time/time.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "third_party/blink/public/common/messaging/transferable_message.h"
#include "third_party/blink/public/platform/modules/background_fetch/background_fetch.mojom-shared.h"
#include "third_party/blink/public/platform/modules/background_fetch/web_background_fetch_registration.h"
#include "third_party/blink/public/platform/web_canonical_cookie.h"

#include <memory>

namespace blink {

struct WebCanMakePaymentEventData;
class WebSecurityOrigin;
class WebServiceWorkerRequest;
class WebString;
struct WebNotificationData;
struct WebPaymentRequestEventData;
struct WebServiceWorkerClientInfo;
struct WebServiceWorkerError;
struct WebServiceWorkerObjectInfo;
struct WebServiceWorkerRegistrationObjectInfo;
class WebURLResponse;

// A proxy interface to talk to the worker's GlobalScope implementation.
// All methods of this class must be called on the worker thread.
class WebServiceWorkerContextProxy {
 public:
  virtual ~WebServiceWorkerContextProxy() = default;

  virtual void BindServiceWorkerHost(
      mojo::ScopedInterfaceEndpointHandle service_worker_host) = 0;

  virtual void SetRegistration(WebServiceWorkerRegistrationObjectInfo) = 0;

  // Script evaluation does not start until this function is called.
  virtual void ReadyToEvaluateScript() = 0;

  virtual void DispatchActivateEvent(int event_id) = 0;

  virtual void DispatchBackgroundFetchAbortEvent(
      int event_id,
      const WebBackgroundFetchRegistration& registration) = 0;
  virtual void DispatchBackgroundFetchClickEvent(
      int event_id,
      const WebBackgroundFetchRegistration& registration) = 0;
  virtual void DispatchBackgroundFetchFailEvent(
      int event_id,
      const WebBackgroundFetchRegistration& registration) = 0;
  virtual void DispatchBackgroundFetchSuccessEvent(
      int event_id,
      const WebBackgroundFetchRegistration& registration) = 0;
  virtual void DispatchCookieChangeEvent(
      int event_id,
      const WebCanonicalCookie& cookie,
      network::mojom::CookieChangeCause change_cause) = 0;
  virtual void DispatchExtendableMessageEvent(
      int event_id,
      TransferableMessage,
      const WebSecurityOrigin& source_origin,
      const WebServiceWorkerClientInfo&) = 0;
  virtual void DispatchExtendableMessageEvent(
      int event_id,
      TransferableMessage,
      const WebSecurityOrigin& source_origin,
      WebServiceWorkerObjectInfo) = 0;
  virtual void DispatchInstallEvent(int event_id) = 0;
  virtual void DispatchFetchEvent(int fetch_event_id,
                                  const WebServiceWorkerRequest& web_request,
                                  bool navigation_preload_sent) = 0;
  virtual void DispatchNotificationClickEvent(int event_id,
                                              const WebString& notification_id,
                                              const WebNotificationData&,
                                              int action_index,
                                              const WebString& reply) = 0;
  virtual void DispatchNotificationCloseEvent(int event_id,
                                              const WebString& notification_id,
                                              const WebNotificationData&) = 0;
  virtual void DispatchPushEvent(int event_id, const WebString& data) = 0;

  virtual bool HasFetchEventHandler() = 0;

  // Once the ServiceWorker has finished handling the sync event,
  // didHandleSyncEvent is called on the context client.
  virtual void DispatchSyncEvent(int sync_event_id,
                                 const WebString& tag,
                                 bool last_chance) = 0;

  virtual void DispatchAbortPaymentEvent(int event_id) = 0;

  virtual void DispatchCanMakePaymentEvent(
      int event_id,
      const WebCanMakePaymentEventData&) = 0;

  virtual void DispatchPaymentRequestEvent(
      int event_id,
      const WebPaymentRequestEventData&) = 0;

  virtual void OnNavigationPreloadResponse(
      int fetch_event_id,
      std::unique_ptr<WebURLResponse>,
      mojo::ScopedDataPipeConsumerHandle) = 0;
  virtual void OnNavigationPreloadError(
      int fetch_event_id,
      std::unique_ptr<WebServiceWorkerError>) = 0;
  virtual void OnNavigationPreloadComplete(int fetch_event_id,
                                           base::TimeTicks completion_time,
                                           int64_t encoded_data_length,
                                           int64_t encoded_body_length,
                                           int64_t decoded_body_length) = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_SERVICE_WORKER_WEB_SERVICE_WORKER_CONTEXT_PROXY_H_
