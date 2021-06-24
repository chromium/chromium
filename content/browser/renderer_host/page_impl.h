// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PAGE_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_PAGE_IMPL_H_

#include <memory>
#include <vector>

#include "content/browser/fenced_frame/fenced_frame_url_mapping.h"
#include "content/public/browser/page.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"
#include "url/gurl.h"

namespace content {

class RenderFrameHostImpl;

// This implements the Page interface that is exposed to embedders of content,
// and adds things only visible to content.

// Please refer to content/public/browser/page.h for more details.
class CONTENT_EXPORT PageImpl : public Page {
 public:
  explicit PageImpl(RenderFrameHostImpl& rfh);

  ~PageImpl() override;

  // Page implementation.
  const absl::optional<GURL>& GetManifestUrl() const override;
  void GetManifest(GetManifestCallback callback) override;
  bool IsPrimary() override;

  void UpdateManifestUrl(const GURL& manifest_url);

  RenderFrameHostImpl* main_document() const { return &main_document_; }

  bool is_on_load_completed() const { return is_on_load_completed_; }
  void set_is_on_load_completed(bool completed) {
    is_on_load_completed_ = completed;
  }

  const std::vector<blink::mojom::FaviconURLPtr>& favicon_urls() const {
    return favicon_urls_;
  }
  void set_favicon_urls(std::vector<blink::mojom::FaviconURLPtr> favicon_urls) {
    favicon_urls_ = std::move(favicon_urls);
  }

  FencedFrameURLMapping& fenced_frame_urls_map() {
    return fenced_frame_urls_map_;
  }

 private:
  // True if we've received a notification that the onload() handler has
  // run for main frame document.
  bool is_on_load_completed_ = false;

  // Web application manifest URL for this page.
  // See https://w3c.github.io/manifest/#web-application-manifest.
  //
  // This is non-nullopt when the page gets an update of the manifest URL. It
  // can be the empty URL when the manifest url is removed and a non-empty
  // URL when it has a valid URL for the manifest. If this is non-nullopt,
  // WebContentsObserver::DidUpdateWebManifestURL() will be called
  // (either immediately on document load, or on activation in the case
  // of a prerendered page).
  //
  // nullopt indicates that the page did not get an update of the
  // manifest URL, and DidUpdateWebManifestURL() will not be called.
  absl::optional<GURL> manifest_url_;

  // Candidate favicon URLs. Each page may have a collection and will be
  // displayed when active (i.e., upon activation for prerendering).
  std::vector<blink::mojom::FaviconURLPtr> favicon_urls_;

  // Fenced frames:
  // Any fenced frames created within this page will access this map.
  FencedFrameURLMapping fenced_frame_urls_map_;

  // This class is owned by the main RenderFrameHostImpl and it's safe to keep a
  // reference to it.
  RenderFrameHostImpl& main_document_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PAGE_IMPL_H_
