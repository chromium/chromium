/*
 * Copyright (C) 2003, 2006, 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_FRAME_LOAD_REQUEST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_FRAME_LOAD_REQUEST_H_

#include "third_party/blink/public/mojom/blob/blob_url_store.mojom-blink.h"
#include "third_party/blink/public/web/web_triggering_event_info.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/frame_types.h"
#include "third_party/blink/renderer/core/loader/frame_loader_types.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"

namespace blink {

class HTMLFormElement;

struct CORE_EXPORT FrameLoadRequest {
  STACK_ALLOCATED();

 public:
  explicit FrameLoadRequest(Document* origin_document);
  FrameLoadRequest(Document* origin_document, const ResourceRequest&);
  FrameLoadRequest(Document* origin_document,
                   const ResourceRequest&,
                   const AtomicString& frame_name);
  FrameLoadRequest(Document* origin_document,
                   const ResourceRequest&,
                   const AtomicString& frame_name,
                   ContentSecurityPolicyDisposition);

  Document* OriginDocument() const { return origin_document_.Get(); }

  ResourceRequest& GetResourceRequest() { return resource_request_; }
  const ResourceRequest& GetResourceRequest() const {
    return resource_request_;
  }

  const AtomicString& FrameName() const { return frame_name_; }
  void SetFrameName(const AtomicString& frame_name) {
    frame_name_ = frame_name;
  }

  ClientRedirectPolicy ClientRedirect() const { return client_redirect_; }
  void SetClientRedirect(ClientRedirectPolicy client_redirect) {
    client_redirect_ = client_redirect;
  }

  WebTriggeringEventInfo TriggeringEventInfo() const {
    return triggering_event_info_;
  }
  void SetTriggeringEventInfo(WebTriggeringEventInfo info) {
    DCHECK(info != WebTriggeringEventInfo::kUnknown);
    triggering_event_info_ = info;
  }

  HTMLFormElement* Form() const { return form_.Get(); }
  void SetForm(HTMLFormElement* form) { form_ = form; }

  ShouldSendReferrer GetShouldSendReferrer() const {
    return should_send_referrer_;
  }
  void SetShouldSendReferrer(ShouldSendReferrer should_send_referrer) {
    should_send_referrer_ = should_send_referrer;
  }

  ShouldSetOpener GetShouldSetOpener() const { return should_set_opener_; }
  void SetShouldSetOpener(ShouldSetOpener should_set_opener) {
    should_set_opener_ = should_set_opener;
  }

  const AtomicString& HrefTranslate() { return href_translate_; }
  void SetHrefTranslate(const AtomicString& translate) {
    href_translate_ = translate;
  }

  ContentSecurityPolicyDisposition ShouldCheckMainWorldContentSecurityPolicy()
      const {
    return should_check_main_world_content_security_policy_;
  }

  // Sets the BlobURLToken that should be used when fetching the resource. This
  // is needed for blob URLs, because the blob URL might be revoked before the
  // actual fetch happens, which would result in incorrect failures to fetch.
  // The token lets the browser process securely resolves the blob URL even
  // after the url has been revoked.
  // FrameFetchRequest initializes this in its constructor, but in some cases
  // FrameFetchRequest is created asynchronously rather than when a navigation
  // is scheduled, so in those cases NavigationScheduler needs to override the
  // blob FrameLoadRequest might have found.
  void SetBlobURLToken(mojom::blink::BlobURLTokenPtr blob_url_token) {
    DCHECK(blob_url_token);
    blob_url_token_ = base::MakeRefCounted<
        base::RefCountedData<mojom::blink::BlobURLTokenPtr>>(
        std::move(blob_url_token));
  }

  mojom::blink::BlobURLTokenPtr GetBlobURLToken() const {
    if (!blob_url_token_)
      return nullptr;
    mojom::blink::BlobURLTokenPtr result;
    blob_url_token_->data->Clone(MakeRequest(&result));
    return result;
  }

  void SetInputStartTime(base::TimeTicks input_start_time) {
    input_start_time_ = input_start_time;
  }

  base::TimeTicks GetInputStartTime() const { return input_start_time_; }

 private:
  Member<Document> origin_document_;
  ResourceRequest resource_request_;
  AtomicString frame_name_;
  AtomicString href_translate_;
  ClientRedirectPolicy client_redirect_;
  WebTriggeringEventInfo triggering_event_info_ =
      WebTriggeringEventInfo::kNotFromEvent;
  Member<HTMLFormElement> form_;
  ShouldSendReferrer should_send_referrer_;
  ShouldSetOpener should_set_opener_;
  ContentSecurityPolicyDisposition
      should_check_main_world_content_security_policy_;
  scoped_refptr<base::RefCountedData<mojom::blink::BlobURLTokenPtr>>
      blob_url_token_;
  base::TimeTicks input_start_time_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_FRAME_LOAD_REQUEST_H_
