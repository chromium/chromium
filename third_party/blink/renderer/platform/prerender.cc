/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
 *
 */

#include "third_party/blink/renderer/platform/prerender.h"

#include "third_party/blink/public/platform/web_prerender.h"
#include "third_party/blink/public/platform/web_prerendering_support.h"
#include "third_party/blink/renderer/platform/prerender_client.h"

namespace blink {

Prerender::Prerender(PrerenderClient* client,
                     const KURL& url,
                     const unsigned rel_types,
                     const Referrer& referrer,
                     const SecurityOrigin* security_origin)
    : client_(client),
      url_(url),
      rel_types_(rel_types),
      referrer_(referrer),
      security_origin_(security_origin) {}

Prerender::~Prerender() = default;

void Prerender::Trace(blink::Visitor* visitor) {
  visitor->Trace(client_);
}

void Prerender::Dispose() {
  client_ = nullptr;
  extra_data_ = nullptr;
}

void Prerender::Add() {
  if (WebPrerenderingSupport* platform = WebPrerenderingSupport::Current())
    platform->Add(WebPrerender(this));
}

void Prerender::Cancel() {
  if (WebPrerenderingSupport* platform = WebPrerenderingSupport::Current())
    platform->Cancel(WebPrerender(this));
}

void Prerender::Abandon() {
  if (WebPrerenderingSupport* platform = WebPrerenderingSupport::Current())
    platform->Abandon(WebPrerender(this));
}

void Prerender::DidStartPrerender() {
  if (client_)
    client_->DidStartPrerender();
}

void Prerender::DidStopPrerender() {
  if (client_)
    client_->DidStopPrerender();
}

void Prerender::DidSendLoadForPrerender() {
  if (client_)
    client_->DidSendLoadForPrerender();
}

void Prerender::DidSendDOMContentLoadedForPrerender() {
  if (client_)
    client_->DidSendDOMContentLoadedForPrerender();
}

}  // namespace blink
