// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/download_item_utils.h"

#include "base/memory/raw_ptr.h"
#include "components/download/public/common/download_item.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {

namespace {

// This is a UserData::Data that will be attached to a DownloadItem as a
// side-channel for passing WebContents and BrowserContext.
class DownloadItemData : public base::SupportsUserData::Data,
                         public WebContentsObserver {
 public:
  DownloadItemData(BrowserContext* browser_context,
                   WebContents* web_contents,
                   GlobalRenderFrameHostId id);
  ~DownloadItemData() override = default;

  static void Attach(download::DownloadItem* download_item,
                     BrowserContext* browser_context,
                     WebContents* web_contents,
                     GlobalRenderFrameHostId id);

  static DownloadItemData* Get(const download::DownloadItem* download_item);
  static void Detach(download::DownloadItem* download_item);

  BrowserContext* browser_context() const { return browser_context_; }
  GlobalRenderFrameHostId id() const { return id_; }

 private:
  // WebContentsObserver methods.
  void PrimaryPageChanged(Page& page) override;
  void WebContentsDestroyed() override;

  static const char kKey[];
  raw_ptr<BrowserContext, DanglingUntriaged> browser_context_;
  GlobalRenderFrameHostId id_;
};

// static
const char DownloadItemData::kKey[] = "DownloadItemUtils DownloadItemData";

DownloadItemData::DownloadItemData(BrowserContext* browser_context,
                                   WebContents* web_contents,
                                   GlobalRenderFrameHostId id)
    : WebContentsObserver(web_contents),
      browser_context_(browser_context),
      id_(id) {}

// static
void DownloadItemData::Attach(download::DownloadItem* download_item,
                              BrowserContext* browser_context,
                              WebContents* web_contents,
                              GlobalRenderFrameHostId id) {
  auto data =
      std::make_unique<DownloadItemData>(browser_context, web_contents, id);
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

void DownloadItemData::PrimaryPageChanged(Page& page) {
  // To prevent reuse of a render in a different primary page,
  // DownloadItemUtils::GetWebContents() will return null after the primary page
  // changed.
  id_ = GlobalRenderFrameHostId();
}

void DownloadItemData::WebContentsDestroyed() {
  Observe(nullptr);
  id_ = GlobalRenderFrameHostId();
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
  if (!data || !data->id()) {
    return nullptr;
  }
  return data->web_contents();
}

// static
RenderFrameHost* DownloadItemUtils::GetRenderFrameHost(
    const download::DownloadItem* download_item) {
  DownloadItemData* data = DownloadItemData::Get(download_item);
  if (!data)
    return nullptr;
  return RenderFrameHost::FromID(data->id());
}

// static
WebContents* DownloadItemUtils::GetOriginalWebContents(
    const download::DownloadItem* download_item) {
  DownloadItemData* data = DownloadItemData::Get(download_item);
  if (!data) {
    return nullptr;
  }
  return data->web_contents();
}

// static
void DownloadItemUtils::AttachInfo(download::DownloadItem* download_item,
                                   BrowserContext* browser_context,
                                   WebContents* web_contents,
                                   GlobalRenderFrameHostId id) {
  DownloadItemData::Attach(download_item, browser_context, web_contents, id);
}

// static
void DownloadItemUtils::AttachInfoForTesting(
    download::DownloadItem* download_item,
    BrowserContext* browser_context,
    WebContents* web_contents) {
  DownloadItemUtils::AttachInfo(
      download_item, browser_context, web_contents,
      web_contents ? web_contents->GetPrimaryMainFrame()->GetGlobalId()
                   : GlobalRenderFrameHostId());
}

}  // namespace content
