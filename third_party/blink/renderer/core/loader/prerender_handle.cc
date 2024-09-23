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

#include "third_party/blink/renderer/core/loader/prerender_handle.h"

#include "services/network/public/mojom/referrer_policy.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"

namespace blink {

// static
PrerenderHandle* PrerenderHandle::Create(
    Document& document,
    const KURL& url,
    mojom::blink::PrerenderTriggerType trigger_type) {
  // Prerenders are unlike requests in most ways (for instance, they pass down
  // fragments, and they don't return data), but they do have referrers.

  if (!document.GetFrame())
    return nullptr;

  ExecutionContext* context = document.GetExecutionContext();
  Referrer referrer = SecurityPolicy::GenerateReferrer(
      context->GetReferrerPolicy(), url, context->OutgoingReferrer());

  // Record an origin type of the target URL.
  if (trigger_type == mojom::blink::PrerenderTriggerType::kLinkRelPrerender) {
    const SecurityOrigin* initiator_origin = context->GetSecurityOrigin();
    scoped_refptr<SecurityOrigin> prerendering_origin =
        SecurityOrigin::Create(url);
    if (prerendering_origin->IsSameOriginWith(initiator_origin)) {
      UseCounter::Count(context, WebFeature::kLinkRelPrerenderSameOrigin);
    } else if (prerendering_origin->IsSameSiteWith(initiator_origin)) {
      UseCounter::Count(context,
                        WebFeature::kLinkRelPrerenderSameSiteCrossOrigin);
    } else {
      UseCounter::Count(context, WebFeature::kLinkRelPrerenderCrossSite);
    }
  }

  auto attributes = mojom::blink::PrerenderAttributes::New();
  attributes->url = url;
  attributes->trigger_type = trigger_type;
  attributes->referrer = mojom::blink::Referrer::New(
      KURL(NullURL(), referrer.referrer), referrer.referrer_policy);
  // TODO(bokan): This is the _frame_ size, which is affected by the viewport
  // <meta> tag, and is likely not what we want to use here. For example, if a
  // page sets <meta name="viewport" content="width=42"> the frame size will
  // have width=42. The prerendered page is unlikely to share the same
  // viewport. I think this wants the size of the outermost WebView but that's
  // not currently plumbed into child renderers AFAICT.
  attributes->view_size = document.GetFrame()->GetOutermostMainFrameSize();

  HeapMojoRemote<mojom::blink::NoStatePrefetchProcessor> prefetch_processor(
      context);

  context->GetBrowserInterfaceBroker().GetInterface(
      prefetch_processor.BindNewPipeAndPassReceiver(
          context->GetTaskRunner(TaskType::kMiscPlatformAPI)));
  prefetch_processor->Start(std::move(attributes));
  return MakeGarbageCollected<PrerenderHandle>(PassKey(), context, url,
                                               std::move(prefetch_processor));
}

PrerenderHandle::PrerenderHandle(
    PassKey pass_key,
    ExecutionContext* context,
    const KURL& url,
    HeapMojoRemote<mojom::blink::NoStatePrefetchProcessor>
        remote_fetch_processor)
    : url_(url),
      remote_prefetch_processor_(std::move(remote_fetch_processor)) {}

PrerenderHandle::~PrerenderHandle() = default;

void PrerenderHandle::Cancel() {
  if (remote_prefetch_processor_.is_bound())
    remote_prefetch_processor_->Cancel();
  remote_prefetch_processor_.reset();
}

const KURL& PrerenderHandle::Url() const {
  return url_;
}

void PrerenderHandle::Trace(Visitor* visitor) const {
  visitor->Trace(remote_prefetch_processor_);
}

}  // namespace blink
