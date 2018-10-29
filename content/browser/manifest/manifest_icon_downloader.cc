// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/manifest_icon_downloader.h"

#include <stddef.h>

#include <limits>

#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/console_message_level.h"
#include "skia/ext/image_operations.h"

namespace content {

// DevToolsConsoleHelper is a class that holds a WebContents in order to be able
// to send a message to the WebContents' main frame. It is used so
// ManifestIconDownloader and the callers do not have to worry about
// |web_contents| lifetime. If the |web_contents| is invalidated before the
// message can be sent, the message will simply be ignored.
class ManifestIconDownloader::DevToolsConsoleHelper
    : public WebContentsObserver {
 public:
  explicit DevToolsConsoleHelper(WebContents* web_contents);
  ~DevToolsConsoleHelper() override = default;

  void AddMessage(ConsoleMessageLevel level, const std::string& message);
};

ManifestIconDownloader::DevToolsConsoleHelper::DevToolsConsoleHelper(
    WebContents* web_contents)
    : WebContentsObserver(web_contents) {}

void ManifestIconDownloader::DevToolsConsoleHelper::AddMessage(
    ConsoleMessageLevel level,
    const std::string& message) {
  if (!web_contents())
    return;
  web_contents()->GetMainFrame()->AddMessageToConsole(level, message);
}

bool ManifestIconDownloader::Download(
    WebContents* web_contents,
    const GURL& icon_url,
    int ideal_icon_size_in_px,
    int minimum_icon_size_in_px,
    const ManifestIconDownloader::IconFetchCallback& callback) {
  DCHECK(minimum_icon_size_in_px <= ideal_icon_size_in_px);
  if (!web_contents || !icon_url.is_valid())
    return false;

  web_contents->DownloadImage(
      icon_url,
      false,  // is_favicon
      0,      // max_bitmap_size - 0 means no maximum size.
      false,  // bypass_cache
      base::BindOnce(&ManifestIconDownloader::OnIconFetched,
                     ideal_icon_size_in_px, minimum_icon_size_in_px,
                     base::Owned(new DevToolsConsoleHelper(web_contents)),
                     callback));
  return true;
}

void ManifestIconDownloader::OnIconFetched(
    int ideal_icon_size_in_px,
    int minimum_icon_size_in_px,
    DevToolsConsoleHelper* console_helper,
    const ManifestIconDownloader::IconFetchCallback& callback,
    int id,
    int http_status_code,
    const GURL& url,
    const std::vector<SkBitmap>& bitmaps,
    const std::vector<gfx::Size>& sizes) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (bitmaps.empty()) {
    console_helper->AddMessage(
        CONSOLE_MESSAGE_LEVEL_ERROR,
        "Error while trying to use the following icon from the Manifest: " +
            url.spec() + " (Download error or resource isn't a valid image)");

    callback.Run(SkBitmap());
    return;
  }

  const int closest_index = FindClosestBitmapIndex(
      ideal_icon_size_in_px, minimum_icon_size_in_px, bitmaps);

  if (closest_index == -1) {
    console_helper->AddMessage(
        CONSOLE_MESSAGE_LEVEL_ERROR,
        "Error while trying to use the following icon from the Manifest: " +
            url.spec() +
            " (Resource size is not correct - typo in the Manifest?)");

    callback.Run(SkBitmap());
    return;
  }

  const SkBitmap& chosen = bitmaps[closest_index];

  // Only scale if we need to scale down. For scaling up we will let the system
  // handle that when it is required to display it. This saves space in the
  // webapp storage system as well.
  if (chosen.height() > ideal_icon_size_in_px ||
      chosen.width() > ideal_icon_size_in_px) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&ManifestIconDownloader::ScaleIcon,
                       ideal_icon_size_in_px, chosen, callback));
    return;
  }

  callback.Run(chosen);
}

void ManifestIconDownloader::ScaleIcon(
    int ideal_icon_size_in_px,
    const SkBitmap& bitmap,
    const ManifestIconDownloader::IconFetchCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  const SkBitmap& scaled = skia::ImageOperations::Resize(
      bitmap, skia::ImageOperations::RESIZE_BEST, ideal_icon_size_in_px,
      ideal_icon_size_in_px);

  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                           base::BindOnce(callback, scaled));
}

int ManifestIconDownloader::FindClosestBitmapIndex(
    int ideal_icon_size_in_px,
    int minimum_icon_size_in_px,
    const std::vector<SkBitmap>& bitmaps) {
  int best_index = -1;
  int best_delta = std::numeric_limits<int>::min();
  const int max_negative_delta =
      minimum_icon_size_in_px - ideal_icon_size_in_px;

  for (size_t i = 0; i < bitmaps.size(); ++i) {
    if (bitmaps[i].height() != bitmaps[i].width())
      continue;

    int delta = bitmaps[i].width() - ideal_icon_size_in_px;
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

  // There was no square icon of a correct size found. Try to find the most
  // square-like icon which has both dimensions greater than the minimum size.
  float best_ratio_difference = std::numeric_limits<float>::infinity();
  for (size_t i = 0; i < bitmaps.size(); ++i) {
    if (bitmaps[i].height() < minimum_icon_size_in_px ||
        bitmaps[i].width() < minimum_icon_size_in_px) {
      continue;
    }

    float height = static_cast<float>(bitmaps[i].height());
    float width = static_cast<float>(bitmaps[i].width());
    float ratio = height / width;
    float ratio_difference = fabs(ratio - 1);
    if (ratio_difference < best_ratio_difference) {
      best_index = i;
      best_ratio_difference = ratio_difference;
    }
  }

  return best_index;
}

}  // namespace content
