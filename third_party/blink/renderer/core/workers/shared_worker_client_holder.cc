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

#include "third_party/blink/renderer/core/workers/shared_worker_client_holder.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/public/common/security_context/insecure_request_policy.h"
#include "third_party/blink/public/mojom/blob/blob_url_store.mojom-blink.h"
#include "third_party/blink/public/mojom/loader/fetch_client_settings_object.mojom-blink.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom-blink.h"
#include "third_party/blink/public/mojom/worker/shared_worker_info.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_shared_worker.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fetch/request.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/script/script.h"
#include "third_party/blink/renderer/core/workers/shared_worker.h"
#include "third_party/blink/renderer/core/workers/shared_worker_client.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher_properties.h"

namespace blink {

const char SharedWorkerClientHolder::kSupplementName[] =
    "SharedWorkerClientHolder";

SharedWorkerClientHolder* SharedWorkerClientHolder::From(
    LocalDOMWindow& window) {
  DCHECK(IsMainThread());
  SharedWorkerClientHolder* holder =
      Supplement<LocalDOMWindow>::From<SharedWorkerClientHolder>(window);
  if (!holder) {
    holder = MakeGarbageCollected<SharedWorkerClientHolder>(window);
    Supplement<LocalDOMWindow>::ProvideTo(window, holder);
  }
  return holder;
}

SharedWorkerClientHolder::SharedWorkerClientHolder(LocalDOMWindow& window)
    : Supplement(window),
      connector_(&window),
      client_receivers_(&window),
      task_runner_(window.GetTaskRunner(blink::TaskType::kDOMManipulation)) {
  DCHECK(IsMainThread());
  window.GetBrowserInterfaceBroker().GetInterface(
      connector_.BindNewPipeAndPassReceiver(task_runner_));
}

void SharedWorkerClientHolder::Connect(
    SharedWorker* worker,
    MessagePortChannel port,
    const KURL& url,
    mojo::PendingRemote<mojom::blink::BlobURLToken> blob_url_token,
    mojom::blink::WorkerOptionsPtr options,
    mojom::blink::SharedWorkerSameSiteCookies same_site_cookies,
    ukm::SourceId client_ukm_source_id,
    const HeapMojoRemote<mojom::blink::SharedWorkerConnector>*
        connector_override) {
  DCHECK(IsMainThread());
  DCHECK(options);

  mojo::PendingRemote<mojom::blink::SharedWorkerClient> client;
  client_receivers_.Add(std::make_unique<SharedWorkerClient>(worker),
                        client.InitWithNewPipeAndPassReceiver(), task_runner_);

  auto* outside_fetch_client_settings_object =
      MakeGarbageCollected<FetchClientSettingsObjectSnapshot>(
          worker->GetExecutionContext()
              ->Fetcher()
              ->GetProperties()
              .GetFetchClientSettingsObject());

  mojom::InsecureRequestsPolicy insecure_requests_policy =
      (outside_fetch_client_settings_object->GetInsecureRequestsPolicy() &
       mojom::blink::InsecureRequestPolicy::kUpgradeInsecureRequests) !=
              mojom::blink::InsecureRequestPolicy::kLeaveInsecureRequestsAlone
          ? mojom::InsecureRequestsPolicy::kUpgrade
          : mojom::InsecureRequestsPolicy::kDoNotUpgrade;

  auto info = mojom::blink::SharedWorkerInfo::New(
      url, std::move(options),
      mojo::Clone(worker->GetExecutionContext()
                      ->GetContentSecurityPolicy()
                      ->GetParsedPolicies()),
      mojom::blink::FetchClientSettingsObject::New(
          outside_fetch_client_settings_object->GetReferrerPolicy(),
          KURL(outside_fetch_client_settings_object->GetOutgoingReferrer()),
          insecure_requests_policy),
      same_site_cookies);

  const HeapMojoRemote<mojom::blink::SharedWorkerConnector>& connector =
      connector_override ? *connector_override : connector_;
  connector->Connect(
      std::move(info), std::move(client),
      worker->GetExecutionContext()->IsSecureContext()
          ? mojom::blink::SharedWorkerCreationContextType::kSecure
          : mojom::blink::SharedWorkerCreationContextType::kNonsecure,
      port.ReleaseHandle(), std::move(blob_url_token), client_ukm_source_id);
}

void SharedWorkerClientHolder::Trace(Visitor* visitor) const {
  visitor->Trace(connector_);
  visitor->Trace(client_receivers_);
  Supplement<LocalDOMWindow>::Trace(visitor);
}

}  // namespace blink
