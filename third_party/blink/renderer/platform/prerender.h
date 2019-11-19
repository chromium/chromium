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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_PRERENDER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_PRERENDER_H_

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/referrer.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class PrerenderClient;

class PLATFORM_EXPORT Prerender final : public GarbageCollected<Prerender> {
  DISALLOW_COPY_AND_ASSIGN(Prerender);

 public:
  class ExtraData : public RefCounted<ExtraData> {
   public:
    virtual ~ExtraData() = default;
  };

  Prerender(PrerenderClient*,
            const KURL&,
            unsigned rel_types,
            const Referrer&,
            const SecurityOrigin* security_origin);
  ~Prerender();
  void Trace(blink::Visitor*);

  void Dispose();

  void Add();
  void Cancel();
  void Abandon();

  const KURL& Url() const { return url_; }
  unsigned RelTypes() const { return rel_types_; }
  const String& GetReferrer() const { return referrer_.referrer; }
  network::mojom::ReferrerPolicy GetReferrerPolicy() const {
    return referrer_.referrer_policy;
  }
  const SecurityOrigin* GetSecurityOrigin() const { return security_origin_; }

  void SetExtraData(scoped_refptr<ExtraData> extra_data) {
    extra_data_ = std::move(extra_data);
  }
  ExtraData* GetExtraData() { return extra_data_.get(); }

  void DidStartPrerender();
  void DidStopPrerender();
  void DidSendLoadForPrerender();
  void DidSendDOMContentLoadedForPrerender();

 private:
  // The embedder's prerendering support holds on to pending Prerender objects;
  // those references should not keep the PrerenderClient alive -- if the client
  // becomes otherwise unreachable it should be GCed (at which point it will
  // abandon this Prerender object.)
  WeakMember<PrerenderClient> client_;

  const KURL url_;
  const unsigned rel_types_;
  const Referrer referrer_;
  const SecurityOrigin* const security_origin_;

  scoped_refptr<ExtraData> extra_data_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_PRERENDER_H_
