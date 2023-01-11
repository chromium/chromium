// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/manifest_icon_downloader.h"

#include <stddef.h>

#include <limits>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/numerics/safe_conversions.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "skia/ext/image_operations.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"

namespace content {

bool ManifestIconDownloader::Download(
    WebContents* web_contents,
    const GURL& icon_url,
    int ideal_icon_size_in_px,
    int minimum_icon_size_in_px,
    int maximum_icon_size_in_px,
    IconFetchCallback callback,
    bool square_only,
    const GlobalRenderFrameHostId& initiator_frame_routing_id) {
  DCHECK(minimum_icon_size_in_px <= ideal_icon_size_in_px);
  if (!web_contents || !icon_url.is_valid())
    return false;

  const gfx::Size preferred_size(ideal_icon_size_in_px, ideal_icon_size_in_px);
  web_contents->DownloadImageInFrame(
      initiator_frame_routing_id, icon_url,
      false,                    // is_favicon
      preferred_size,           // preferred_size
      maximum_icon_size_in_px,  // max_bitmap_size - 0 means no maximum size.
      false,                    // bypass_cache
      base::BindOnce(&ManifestIconDownloader::OnIconFetched,
                     ideal_icon_size_in_px, minimum_icon_size_in_px,
                     square_only, web_contents->GetWeakPtr(),
                     std::move(callback)));
  return true;
}

void ManifestIconDownloader::OnIconFetched(
    int ideal_icon_size_in_px,
    int minimum_icon_size_in_px,
    bool square_only,
    base::WeakPtr<WebContents> web_contents,
    IconFetchCallback callback,
    int id,
    int http_status_code,
    const GURL& url,
    const std::vector<SkBitmap>& bitmaps,
    const std::vector<gfx::Size>& sizes) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (bitmaps.empty()) {
    if (web_contents) {
      web_contents->GetPrimaryMainFrame()->AddMessageToConsole(
          blink::mojom::ConsoleMessageLevel::kError,
          "Error while trying to use the following icon from the Manifest: " +
              url.spec() + " (Download error or resource isn't a valid image)");
    }

    std::move(callback).Run(SkBitmap());
    return;
  }

  const int closest_index = FindClosestBitmapIndex(
      ideal_icon_size_in_px, minimum_icon_size_in_px, square_only, bitmaps);

  if (closest_index == -1) {
    if (web_contents) {
      web_contents->GetPrimaryMainFrame()->AddMessageToConsole(
          blink::mojom::ConsoleMessageLevel::kError,
          "Error while trying to use the following icon from the Manifest: " +
              url.spec() +
              " (Resource size is not correct - typo in the Manifest?)");
    }

    std::move(callback).Run(SkBitmap());
    return;
  }

  const SkBitmap& chosen = bitmaps[closest_index];
  float ratio = 1.0;
  // Preserve width/height ratio if non-square icons allowed.
  if (!square_only && !chosen.empty()) {
    ratio = base::checked_cast<float>(chosen.width()) /
            base::checked_cast<float>(chosen.height());
  }
  float ideal_icon_width_in_px = ratio * ideal_icon_size_in_px;
  // Only scale if we need to scale down. For scaling up we will let the system
  // handle that when it is required to display it. This saves space in the
  // webapp storage system as well.
  if (chosen.height() > ideal_icon_size_in_px ||
      chosen.width() > ideal_icon_width_in_px) {
    GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&ManifestIconDownloader::ScaleIcon,
                                  ideal_icon_width_in_px, ideal_icon_size_in_px,
                                  chosen, std::move(callback)));
    return;
  }

  std::move(callback).Run(chosen);
}

void ManifestIconDownloader::ScaleIcon(int ideal_icon_width_in_px,
                                       int ideal_icon_height_in_px,
                                       const SkBitmap& bitmap,
                                       IconFetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  const SkBitmap& scaled = skia::ImageOperations::Resize(
      bitmap, skia::ImageOperations::RESIZE_BEST, ideal_icon_width_in_px,
      ideal_icon_height_in_px);

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), scaled));
}

int ManifestIconDownloader::FindClosestBitmapIndex(
    int ideal_icon_size_in_px,
    int minimum_icon_size_in_px,
    bool square_only,
    const std::vector<SkBitmap>& bitmaps) {
  int best_index = -1;
  int best_delta = std::numeric_limits<int>::min();
  const int max_negative_delta =
      minimum_icon_size_in_px - ideal_icon_size_in_px;

  for (size_t i = 0; i < bitmaps.size(); ++i) {
    if (bitmaps[i].empty())
      continue;

    // Check for valid width/height ratio.
    float width = base::checked_cast<float>(bitmaps[i].width());
    float height = base::checked_cast<float>(bitmaps[i].height());
    float ratio = width / height;
    if (ratio < 1 || ratio > kMaxWidthToHeightRatio)
      continue;
    if (square_only && ratio != 1)
      continue;

    int delta = bitmaps[i].height() - ideal_icon_size_in_px;
    if (delta == 0)
      return i;

    if (best_delta > 0 && delta < 0)
      continue;

    if ((best_delta > 0 && delta < best_delta) ||
        (best_delta < 0 && delta > best_delta && delta >= max_negative_delta)) {
      best_index = i;
      best_delta = delta;
    }
  }

  if (best_index != -1)
    return best_index;

  // There was no square/landscape icon of a correct size found. Try to find the
  // most square-like icon which has both dimensions greater than the minimum
  // size.
  float best_ratio_difference = std::numeric_limits<float>::infinity();
  for (size_t i = 0; i < bitmaps.size(); ++i) {
    if (bitmaps[i].height() < minimum_icon_size_in_px ||
        bitmaps[i].width() < minimum_icon_size_in_px) {
      continue;
    }

    float height = static_cast<float>(bitmaps[i].height());
    float width = static_cast<float>(bitmaps[i].width());
    float ratio = width / height;
    if (!square_only && ratio > kMaxWidthToHeightRatio)
      continue;
    float ratio_difference = fabs(ratio - 1);
    if (ratio_difference < best_ratio_difference) {
      best_index = i;
      best_ratio_difference = ratio_difference;
    }
  }

  return best_index;
}

}  // namespace content
