// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SERVICE_WORKER_FETCH_RESPOND_WITH_OBSERVER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SERVICE_WORKER_FETCH_RESPOND_WITH_OBSERVER_H_

#include "services/network/public/mojom/fetch_api.mojom-blink.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/service_worker/respond_with_observer.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

class ExecutionContext;
class ScriptValue;
class WaitUntilObserver;

// This class observes the service worker's handling of a FetchEvent and
// notifies the client.
class MODULES_EXPORT FetchRespondWithObserver : public RespondWithObserver {
 public:
  ~FetchRespondWithObserver() override = default;

  static FetchRespondWithObserver* Create(
      ExecutionContext*,
      int fetch_event_id,
      const KURL& request_url,
      network::mojom::FetchRequestMode,
      network::mojom::FetchRedirectMode,
      network::mojom::RequestContextFrameType,
      mojom::RequestContextType,
      WaitUntilObserver*);

  void OnResponseRejected(mojom::ServiceWorkerResponseError) override;
  void OnResponseFulfilled(const ScriptValue&,
                           ExceptionState::ContextType context_type,
                           const char* interface_name,
                           const char* property_name) override;
  void OnNoResponse() override;

  void Trace(blink::Visitor*) override;

 protected:
  FetchRespondWithObserver(ExecutionContext*,
                           int fetch_event_id,
                           const KURL& request_url,
                           network::mojom::FetchRequestMode,
                           network::mojom::FetchRedirectMode,
                           network::mojom::RequestContextFrameType,
                           mojom::RequestContextType,
                           WaitUntilObserver*);

 private:
  const KURL request_url_;
  const network::mojom::FetchRequestMode request_mode_;
  const network::mojom::FetchRedirectMode redirect_mode_;
  const network::mojom::RequestContextFrameType frame_type_;
  const mojom::RequestContextType request_context_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SERVICE_WORKER_FETCH_RESPOND_WITH_OBSERVER_H_
