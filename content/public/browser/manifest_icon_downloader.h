// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_MANIFEST_ICON_DOWNLOADER_H_
#define CONTENT_PUBLIC_BROWSER_MANIFEST_ICON_DOWNLOADER_H_

#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "content/common/content_export.h"

class GURL;
class SkBitmap;

namespace gfx {
class Size;
}  // namespace gfx

namespace content {

class WebContents;

// Helper class which downloads the icon located at a specified URL. If the
// icon file contains multiple icons then it attempts to pick the one closest in
// size bigger than or equal to ideal_icon_size_in_px, taking into account the
// density of the device. If a bigger icon is chosen then, the icon is scaled
// down to be equal to ideal_icon_size_in_px. Smaller icons will be chosen down
// to the value specified by |minimum_icon_size_in_px|.
class CONTENT_EXPORT ManifestIconDownloader final {
 public:
  using IconFetchCallback = base::OnceCallback<void(const SkBitmap&)>;

  ManifestIconDownloader() = delete;
  ~ManifestIconDownloader() = delete;

  // Returns whether the download has started.
  // It will return false if the current context or information do not allow to
  // download the image.
  static bool Download(content::WebContents* web_contents,
                       const GURL& icon_url,
                       int ideal_icon_size_in_px,
                       int minimum_icon_size_in_px,
                       IconFetchCallback callback,
                       bool square_only = true);

  // This threshold has been chosen arbitrarily and is open to any necessary
  // changes in the future.
  static const int kMaxWidthToHeightRatio = 5;

 private:
  class DevToolsConsoleHelper;

  // Callback run after the manifest icon downloaded successfully or the
  // download failed.
  static void OnIconFetched(int ideal_icon_size_in_px,
                            int minimum_icon_size_in_px,
                            bool square_only,
                            DevToolsConsoleHelper* console_helper,
                            IconFetchCallback callback,
                            int id,
                            int http_status_code,
                            const GURL& url,
                            const std::vector<SkBitmap>& bitmaps,
                            const std::vector<gfx::Size>& sizes);

  static void ScaleIcon(int ideal_icon_width_in_px,
                        int ideal_icon_height_in_px,
                        const SkBitmap& bitmap,
                        IconFetchCallback callback);

  static int FindClosestBitmapIndex(int ideal_icon_size_in_px,
                                    int minimum_icon_size_in_px,
                                    bool square_only,
                                    const std::vector<SkBitmap>& bitmaps);

  friend class ManifestIconDownloaderTest;

  DISALLOW_COPY_AND_ASSIGN(ManifestIconDownloader);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_MANIFEST_ICON_DOWNLOADER_H_
