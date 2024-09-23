// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/payments/payment_instrument_icon_fetcher.h"

#include <limits>
#include <string_view>
#include <utility>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"
#include "components/payments/content/icon/icon_size.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/manifest_icon_downloader.h"
#include "third_party/blink/public/common/manifest/manifest_icon_selector.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"

namespace content {
namespace {

void DownloadBestMatchingIcon(
    const GURL& scope,
    std::unique_ptr<std::vector<GlobalRenderFrameHostId>> frame_routing_ids,
    const std::vector<blink::Manifest::ImageResource>& icons,
    PaymentInstrumentIconFetcher::PaymentInstrumentIconFetcherCallback
        callback);

WebContents* GetWebContentsFromFrameRoutingIds(
    const GURL& scope,
    const std::vector<GlobalRenderFrameHostId>& frame_routing_ids);

void OnIconFetched(
    const GURL& scope,
    std::unique_ptr<std::vector<GlobalRenderFrameHostId>> frame_routing_ids,
    const std::vector<blink::Manifest::ImageResource>& icons,
    PaymentInstrumentIconFetcher::PaymentInstrumentIconFetcherCallback callback,
    const SkBitmap& bitmap) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (bitmap.drawsNothing()) {
    if (icons.empty()) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), std::string()));
    } else {
      // If could not download or decode the chosen image(e.g. not supported,
      // invalid), try it again with remaining icons.
      DownloadBestMatchingIcon(scope, std::move(frame_routing_ids), icons,
                               std::move(callback));
    }
    return;
  }

  std::vector<unsigned char> bitmap_data;
  bool success = gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, false, &bitmap_data);
  DCHECK(success);

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback),
                     base::Base64Encode(std::string_view(
                         reinterpret_cast<const char*>(&bitmap_data[0]),
                         bitmap_data.size()))));
}

void DownloadBestMatchingIcon(
    const GURL& scope,
    std::unique_ptr<std::vector<GlobalRenderFrameHostId>> frame_routing_ids,
    const std::vector<blink::Manifest::ImageResource>& icons,
    PaymentInstrumentIconFetcher::PaymentInstrumentIconFetcherCallback
        callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebContents* web_contents =
      GetWebContentsFromFrameRoutingIds(scope, *frame_routing_ids);
  if (web_contents == nullptr) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::string()));
    return;
  }

  gfx::NativeView native_view = web_contents->GetNativeView();
  GURL icon_url = blink::ManifestIconSelector::FindBestMatchingIcon(
      icons, payments::IconSizeCalculator::IdealIconHeight(native_view),
      payments::IconSizeCalculator::MinimumIconHeight(),
      ManifestIconDownloader::kMaxWidthToHeightRatio,
      blink::mojom::ManifestImageResource_Purpose::ANY);
  if (!icon_url.is_valid()) {
    // If the icon url is invalid, it's better to give the information to
    // developers in advance unlike when fetching or decoding fails. We already
    // checked whether they are valid in renderer side. So, if the icon url is
    // invalid, it's something wrong.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::string()));
    return;
  }

  std::vector<blink::Manifest::ImageResource> copy_icons;
  for (const auto& icon : icons) {
    if (icon.src != icon_url) {
      copy_icons.emplace_back(icon);
    }
  }

  bool can_download_icon = ManifestIconDownloader::Download(
      web_contents, icon_url,
      payments::IconSizeCalculator::IdealIconHeight(native_view),
      payments::IconSizeCalculator::MinimumIconHeight(),
      /* maximum_icon_size_in_px= */ std::numeric_limits<int>::max(),
      base::BindOnce(&OnIconFetched, scope, std::move(frame_routing_ids),
                     copy_icons, std::move(callback)),
      false /* square_only */);
  DCHECK(can_download_icon);
}

WebContents* GetWebContentsFromFrameRoutingIds(
    const GURL& scope,
    const std::vector<GlobalRenderFrameHostId>& frame_routing_ids) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (const auto& ids : frame_routing_ids) {
    RenderFrameHostImpl* render_frame_host =
        RenderFrameHostImpl::FromID(ids.child_id, ids.frame_routing_id);
    if (!render_frame_host)
      continue;

    WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(
        WebContents::FromRenderFrameHost(render_frame_host));
    if (!web_contents || web_contents->IsHidden() ||
        scope.DeprecatedGetOriginAsURL().spec().compare(
            web_contents->GetLastCommittedURL()
                .DeprecatedGetOriginAsURL()
                .spec()) != 0) {
      continue;
    }
    return web_contents;
  }
  return nullptr;
}

}  // namespace

// static
void PaymentInstrumentIconFetcher::Start(
    const GURL& scope,
    std::unique_ptr<std::vector<GlobalRenderFrameHostId>> provider_hosts,
    const std::vector<blink::Manifest::ImageResource>& icons,
    PaymentInstrumentIconFetcherCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DownloadBestMatchingIcon(scope, std::move(provider_hosts), icons,
                           std::move(callback));
}

}  // namespace content
