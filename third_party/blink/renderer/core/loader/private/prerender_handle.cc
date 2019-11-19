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
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/loader/prerenderer_client.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/prerender.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"

namespace blink {

// static
PrerenderHandle* PrerenderHandle::Create(Document& document,
                                         PrerenderClient* client,
                                         const KURL& url,
                                         const unsigned prerender_rel_types) {
  // Prerenders are unlike requests in most ways (for instance, they pass down
  // fragments, and they don't return data), but they do have referrers.
  if (!document.GetFrame())
    return nullptr;

  auto* prerender = MakeGarbageCollected<Prerender>(
      client, url, prerender_rel_types,
      SecurityPolicy::GenerateReferrer(document.GetReferrerPolicy(),
                                       document.GetSecurityOrigin(), url,
                                       document.OutgoingReferrer()),
      document.GetSecurityOrigin());

  PrerendererClient* prerenderer_client =
      PrerendererClient::From(document.GetPage());
  if (prerenderer_client)
    prerenderer_client->WillAddPrerender(prerender);
  prerender->Add();

  return MakeGarbageCollected<PrerenderHandle>(document, prerender);
}

PrerenderHandle::PrerenderHandle(Document& document, Prerender* prerender)
    : ContextLifecycleObserver(&document), prerender_(prerender) {}

PrerenderHandle::~PrerenderHandle() = default;

void PrerenderHandle::Dispose() {
  if (prerender_) {
    prerender_->Abandon();
    Detach();
  }
}

void PrerenderHandle::Cancel() {
  // Avoid both abandoning and canceling the same prerender. In the abandon
  // case, the LinkLoader cancels the PrerenderHandle as the Document is
  // destroyed, even through the ContextLifecycleObserver has already abandoned
  // it.
  if (!prerender_)
    return;
  prerender_->Cancel();
  Detach();
}

const KURL& PrerenderHandle::Url() const {
  return prerender_->Url();
}

void PrerenderHandle::ContextDestroyed(ExecutionContext*) {
  // A PrerenderHandle is not removed from LifecycleNotifier::m_observers until
  // the next GC runs. Thus contextDestroyed() can be called for a
  // PrerenderHandle that is already cancelled (and thus detached). In that
  // case, we should not detach the PrerenderHandle again.
  if (!prerender_)
    return;
  prerender_->Abandon();
  Detach();
}

void PrerenderHandle::Detach() {
  prerender_->Dispose();
  prerender_.Clear();
}

void PrerenderHandle::Trace(blink::Visitor* visitor) {
  visitor->Trace(prerender_);
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
