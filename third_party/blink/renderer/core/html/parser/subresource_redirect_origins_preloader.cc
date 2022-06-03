// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/parser/subresource_redirect_origins_preloader.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

// static
const char SubresourceRedirectOriginsPreloader::kSupplementName[] =
    "SubresourceRedirectOriginsPreloader";

SubresourceRedirectOriginsPreloader* SubresourceRedirectOriginsPreloader::From(
    Document& document) {
  if (!base::FeatureList::IsEnabled(blink::features::kSubresourceRedirect) ||
      !GetNetworkStateNotifier().SaveDataEnabled()) {
    return nullptr;
  }

  SubresourceRedirectOriginsPreloader* preloader =
      Supplement<Document>::From<SubresourceRedirectOriginsPreloader>(document);
  if (!preloader) {
    preloader =
        MakeGarbageCollected<SubresourceRedirectOriginsPreloader>(document);
    ProvideTo(document, preloader);
  }
  return preloader;
}

SubresourceRedirectOriginsPreloader::SubresourceRedirectOriginsPreloader(
    Document& document)
    : Supplement<Document>(document) {}

void SubresourceRedirectOriginsPreloader::Trace(Visitor* visitor) const {
  Supplement<Document>::Trace(visitor);
}

void SubresourceRedirectOriginsPreloader::AddImagePreloadRequest(
    const KURL& base_url,
    const String& resource_url) {
  KURL url = base_url.IsEmpty() ? GetSupplementable()->CompleteURL(resource_url)
                                : GetSupplementable()->CompleteURLWithOverride(
                                      resource_url, base_url);

  if (!url.IsValid())
    return;

  if (!url.ProtocolIsInHTTPFamily())
    return;

  auto origin = SecurityOrigin::Create(url);
  if (origin->IsOpaque())
    return;

  origins_.insert(origin);
}

void SubresourceRedirectOriginsPreloader::PreloadOriginsNow() {
  GetSupplementable()
      ->GetFrame()
      ->Client()
      ->PreloadSubresourceOptimizationsForOrigins(origins_);
  origins_.clear();
}

}  // namespace blink
