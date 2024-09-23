// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/background_fetch/background_fetch_icon_loader.h"

#include "base/time/time.h"
#include "third_party/blink/public/common/manifest/manifest_icon_selector.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_image_resource.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/background_fetch/background_fetch_bridge.h"
#include "third_party/blink/renderer/modules/manifest/image_resource_type_converters.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_utils.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/string_impl.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"

namespace blink {

namespace {

constexpr base::TimeDelta kIconFetchTimeout = base::Seconds(30);
constexpr int kMinimumIconSizeInPx = 0;

}  // namespace

BackgroundFetchIconLoader::BackgroundFetchIconLoader()
    : threaded_icon_loader_(MakeGarbageCollected<ThreadedIconLoader>()) {}

void BackgroundFetchIconLoader::Start(
    BackgroundFetchBridge* bridge,
    ExecutionContext* execution_context,
    HeapVector<Member<ManifestImageResource>> icons,
    IconCallback icon_callback) {
  DCHECK_GE(icons.size(), 1u);
  DCHECK(bridge);

  icons_ = std::move(icons);
  bridge->GetIconDisplaySize(WTF::BindOnce(
      &BackgroundFetchIconLoader::DidGetIconDisplaySizeIfSoLoadIcon,
      WrapWeakPersistent(this), WrapWeakPersistent(execution_context),
      std::move(icon_callback)));
}

void BackgroundFetchIconLoader::DidGetIconDisplaySizeIfSoLoadIcon(
    ExecutionContext* execution_context,
    IconCallback icon_callback,
    const gfx::Size& icon_display_size_pixels) {
  // If |icon_display_size_pixels| is empty then no image will be displayed by
  // the UI powering Background Fetch. Bail out immediately.
  if (icon_display_size_pixels.IsEmpty()) {
    std::move(icon_callback)
        .Run(SkBitmap(), -1 /* ideal_to_chosen_icon_size_times_hundred */);
    return;
  }

  KURL best_icon_url = PickBestIconForDisplay(
      execution_context, icon_display_size_pixels.height());
  if (best_icon_url.IsEmpty()) {
    // None of the icons provided was suitable.
    std::move(icon_callback)
        .Run(SkBitmap(), -1 /* ideal_to_chosen_icon_size_times_hundred */);
    return;
  }

  icon_callback_ = std::move(icon_callback);

  ResourceRequest resource_request(best_icon_url);
  resource_request.SetRequestContext(mojom::blink::RequestContextType::IMAGE);
  resource_request.SetRequestDestination(
      network::mojom::RequestDestination::kImage);
  resource_request.SetPriority(ResourceLoadPriority::kMedium);
  resource_request.SetKeepalive(true);
  resource_request.SetMode(network::mojom::RequestMode::kNoCors);
  resource_request.SetTargetAddressSpace(
      network::mojom::IPAddressSpace::kUnknown);
  resource_request.SetCredentialsMode(
      network::mojom::CredentialsMode::kInclude);
  resource_request.SetSkipServiceWorker(true);
  resource_request.SetTimeoutInterval(kIconFetchTimeout);

  FetchUtils::LogFetchKeepAliveRequestMetric(
      resource_request.GetRequestContext(),
      FetchUtils::FetchKeepAliveRequestState::kTotal);
  threaded_icon_loader_->Start(
      execution_context, resource_request, icon_display_size_pixels,
      WTF::BindOnce(&BackgroundFetchIconLoader::DidGetIcon,
                    WrapWeakPersistent(this)));
}

KURL BackgroundFetchIconLoader::PickBestIconForDisplay(
    ExecutionContext* execution_context,
    int ideal_size_pixels) {
  WebVector<Manifest::ImageResource> icons;
  for (auto& icon : icons_) {
    // Update the src of |icon| to include the base URL in case relative paths
    // were used.
    icon->setSrc(execution_context->CompleteURL(icon->src()));
    Manifest::ImageResource candidate_icon =
        blink::ConvertManifestImageResource(icon);
    // Provide default values for 'purpose' and 'sizes' if they are missing.
    if (candidate_icon.sizes.empty())
      candidate_icon.sizes.emplace_back(gfx::Size(0, 0));
    if (candidate_icon.purpose.empty()) {
      candidate_icon.purpose.emplace_back(
          mojom::ManifestImageResource_Purpose::ANY);
    }
    icons.emplace_back(candidate_icon);
  }

  return KURL(ManifestIconSelector::FindBestMatchingSquareIcon(
      icons.ReleaseVector(), ideal_size_pixels, kMinimumIconSizeInPx,
      mojom::ManifestImageResource_Purpose::ANY));
}

void BackgroundFetchIconLoader::Stop() {
  threaded_icon_loader_->Stop();
}

void BackgroundFetchIconLoader::DidGetIcon(SkBitmap icon, double resize_scale) {
  if (icon.isNull()) {
    std::move(icon_callback_).Run(icon, -1);
    return;
  }

  int ideal_to_chosen_icon_size_times_hundred = std::round(resize_scale) * 100;
  std::move(icon_callback_).Run(icon, ideal_to_chosen_icon_size_times_hundred);
}

void BackgroundFetchIconLoader::Trace(Visitor* visitor) const {
  visitor->Trace(icons_);
  visitor->Trace(threaded_icon_loader_);
}

}  // namespace blink
