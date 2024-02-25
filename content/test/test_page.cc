// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_page.h"

#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"

namespace content {

TestPage::TestPage(RenderFrameHostImpl& rfh, PageDelegate& delegate)
    : PageImpl(rfh, delegate) {}

TestPage::~TestPage() = default;

const std::optional<GURL>& TestPage::GetManifestUrl() const {
  if (manifest_url_.has_value())
    return manifest_url_;
  return PageImpl::GetManifestUrl();
}

void TestPage::UpdateManifestUrl(const GURL& manifest_url) {
  manifest_url_ = manifest_url;
  PageImpl::UpdateManifestUrl(manifest_url);
}

}  // namespace content
