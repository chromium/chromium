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

#include "third_party/blink/renderer/core/exported/shared_worker_repository_client_impl.h"

#include <memory>
#include <utility>
#include "third_party/blink/public/platform/web_content_security_policy.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_shared_worker.h"
#include "third_party/blink/public/web/web_shared_worker_connect_listener.h"
#include "third_party/blink/public/web/web_shared_worker_repository_client.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/workers/shared_worker.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"

namespace blink {
namespace {

mojom::SharedWorkerCreationContextType ToCreationContextType(
    bool is_secure_context) {
  return is_secure_context ? mojom::SharedWorkerCreationContextType::kSecure
                           : mojom::SharedWorkerCreationContextType::kNonsecure;
}

}  // namespace

// Implementation of the callback interface passed to the embedder. This will be
// destructed when a connection to a shared worker is established.
class SharedWorkerConnectListener final
    : public WebSharedWorkerConnectListener {
 public:
  explicit SharedWorkerConnectListener(SharedWorker* worker)
      : worker_(worker) {}

  ~SharedWorkerConnectListener() override {
    // We have lost our connection to the worker. If this happens before
    // Connected() is called, then it suggests that the document is gone or
    // going away.
  }

  // WebSharedWorkerConnectListener overrides.

  void WorkerCreated(
      mojom::SharedWorkerCreationContextType creation_context_type) override {
    worker_->SetIsBeingConnected(true);

    // No nested workers (for now) - connect() should only be called from
    // document context.
    DCHECK(worker_->GetExecutionContext()->IsDocument());
    DCHECK_EQ(creation_context_type,
              ToCreationContextType(
                  worker_->GetExecutionContext()->IsSecureContext()));
  }

  void ScriptLoadFailed() override {
    worker_->DispatchEvent(*Event::CreateCancelable(EventTypeNames::error));
    worker_->SetIsBeingConnected(false);
  }

  void Connected() override { worker_->SetIsBeingConnected(false); }

  void CountFeature(WebFeature feature) override {
    UseCounter::Count(worker_->GetExecutionContext(), feature);
  }

  Persistent<SharedWorker> worker_;
};

static WebSharedWorkerRepositoryClient::DocumentID GetId(void* document) {
  DCHECK(document);
  return reinterpret_cast<WebSharedWorkerRepositoryClient::DocumentID>(
      document);
}

void SharedWorkerRepositoryClientImpl::Connect(
    SharedWorker* worker,
    MessagePortChannel port,
    const KURL& url,
    mojom::blink::BlobURLTokenPtr blob_url_token,
    const String& name) {
  DCHECK(client_);

  // No nested workers (for now) - connect() should only be called from document
  // context.
  Document* document = To<Document>(worker->GetExecutionContext());

  // TODO(estark): this is broken, as it only uses the first header
  // when multiple might have been sent. Fix by making the
  // SharedWorkerConnectListener interface take a map that can contain
  // multiple headers.
  Vector<CSPHeaderAndType> headers =
      worker->GetExecutionContext()->GetContentSecurityPolicy()->Headers();
  WebString header;
  WebContentSecurityPolicyType header_type =
      kWebContentSecurityPolicyTypeReport;

  if (headers.size() > 0) {
    header = headers[0].first;
    header_type = static_cast<WebContentSecurityPolicyType>(headers[0].second);
  }

  bool is_secure_context = worker->GetExecutionContext()->IsSecureContext();
  std::unique_ptr<WebSharedWorkerConnectListener> listener =
      std::make_unique<SharedWorkerConnectListener>(worker);
  client_->Connect(
      url, name, GetId(document), header, header_type,
      worker->GetExecutionContext()->GetSecurityContext().AddressSpace(),
      ToCreationContextType(is_secure_context), std::move(port),
      blob_url_token.PassInterface().PassHandle(), std::move(listener));
}

void SharedWorkerRepositoryClientImpl::DocumentDetached(Document* document) {
  DCHECK(client_);
  client_->DocumentDetached(GetId(document));
}

SharedWorkerRepositoryClientImpl::SharedWorkerRepositoryClientImpl(
    WebSharedWorkerRepositoryClient* client)
    : client_(client) {}

}  // namespace blink
