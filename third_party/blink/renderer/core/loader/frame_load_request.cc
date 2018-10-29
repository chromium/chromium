// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/frame_load_request.h"

#include "third_party/blink/public/common/blob/blob_utils.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/core/fileapi/public_url_manager.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

FrameLoadRequest::FrameLoadRequest(Document* origin_document)
    : FrameLoadRequest(origin_document, ResourceRequest()) {}

FrameLoadRequest::FrameLoadRequest(Document* origin_document,
                                   const ResourceRequest& resource_request)
    : FrameLoadRequest(origin_document, resource_request, AtomicString()) {}

FrameLoadRequest::FrameLoadRequest(Document* origin_document,
                                   const ResourceRequest& resource_request,
                                   const AtomicString& frame_name)
    : FrameLoadRequest(origin_document,
                       resource_request,
                       frame_name,
                       kCheckContentSecurityPolicy) {}

FrameLoadRequest::FrameLoadRequest(
    Document* origin_document,
    const ResourceRequest& resource_request,
    const AtomicString& frame_name,
    ContentSecurityPolicyDisposition
        should_check_main_world_content_security_policy)
    : origin_document_(origin_document),
      resource_request_(resource_request),
      frame_name_(frame_name),
      client_redirect_(ClientRedirectPolicy::kNotClientRedirect),
      should_send_referrer_(kMaybeSendReferrer),
      should_set_opener_(kMaybeSetOpener),
      should_check_main_world_content_security_policy_(
          should_check_main_world_content_security_policy) {
  // These flags are passed to a service worker which controls the page.
  resource_request_.SetFetchRequestMode(
      network::mojom::FetchRequestMode::kNavigate);
  resource_request_.SetFetchCredentialsMode(
      network::mojom::FetchCredentialsMode::kInclude);
  resource_request_.SetFetchRedirectMode(
      network::mojom::FetchRedirectMode::kManual);

  if (origin_document) {
    DCHECK(!resource_request_.RequestorOrigin());
    resource_request_.SetRequestorOrigin(
        SecurityOrigin::Create(origin_document->Url()));

    if (resource_request.Url().ProtocolIs("blob") &&
        BlobUtils::MojoBlobURLsEnabled()) {
      blob_url_token_ = base::MakeRefCounted<
          base::RefCountedData<mojom::blink::BlobURLTokenPtr>>();
      origin_document->GetPublicURLManager().Resolve(
          resource_request.Url(), MakeRequest(&blob_url_token_->data));
    }
  }
}

}  // namespace blink
