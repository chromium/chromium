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
#include "base/logging.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/public/mojom/blob/blob_url_store.mojom-blink.h"
#include "third_party/blink/public/mojom/loader/fetch_client_settings_object.mojom-blink.h"
#include "third_party/blink/public/mojom/worker/shared_worker_info.mojom-blink.h"
#include "third_party/blink/public/platform/web_content_security_policy.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_shared_worker.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/workers/shared_worker.h"
#include "third_party/blink/renderer/core/workers/shared_worker_client.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher_properties.h"

namespace blink {

const char SharedWorkerClientHolder::kSupplementName[] =
    "SharedWorkerClientHolder";

SharedWorkerClientHolder* SharedWorkerClientHolder::From(Document& document) {
  DCHECK(IsMainThread());
  SharedWorkerClientHolder* holder =
      Supplement<Document>::From<SharedWorkerClientHolder>(document);
  if (!holder) {
    holder = MakeGarbageCollected<SharedWorkerClientHolder>(document);
    Supplement<Document>::ProvideTo(document, holder);
  }
  return holder;
}

SharedWorkerClientHolder::SharedWorkerClientHolder(Document& document)
    : ContextLifecycleObserver(&document),
      task_runner_(document.GetTaskRunner(blink::TaskType::kDOMManipulation)) {
  DCHECK(IsMainThread());
  document.GetInterfaceProvider()->GetInterface(
      connector_.BindNewPipeAndPassReceiver(task_runner_));
}

void SharedWorkerClientHolder::Connect(
    SharedWorker* worker,
    MessagePortChannel port,
    const KURL& url,
    mojo::PendingRemote<mojom::blink::BlobURLToken> blob_url_token,
    const String& name) {
  DCHECK(IsMainThread());
  DCHECK(!name.IsNull());

  // TODO(estark): this is broken, as it only uses the first header
  // when multiple might have been sent. Fix by making the
  // mojom::blink::SharedWorkerInfo take a map that can contain multiple
  // headers.
  Vector<CSPHeaderAndType> headers =
      worker->GetExecutionContext()->GetContentSecurityPolicy()->Headers();
  WebString header = "";
  auto header_type = network::mojom::ContentSecurityPolicyType::kReport;
  if (headers.size() > 0) {
    header = headers[0].first;
    header_type = static_cast<network::mojom::ContentSecurityPolicyType>(
        headers[0].second);
  }

  mojom::blink::SharedWorkerInfoPtr info(mojom::blink::SharedWorkerInfo::New(
      url, name, header, header_type,
      worker->GetExecutionContext()->GetSecurityContext().AddressSpace()));

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
      outside_fetch_client_settings_object->GetInsecureRequestsPolicy() &
              kUpgradeInsecureRequests
          ? mojom::InsecureRequestsPolicy::kUpgrade
          : mojom::InsecureRequestsPolicy::kDoNotUpgrade;

  connector_->Connect(
      std::move(info),
      mojom::blink::FetchClientSettingsObject::New(
          outside_fetch_client_settings_object->GetReferrerPolicy(),
          KURL(outside_fetch_client_settings_object->GetOutgoingReferrer()),
          insecure_requests_policy),
      std::move(client),
      worker->GetExecutionContext()->IsSecureContext()
          ? mojom::SharedWorkerCreationContextType::kSecure
          : mojom::SharedWorkerCreationContextType::kNonsecure,
      port.ReleaseHandle(),
      mojo::PendingRemote<mojom::blink::BlobURLToken>(
          blob_url_token.PassPipe(), mojom::blink::BlobURLToken::Version_));
}

void SharedWorkerClientHolder::ContextDestroyed(ExecutionContext*) {
  DCHECK(IsMainThread());
  // Close mojo connections which will signal disinterest in the associated
  // shared worker.
  client_receivers_.Clear();
}

void SharedWorkerClientHolder::Trace(Visitor* visitor) {
  Supplement<Document>::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
