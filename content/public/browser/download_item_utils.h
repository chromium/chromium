// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DOWNLOAD_ITEM_UTILS_H_
#define CONTENT_PUBLIC_BROWSER_DOWNLOAD_ITEM_UTILS_H_

#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"

namespace download {
class DownloadItem;
}

namespace content {

class BrowserContext;
class WebContents;
class RenderFrameHost;

// Helper class to attach WebContents and BrowserContext to a DownloadItem.
class CONTENT_EXPORT DownloadItemUtils {
 public:
  // Helper method to get the BrowserContext from a DownloadItem.
  static BrowserContext* GetBrowserContext(
      const download::DownloadItem* downloadItem);

  // Helper method to get the WebContents from a DownloadItem.
  static WebContents* GetWebContents(
      const download::DownloadItem* downloadItem);

  // Helper method to get the RenderFrameHost from a DownloadItem.
  static RenderFrameHost* GetRenderFrameHost(
      const download::DownloadItem* downloadItem);

  // Get the original WebContents that triggers the download. The returned
  // WebContents might be different from that of calling GetWebContents(). If
  // the primary page of the WebContents changes, GetWebContents() will return
  // null, while this method will still return the original WebContents. Only
  // call this method when you really need to get the Tab that triggers the
  // download.
  static WebContents* GetOriginalWebContents(
      const download::DownloadItem* downloadItem);

  // Attach information to a DownloadItem.
  static void AttachInfo(download::DownloadItem* downloadItem,
                         BrowserContext* browser_context,
                         WebContents* web_contents,
                         GlobalRenderFrameHostId id);

  // Attach information to a DownloadItem.
  static void AttachInfoForTesting(download::DownloadItem* downloadItem,
                                   BrowserContext* browser_context,
                                   WebContents* web_contents);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DOWNLOAD_ITEM_UTILS_H_
