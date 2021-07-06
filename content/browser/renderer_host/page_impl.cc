// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/page_impl.h"

#include "content/browser/manifest/manifest_manager_host.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/page_delegate.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/public/browser/render_view_host.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value.h"

namespace content {

PageImpl::PageImpl(RenderFrameHostImpl& rfh, PageDelegate& delegate)
    : main_document_(rfh), delegate_(delegate) {}

PageImpl::~PageImpl() {
  // As SupportsUserData is a base class of PageImpl, Page members will be
  // destroyed before running ~SupportsUserData, which would delete the
  // associated PageUserData objects. Avoid this by calling ClearAllUserData
  // explicitly here to ensure that the PageUserData destructors can access
  // associated Page object.
  ClearAllUserData();
}

const absl::optional<GURL>& PageImpl::GetManifestUrl() const {
  return manifest_url_;
}

void PageImpl::GetManifest(GetManifestCallback callback) {
  ManifestManagerHost* manifest_manager_host =
      ManifestManagerHost::GetOrCreateForCurrentDocument(&main_document_);
  manifest_manager_host->GetManifest(std::move(callback));
}

bool PageImpl::IsPrimary() {
  // TODO(https://crbug.com/1222722): Query RenderFrameHost::IsInFencedFrame()
  // when it is available.
  return main_document_.lifecycle_state() ==
         RenderFrameHostImpl::LifecycleStateImpl::kActive;
}

void PageImpl::UpdateManifestUrl(const GURL& manifest_url) {
  manifest_url_ = manifest_url;

  // If |main_document_| is not active, the notification is sent on the page
  // activation.
  if (!main_document_.IsActive())
    return;

  main_document_.delegate()->OnManifestUrlChanged(*this);
}

void PageImpl::WriteIntoTrace(perfetto::TracedValue context) {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("main_document", main_document_);
}

void PageImpl::OnFirstVisuallyNonEmptyPaint() {
  did_first_visually_non_empty_paint_ = true;
  delegate_.OnFirstVisuallyNonEmptyPaint(*this);
}

void PageImpl::OnThemeColorChanged(const absl::optional<SkColor>& theme_color) {
  main_document_theme_color_ = theme_color;
  delegate_.OnThemeColorChanged(*this);
}

void PageImpl::DidChangeBackgroundColor(SkColor background_color,
                                        bool color_adjust) {
  main_document_background_color_ = background_color;
  delegate_.OnBackgroundColorChanged(*this);
  if (color_adjust) {
    // <meta name="color-scheme" content="dark"> may pass the dark canvas
    // background before the first paint in order to avoid flashing the white
    // background in between loading documents. If we perform a navigation
    // within the same renderer process, we keep the content background from the
    // previous page while rendering is blocked in the new page, but for cross
    // process navigations we would paint the default background (typically
    // white) while the rendering is blocked.
    main_document_.GetRenderWidgetHost()->GetView()->SetContentBackgroundColor(
        background_color);
  }
}

void PageImpl::SetContentsMimeType(std::string mime_type) {
  contents_mime_type_ = std::move(mime_type);
}

RenderFrameHost& PageImpl::GetMainDocumentHelper() {
  return main_document_;
}

RenderFrameHostImpl& PageImpl::GetMainDocument() const {
  return main_document_;
}

}  // namespace content
