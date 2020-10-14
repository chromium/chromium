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

#include "third_party/blink/renderer/core/loader/private/prerender_handle.h"

#include "services/network/public/mojom/referrer_policy.mojom-blink.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/loader/private/prerender_client.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"

namespace blink {

// static
PrerenderHandle* PrerenderHandle::Create(
    Document& document,
    PrerenderClient* client,
    const KURL& url,
    mojom::blink::PrerenderRelType prerender_rel_type) {
  // Prerenders are unlike requests in most ways (for instance, they pass down
  // fragments, and they don't return data), but they do have referrers.

  if (!document.GetFrame())
    return nullptr;

  ExecutionContext* context = document.GetExecutionContext();
  Referrer referrer = SecurityPolicy::GenerateReferrer(
      context->GetReferrerPolicy(), url, context->OutgoingReferrer());

  auto attributes = mojom::blink::PrerenderAttributes::New();
  attributes->url = url;
  attributes->rel_type = prerender_rel_type;
  attributes->referrer = mojom::blink::Referrer::New(
      KURL(NullURL(), referrer.referrer), referrer.referrer_policy);
  attributes->view_size =
      gfx::Size(document.GetFrame()->GetMainFrameViewportSize());

  HeapMojoRemote<mojom::blink::PrerenderProcessor> prerender_processor(context);
  context->GetBrowserInterfaceBroker().GetInterface(
      prerender_processor.BindNewPipeAndPassReceiver(
          context->GetTaskRunner(TaskType::kMiscPlatformAPI)));

  mojo::PendingRemote<mojom::blink::PrerenderProcessorClient>
      prerender_processor_client;
  auto receiver = prerender_processor_client.InitWithNewPipeAndPassReceiver();

  prerender_processor->Start(std::move(attributes),
                             std::move(prerender_processor_client));

  return MakeGarbageCollected<PrerenderHandle>(PassKey(), context, client, url,
                                               std::move(prerender_processor),
                                               std::move(receiver));
}

PrerenderHandle::PrerenderHandle(
    PassKey pass_key,
    ExecutionContext* context,
    PrerenderClient* client,
    const KURL& url,
    HeapMojoRemote<mojom::blink::PrerenderProcessor> remote_processor,
    mojo::PendingReceiver<mojom::blink::PrerenderProcessorClient> receiver)
    : url_(url),
      client_(client),
      remote_processor_(std::move(remote_processor)),
      receiver_(this, context) {
  receiver_.Bind(std::move(receiver),
                 context->GetTaskRunner(TaskType::kMiscPlatformAPI));
}

PrerenderHandle::~PrerenderHandle() = default;

void PrerenderHandle::Cancel() {
  if (remote_processor_.is_bound())
    remote_processor_->Cancel();
  remote_processor_.reset();
  receiver_.reset();
}

const KURL& PrerenderHandle::Url() const {
  return url_;
}

void PrerenderHandle::OnPrerenderStart() {
  if (client_)
    client_->DidStartPrerender();
}

void PrerenderHandle::OnPrerenderStopLoading() {
  if (client_)
    client_->DidSendLoadForPrerender();
}

void PrerenderHandle::OnPrerenderDomContentLoaded() {
  if (client_)
    client_->DidSendDOMContentLoadedForPrerender();
}

void PrerenderHandle::OnPrerenderStop() {
  if (client_)
    client_->DidStopPrerender();
}

void PrerenderHandle::Trace(Visitor* visitor) const {
  visitor->Trace(client_);
  visitor->Trace(remote_processor_);
  visitor->Trace(receiver_);
}

}  // namespace blink
