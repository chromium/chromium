/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/workers/shared_worker.h"

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/common/blob/blob_utils.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/public_url_manager.h"
#include "third_party/blink/renderer/core/messaging/message_channel.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/workers/shared_worker_client_holder.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

namespace {

void RecordSharedWorkerUsage(Document* document) {
  UseCounter::Count(document, WebFeature::kSharedWorkerStart);

  // Don't record the use counter if the frame is same-origin to the top frame,
  // or if we can't tell whether the frame was ever cross-origin or not.
  if (!document->TopFrameOrigin() ||
      document->TopFrameOrigin()->CanAccess(document->GetSecurityOrigin())) {
    return;
  }

  UseCounter::Count(document, WebFeature::kThirdPartySharedWorker);
}

}  // namespace

SharedWorker::SharedWorker(ExecutionContext* context)
    : AbstractWorker(context),
      is_being_connected_(false),
      feature_handle_for_scheduler_(context->GetScheduler()->RegisterFeature(
          SchedulingPolicy::Feature::kSharedWorker,
          {SchedulingPolicy::RecordMetricsForBackForwardCache()})) {}

SharedWorker* SharedWorker::Create(ExecutionContext* context,
                                   const String& url,
                                   const String& name,
                                   ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  // We don't currently support nested workers, so workers can only be created
  // from documents.
  Document* document = To<Document>(context);
  DCHECK(document);

  RecordSharedWorkerUsage(document);

  SharedWorker* worker = MakeGarbageCollected<SharedWorker>(context);
  worker->UpdateStateIfNeeded();

  auto* channel = MakeGarbageCollected<MessageChannel>(context);
  worker->port_ = channel->port1();
  MessagePortChannel remote_port = channel->port2()->Disentangle();

  if (!document->GetSecurityOrigin()->CanAccessSharedWorkers()) {
    exception_state.ThrowSecurityError(
        "Access to shared workers is denied to origin '" +
        document->GetSecurityOrigin()->ToString() + "'.");
    return nullptr;
  } else if (document->GetSecurityOrigin()->IsLocal()) {
    UseCounter::Count(document, WebFeature::kFileAccessedSharedWorker);
  }

  KURL script_url = ResolveURL(context, url, exception_state,
                               mojom::RequestContextType::SHARED_WORKER);
  if (script_url.IsEmpty())
    return nullptr;

  mojo::PendingRemote<mojom::blink::BlobURLToken> blob_url_token;
  if (script_url.ProtocolIs("blob")) {
    document->GetPublicURLManager().Resolve(
        script_url, blob_url_token.InitWithNewPipeAndPassReceiver());
  }

  // |name| should not be null according to the HTML spec, but the current impl
  // wrongly allows it when |name| is omitted. See TODO comment in
  // shared_worker.idl.
  // TODO(nhiroki): Stop assigning null to |name| as a default value, and remove
  // this hack.
  String worker_name = "";
  if (!name.IsNull())
    worker_name = name;

  SharedWorkerClientHolder::From(*document)->Connect(
      worker, std::move(remote_port), script_url, std::move(blob_url_token),
      worker_name);

  return worker;
}

SharedWorker::~SharedWorker() = default;

const AtomicString& SharedWorker::InterfaceName() const {
  return event_target_names::kSharedWorker;
}

bool SharedWorker::HasPendingActivity() const {
  return is_being_connected_;
}

void SharedWorker::ContextLifecycleStateChanged(
    mojom::FrameLifecycleState state) {}

void SharedWorker::Trace(blink::Visitor* visitor) {
  visitor->Trace(port_);
  AbstractWorker::Trace(visitor);
  Supplementable<SharedWorker>::Trace(visitor);
}

}  // namespace blink
