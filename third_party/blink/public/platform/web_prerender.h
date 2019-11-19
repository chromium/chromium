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
 */

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_PRERENDER_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_PRERENDER_H_

#include "services/network/public/mojom/referrer_policy.mojom-shared.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_private_ptr.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "url/origin.h"

namespace blink {

class Prerender;

class WebPrerender {
 public:
  class ExtraData {
   public:
    virtual ~ExtraData() = default;
  };

  ~WebPrerender() { Reset(); }
  WebPrerender() = default;
  WebPrerender(const WebPrerender& other) { Assign(other); }
  WebPrerender& operator=(const WebPrerender& other) {
    Assign(other);
    return *this;
  }

#if INSIDE_BLINK
  BLINK_PLATFORM_EXPORT explicit WebPrerender(Prerender*);

  BLINK_PLATFORM_EXPORT const Prerender* ToPrerender() const;
#endif

  BLINK_PLATFORM_EXPORT void Reset();
  BLINK_PLATFORM_EXPORT void Assign(const WebPrerender&);
  BLINK_PLATFORM_EXPORT bool IsNull() const;

  BLINK_PLATFORM_EXPORT WebURL Url() const;
  BLINK_PLATFORM_EXPORT WebString GetReferrer() const;
  BLINK_PLATFORM_EXPORT url::Origin SecurityOrigin() const;
  BLINK_PLATFORM_EXPORT unsigned RelTypes() const;
  BLINK_PLATFORM_EXPORT network::mojom::ReferrerPolicy GetReferrerPolicy()
      const;

  BLINK_PLATFORM_EXPORT void SetExtraData(ExtraData*);
  BLINK_PLATFORM_EXPORT const ExtraData* GetExtraData() const;

  BLINK_PLATFORM_EXPORT void DidStartPrerender();
  BLINK_PLATFORM_EXPORT void DidStopPrerender();
  BLINK_PLATFORM_EXPORT void DidSendLoadForPrerender();
  BLINK_PLATFORM_EXPORT void DidSendDOMContentLoadedForPrerender();

 private:
  WebPrivatePtr<Prerender> private_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_PRERENDER_H_
