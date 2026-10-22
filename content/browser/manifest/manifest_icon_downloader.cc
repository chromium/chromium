// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/manifest_icon_downloader.h"

#include <stddef.h>

#include <algorithm>
#include <limits>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "skia/ext/image_operations.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "ui/gfx/geometry/size.h"

namespace content {

namespace {

const SkBitmap& MeasureMetrics(const GURL& icon_url,
                               const SkBitmap& bitmap,
                               ManifestIconDownloader::Result icon_result) {
  base::UmaHistogramEnumeration("WebApp.ManifestIconDownloader.Result",
                                icon_result);
  if (icon_url.SchemeIs(content::kChromeUIScheme)) {
    base::UmaHistogramEnumeration(
        "WebApp.ManifestIconDownloader.ChromeUrl.Result", icon_result);
  }
  return bitmap;
}

}  // namespace

bool ManifestIconDownloader::Download(
    WebContents* web_contents,
    const GURL& icon_url,
    int ideal_icon_size_in_px,
    int minimum_icon_size_in_px,
    int maximum_icon_size_in_px,
    IconFetchCallback callback,
    bool square_only,
    const GlobalRenderFrameHostId& initiator_frame_routing_id,
    bool suppress_warnings) {
  // TODO(crbug.com/557584562): CHECK-exclusion: Convert to a CHECK once we are
  // confident it won't be triggered.
  DCHECK(minimum_icon_size_in_px <= ideal_icon_size_in_px);
  if (!web_contents || !icon_url.is_valid())
    return false;

  const gfx::Size preferred_size(ideal_icon_size_in_px, ideal_icon_size_in_px);

  IconFetchCallbackWithResult final_callback_with_metrics =
      base::BindOnce(&MeasureMetrics, icon_url).Then(std::move(callback));

  web_contents->DownloadImageInFrame(
      initiator_frame_routing_id, icon_url,
      false,                    // is_favicon
      preferred_size,           // preferred_size
      maximum_icon_size_in_px,  // max_bitmap_size - 0 means no maximum size.
      false,                    // bypass_cache
      base::BindOnce(
          &ManifestIconDownloader::OnIconFetched, ideal_icon_size_in_px,
          minimum_icon_size_in_px, square_only, web_contents->GetWeakPtr(),
          std::move(final_callback_with_metrics), suppress_warnings));
  return true;
}

bool ManifestIconDownloader::IsUsableIconSize(const gfx::Size& size,
                                              int minimum_icon_size_in_px,
                                              bool pad_to_square) {
  const int longest_side = std::max(size.width(), size.height());
  const int shortest_side = std::min(size.width(), size.height());
  if (shortest_side <= 0) {
    return false;
  }
  // A padded icon ends up as `longest_side` x `longest_side`.
  if ((pad_to_square ? longest_side : shortest_side) <
      minimum_icon_size_in_px) {
    return false;
  }
  return static_cast<float>(longest_side) / shortest_side <=
         kMaxWidthToHeightRatio;
}

SkBitmap ManifestIconDownloader::PadIconToSquare(const SkBitmap& icon,
                                                 int square_size) {
  CHECK(!icon.drawsNothing());
  CHECK_GT(square_size, 0);
  const int longest_side = std::max(icon.width(), icon.height());
  SkBitmap resized = icon;
  if (longest_side > square_size) {
    const float scale = static_cast<float>(square_size) / longest_side;
    resized = skia::ImageOperations::Resize(
        icon, skia::ImageOperations::RESIZE_BEST,
        std::clamp(base::ClampRound(icon.width() * scale), 1, square_size),
        std::clamp(base::ClampRound(icon.height() * scale), 1, square_size));
  }
  if (resized.width() == square_size && resized.height() == square_size) {
    return resized;
  }

  SkBitmap square;
  square.allocN32Pixels(square_size, square_size);
  square.eraseColor(SK_ColorTRANSPARENT);
  SkCanvas canvas(square);
  canvas.drawImage(SkImages::RasterFromBitmap(resized),
                   (square_size - resized.width()) / 2.0f,
                   (square_size - resized.height()) / 2.0f);
  return square;
}

void ManifestIconDownloader::OnIconFetched(
    int ideal_icon_size_in_px,
    int minimum_icon_size_in_px,
    bool square_only,
    base::WeakPtr<WebContents> web_contents,
    IconFetchCallbackWithResult callback,
    bool suppress_warnings,
    int id,
    int http_status_code,
    const GURL& url,
    const std::vector<SkBitmap>& bitmaps,
    const std::vector<gfx::Size>& sizes) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);

  if (bitmaps.empty()) {
    if (web_contents && !suppress_warnings) {
      web_contents->GetPrimaryMainFrame()->AddMessageToConsole(
          blink::mojom::ConsoleMessageLevel::kWarning,
          "Error while trying to use the following icon from the Manifest: " +
              url.spec() + " (Download error or resource isn't a valid image)");
    }

    std::move(callback).Run(SkBitmap(),
                            ManifestIconDownloader::Result::kNoImageFound);
    return;
  }

  const int closest_index = FindClosestBitmapIndex(
      ideal_icon_size_in_px, minimum_icon_size_in_px, square_only, bitmaps);

  if (closest_index == -1) {
    if (web_contents && !suppress_warnings) {
      web_contents->GetPrimaryMainFrame()->AddMessageToConsole(
          blink::mojom::ConsoleMessageLevel::kWarning,
          "Error while trying to use the following icon from the Manifest: " +
              url.spec() +
              " (Resource size is not correct - typo in the Manifest?)");
    }

    std::move(callback).Run(SkBitmap(),
                            ManifestIconDownloader::Result::kIncorrectSize);
    return;
  }

  const SkBitmap& chosen = bitmaps[closest_index];

  if (square_only) {
    // Scaling down happens while padding, and icons are never scaled up.
    const int square_size = std::min(std::max(chosen.width(), chosen.height()),
                                     ideal_icon_size_in_px);
    if (chosen.width() != square_size || chosen.height() != square_size) {
      GetIOThreadTaskRunner({})->PostTask(
          FROM_HERE, base::BindOnce(&ManifestIconDownloader::PadIcon,
                                    square_size, chosen, std::move(callback)));
      return;
    }
    std::move(callback).Run(chosen, ManifestIconDownloader::Result::kSuccess);
    return;
  }

  // Preserve the width/height ratio of non-square icons.
  const float ratio = chosen.empty()
                          ? 1.0f
                          : base::checked_cast<float>(chosen.width()) /
                                base::checked_cast<float>(chosen.height());
  const int ideal_icon_width_in_px =
      base::ClampRound(ratio * ideal_icon_size_in_px);
  const int ideal_icon_height_in_px = ideal_icon_size_in_px;
  // Only scale if we need to scale down. For scaling up we will let the system
  // handle that when it is required to display it. This saves space in the
  // webapp storage system as well.
  if (chosen.height() > ideal_icon_height_in_px ||
      chosen.width() > ideal_icon_width_in_px) {
    GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&ManifestIconDownloader::ScaleIcon,
                       ideal_icon_width_in_px, ideal_icon_height_in_px, chosen,
                       std::move(callback)));
    return;
  }

  std::move(callback).Run(chosen, ManifestIconDownloader::Result::kSuccess);
}

