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

#include "third_party/blink/public/common/blob/blob_utils.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/public_url_manager.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/messaging/message_channel.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/workers/shared_worker_repository_client.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

inline SharedWorker::SharedWorker(ExecutionContext* context)
    : AbstractWorker(context), is_being_connected_(false) {}

SharedWorker* SharedWorker::Create(ExecutionContext* context,
                                   const String& url,
                                   const String& name,
                                   ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  UseCounter::Count(context, WebFeature::kSharedWorkerStart);

  SharedWorker* worker = new SharedWorker(context);

  MessageChannel* channel = MessageChannel::Create(context);
  worker->port_ = channel->port1();
  MessagePortChannel remote_port = channel->port2()->Disentangle();

  // We don't currently support nested workers, so workers can only be created
  // from documents.
  Document* document = To<Document>(context);
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

  mojom::blink::BlobURLTokenPtr blob_url_token;
  if (script_url.ProtocolIs("blob") && BlobUtils::MojoBlobURLsEnabled()) {
    document->GetPublicURLManager().Resolve(script_url,
                                            MakeRequest(&blob_url_token));
  }

  if (document->GetFrame()->Client()->GetSharedWorkerRepositoryClient()) {
    document->GetFrame()->Client()->GetSharedWorkerRepositoryClient()->Connect(
        worker, std::move(remote_port), script_url, std::move(blob_url_token),
        name);
  }

  return worker;
}

SharedWorker::~SharedWorker() = default;

const AtomicString& SharedWorker::InterfaceName() const {
  return EventTargetNames::SharedWorker;
}

bool SharedWorker::HasPendingActivity() const {
  return is_being_connected_;
}

void SharedWorker::Trace(blink::Visitor* visitor) {
  visitor->Trace(port_);
  AbstractWorker::Trace(visitor);
  Supplementable<SharedWorker>::Trace(visitor);
}

}  // namespace blink
