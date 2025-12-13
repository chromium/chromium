// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/testing/fake_resource_load_info_notifier.h"

#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom.h"

namespace blink {

FakeResourceLoadInfoNotifier::FakeResourceLoadInfoNotifier() = default;
FakeResourceLoadInfoNotifier::~FakeResourceLoadInfoNotifier() = default;

#if BUILDFLAG(IS_ANDROID)
void FakeResourceLoadInfoNotifier::NotifyUpdateUserGestureCarryoverInfo() {}
#endif  // BUILDFLAG(IS_ANDROID)

void FakeResourceLoadInfoNotifier::NotifyResourceRedirectReceived(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr redirect_response) {}

void FakeResourceLoadInfoNotifier::NotifyResourceResponseReceived(
    int64_t request_id,
    const url::SchemeHostPort& final_url,
    network::mojom::URLResponseHeadPtr head,
    network::mojom::RequestDestination request_destination,
    bool is_ad_resource) {}

void FakeResourceLoadInfoNotifier::NotifyResourceTransferSizeUpdated(
    int64_t request_id,
    int32_t transfer_size_diff) {}

void FakeResourceLoadInfoNotifier::NotifyResourceLoadCompleted(
    blink::mojom::ResourceLoadInfoPtr resource_load_info,
    const ::network::URLLoaderCompletionStatus& status) {
  resource_load_info_ = std::move(resource_load_info);
}
void FakeResourceLoadInfoNotifier::NotifyResourceLoadCanceled(
    int64_t request_id) {}

void FakeResourceLoadInfoNotifier::Clone(
    mojo::PendingReceiver<blink::mojom::ResourceLoadInfoNotifier>
        pending_resource_load_info_notifier) {}

std::string FakeResourceLoadInfoNotifier::GetMimeType() {
  return resource_load_info_->mime_type;
}

}  // namespace blink
