// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PAGE_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_PAGE_IMPL_H_

#include <memory>
#include <vector>

#include "content/browser/fenced_frame/fenced_frame_url_mapping.h"
#include "content/common/content_export.h"
#include "content/public/browser/page.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

namespace content {

class PageDelegate;
class RenderFrameHostImpl;

// This implements the Page interface that is exposed to embedders of content,
// and adds things only visible to content.

// Please refer to content/public/browser/page.h for more details.
class CONTENT_EXPORT PageImpl : public Page {
 public:
  explicit PageImpl(RenderFrameHostImpl& rfh, PageDelegate& delegate);

  ~PageImpl() override;

  // Page implementation.
  const absl::optional<GURL>& GetManifestUrl() const override;
  void GetManifest(GetManifestCallback callback) override;
  bool IsPrimary() override;
  void WriteIntoTrace(perfetto::TracedValue context) override;

  void UpdateManifestUrl(const GURL& manifest_url);

  RenderFrameHostImpl& GetMainDocument() const;

  bool is_on_load_completed_in_main_document() const {
    return is_on_load_completed_in_main_document_;
  }
  void set_is_on_load_completed_in_main_document(bool completed) {
    is_on_load_completed_in_main_document_ = completed;
  }

  void OnFirstVisuallyNonEmptyPaint();
  bool did_first_visually_non_empty_paint() const {
    return did_first_visually_non_empty_paint_;
  }

  const std::vector<blink::mojom::FaviconURLPtr>& favicon_urls() const {
    return favicon_urls_;
  }
  void set_favicon_urls(std::vector<blink::mojom::FaviconURLPtr> favicon_urls) {
    favicon_urls_ = std::move(favicon_urls);
  }

  void OnThemeColorChanged(const absl::optional<SkColor>& theme_color);

  void DidChangeBackgroundColor(SkColor background_color, bool color_adjust);

  absl::optional<SkColor> theme_color() const {
    return main_document_theme_color_;
  }

  absl::optional<SkColor> background_color() const {
    return main_document_background_color_;
  }

  void SetContentsMimeType(std::string mime_type);
  const std::string& contents_mime_type() { return contents_mime_type_; }

  FencedFrameURLMapping& fenced_frame_urls_map() {
    return fenced_frame_urls_map_;
  }

  void set_last_main_document_source_id(ukm::SourceId id) {
    last_main_document_source_id_ = id;
  }
  ukm::SourceId last_main_document_source_id() const {
    return last_main_document_source_id_;
  }

 private:
  // This method is needed to ensure that PageImpl can both implement a Page's
  // method and define a new GetMainDocument(). Please refer to page.h for more
  // details.
  RenderFrameHost& GetMainDocumentHelper() override;

  // True if we've received a notification that the onload() handler has
  // run for the main document.
  bool is_on_load_completed_in_main_document_ = false;

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

  // Whether the first visually non-empty paint has occurred.
  bool did_first_visually_non_empty_paint_ = false;

  // The theme color for the underlying document as specified
  // by theme-color meta tag.
  absl::optional<SkColor> main_document_theme_color_;

  // The background color for the underlying document as computed by CSS.
  absl::optional<SkColor> main_document_background_color_;

  // Contents MIME type for the main document. It can be used to check whether
  // we can do something for special contents.
  std::string contents_mime_type_;

  // Fenced frames:
  // Any fenced frames created within this page will access this map.
  FencedFrameURLMapping fenced_frame_urls_map_;

  // This class is owned by the main RenderFrameHostImpl and it's safe to keep a
  // reference to it.
  RenderFrameHostImpl& main_document_;

  // SourceId of the navigation in this page's main frame. Note that a same
  // document navigation is the only case where this source id can change, since
  // all other navigations create a new PageImpl instance.
  ukm::SourceId last_main_document_source_id_ = ukm::kInvalidSourceId;

  // This page is owned by the RenderFrameHostImpl, which in turn does not
  // outlive the delegate (the contents).
  PageDelegate& delegate_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PAGE_IMPL_H_
