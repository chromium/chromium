// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/download_item_utils.h"

#include "components/download/public/common/download_item.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {

namespace {

// This is a UserData::Data that will be attached to a DownloadItem as a
// side-channel for passing WebContents and BrowserContext.
class DownloadItemData : public base::SupportsUserData::Data,
                         public WebContentsObserver {
 public:
  DownloadItemData(BrowserContext* browser_context, WebContents* web_contents);
  ~DownloadItemData() override = default;

  static void Attach(download::DownloadItem* download_item,
                     BrowserContext* browser_context,
                     WebContents* web_contents);
  static DownloadItemData* Get(const download::DownloadItem* download_item);
  static void Detach(download::DownloadItem* download_item);

  BrowserContext* browser_context() const { return browser_context_; }

 private:
   // WebContentsObserver methods.
   void WebContentsDestroyed() override;

  static const char kKey[];
  BrowserContext* browser_context_;
};

// static
const char DownloadItemData::kKey[] = "DownloadItemUtils DownloadItemData";

DownloadItemData::DownloadItemData(BrowserContext* browser_context,
                                   WebContents* web_contents)
    : WebContentsObserver(web_contents),
      browser_context_(browser_context) {}

// static
void DownloadItemData::Attach(download::DownloadItem* download_item,
                              BrowserContext* browser_context,
                              WebContents* web_contents) {
  auto data = std::make_unique<DownloadItemData>(browser_context, web_contents);
  download_item->SetUserData(&kKey, std::move(data));
}

// static
DownloadItemData* DownloadItemData::Get(
    const download::DownloadItem* download_item) {
  return static_cast<DownloadItemData*>(download_item->GetUserData(&kKey));
}

// static
void DownloadItemData::Detach(download::DownloadItem* download_item) {
  download_item->RemoveUserData(&kKey);
}

void DownloadItemData::WebContentsDestroyed() {
  Observe(nullptr);
}

}  // namespace

// static
BrowserContext* DownloadItemUtils::GetBrowserContext(
    const download::DownloadItem* download_item) {
  DownloadItemData* data = DownloadItemData::Get(download_item);
  if (!data)
    return nullptr;
  return data->browser_context();
}

// static
WebContents* DownloadItemUtils::GetWebContents(
    const download::DownloadItem* download_item) {
  DownloadItemData* data = DownloadItemData::Get(download_item);
  if (!data)
    return nullptr;
  return data->web_contents();
}

// static
void DownloadItemUtils::AttachInfo(download::DownloadItem* download_item,
                                   BrowserContext* browser_context,
                                   WebContents* web_contents) {
  DownloadItemData::Attach(download_item, browser_context, web_contents);
}

}  // namespace content
