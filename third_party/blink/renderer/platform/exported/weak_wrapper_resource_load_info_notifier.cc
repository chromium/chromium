// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/weak_wrapper_resource_load_info_notifier.h"

#include "build/build_config.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom.h"

namespace blink {

WeakWrapperResourceLoadInfoNotifier::WeakWrapperResourceLoadInfoNotifier(
    blink::mojom::ResourceLoadInfoNotifier* resource_load_info_notifier)
    : resource_load_info_notifier_(resource_load_info_notifier) {
  DCHECK(resource_load_info_notifier_);
  DETACH_FROM_THREAD(thread_checker_);
}

void WeakWrapperResourceLoadInfoNotifier::NotifyResourceRedirectReceived(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr redirect_response) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  resource_load_info_notifier_->NotifyResourceRedirectReceived(
      redirect_info, std::move(redirect_response));
}

#if BUILDFLAG(IS_ANDROID)
void WeakWrapperResourceLoadInfoNotifier::
    NotifyUpdateUserGestureCarryoverInfo() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  resource_load_info_notifier_->NotifyUpdateUserGestureCarryoverInfo();
}
#endif

void WeakWrapperResourceLoadInfoNotifier::NotifyResourceResponseReceived(
    int64_t request_id,
    const url::SchemeHostPort& final_response_url,
    network::mojom::URLResponseHeadPtr response_head,
    network::mojom::RequestDestination request_destination,
    bool is_ad_resource) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  resource_load_info_notifier_->NotifyResourceResponseReceived(
      request_id, final_response_url, std::move(response_head),
      request_destination, is_ad_resource);
}

void WeakWrapperResourceLoadInfoNotifier::NotifyResourceTransferSizeUpdated(
    int64_t request_id,
    int32_t transfer_size_diff) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  resource_load_info_notifier_->NotifyResourceTransferSizeUpdated(
      request_id, transfer_size_diff);
}

void WeakWrapperResourceLoadInfoNotifier::NotifyResourceLoadCompleted(
    blink::mojom::ResourceLoadInfoPtr resource_load_info,
    const network::URLLoaderCompletionStatus& status) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  resource_load_info_notifier_->NotifyResourceLoadCompleted(
      std::move(resource_load_info), status);
}

void WeakWrapperResourceLoadInfoNotifier::NotifyResourceLoadCanceled(
    int64_t request_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  resource_load_info_notifier_->NotifyResourceLoadCanceled(request_id);
}

void WeakWrapperResourceLoadInfoNotifier::Clone(
    mojo::PendingReceiver<blink::mojom::ResourceLoadInfoNotifier>
        pending_resource_load_info_notifier) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  resource_load_info_notifier_->Clone(
      std::move(pending_resource_load_info_notifier));
}

base::WeakPtr<WeakWrapperResourceLoadInfoNotifier>
WeakWrapperResourceLoadInfoNotifier::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace blink
