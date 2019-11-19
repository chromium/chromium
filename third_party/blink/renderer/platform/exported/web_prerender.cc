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

#include "third_party/blink/public/platform/web_prerender.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/prerender.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

namespace {

class PrerenderExtraDataContainer : public Prerender::ExtraData {
 public:
  static scoped_refptr<PrerenderExtraDataContainer> Create(
      WebPrerender::ExtraData* extra_data) {
    return base::AdoptRef(new PrerenderExtraDataContainer(extra_data));
  }

  ~PrerenderExtraDataContainer() override = default;

  WebPrerender::ExtraData* GetExtraData() const { return extra_data_.get(); }

 private:
  explicit PrerenderExtraDataContainer(WebPrerender::ExtraData* extra_data)
      : extra_data_(base::WrapUnique(extra_data)) {}

  std::unique_ptr<WebPrerender::ExtraData> extra_data_;
};

}  // namespace

WebPrerender::WebPrerender(Prerender* prerender) : private_(prerender) {}

const Prerender* WebPrerender::ToPrerender() const {
  return private_.Get();
}

void WebPrerender::Reset() {
  private_.Reset();
}

void WebPrerender::Assign(const WebPrerender& other) {
  private_ = other.private_;
}

bool WebPrerender::IsNull() const {
  return private_.IsNull();
}

WebURL WebPrerender::Url() const {
  return WebURL(private_->Url());
}

unsigned WebPrerender::RelTypes() const {
  return private_->RelTypes();
}

WebString WebPrerender::GetReferrer() const {
  return private_->GetReferrer();
}

url::Origin WebPrerender::SecurityOrigin() const {
  auto* security_origin = private_->GetSecurityOrigin();
  return security_origin ? security_origin->ToUrlOrigin() : url::Origin();
}

network::mojom::ReferrerPolicy WebPrerender::GetReferrerPolicy() const {
  return private_->GetReferrerPolicy();
}

void WebPrerender::SetExtraData(WebPrerender::ExtraData* extra_data) {
  private_->SetExtraData(PrerenderExtraDataContainer::Create(extra_data));
}

const WebPrerender::ExtraData* WebPrerender::GetExtraData() const {
  scoped_refptr<Prerender::ExtraData> webcore_extra_data =
      private_->GetExtraData();
  if (!webcore_extra_data)
    return nullptr;
  return static_cast<PrerenderExtraDataContainer*>(webcore_extra_data.get())
      ->GetExtraData();
}

void WebPrerender::DidStartPrerender() {
  private_->DidStartPrerender();
}

void WebPrerender::DidStopPrerender() {
  private_->DidStopPrerender();
}

void WebPrerender::DidSendLoadForPrerender() {
  private_->DidSendLoadForPrerender();
}

void WebPrerender::DidSendDOMContentLoadedForPrerender() {
  private_->DidSendDOMContentLoadedForPrerender();
}

}  // namespace blink