void ManifestIconDownloader::ScaleIcon(int ideal_icon_width_in_px,
                                       int ideal_icon_height_in_px,
                                       const SkBitmap& bitmap,
                                       IconFetchCallbackWithResult callback) {
  CHECK_CURRENTLY_ON(BrowserThread::IO, base::NotFatalUntil::M159);

  const SkBitmap& scaled = skia::ImageOperations::Resize(
      bitmap, skia::ImageOperations::RESIZE_BEST, ideal_icon_width_in_px,
      ideal_icon_height_in_px);

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), scaled,
                                ManifestIconDownloader::Result::kSuccess));
}

void ManifestIconDownloader::PadIcon(int square_size_in_px,
                                     const SkBitmap& bitmap,
                                     IconFetchCallbackWithResult callback) {
  CHECK_CURRENTLY_ON(BrowserThread::IO, base::NotFatalUntil::M159);

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                PadIconToSquare(bitmap, square_size_in_px),
                                ManifestIconDownloader::Result::kSuccess));
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
    if (bitmaps[i].drawsNothing()) {
      continue;
    }

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

  // Try the most square-like bitmap that meets the minimum size.
  float best_ratio_difference = std::numeric_limits<float>::infinity();
  for (size_t i = 0; i < bitmaps.size(); ++i) {
    if (bitmaps[i].drawsNothing()) {
      continue;
    }
    const gfx::Size size(bitmaps[i].width(), bitmaps[i].height());
    if (!IsUsableIconSize(size, minimum_icon_size_in_px,
                          /*pad_to_square=*/square_only)) {
      continue;
    }

    const float ratio_difference =
        static_cast<float>(std::max(size.width(), size.height())) /
            std::min(size.width(), size.height()) -
        1.0f;
    if (ratio_difference < best_ratio_difference) {
      best_index = i;
      best_ratio_difference = ratio_difference;
    }
  }

  return best_index;
}

}  // namespace content
