// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/preconnector.h"

#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"

namespace content {

Preconnector::Preconnector(RenderFrameHost& render_frame_host) {
  preconnect_delegate_ =
      GetContentClient()->browser()->CreateAnchorElementPreconnectDelegate(
          render_frame_host);
}

Preconnector::~Preconnector() = default;

void Preconnector::MaybePreconnect(const GURL& url) {
  if (preconnect_delegate_)
    preconnect_delegate_->MaybePreconnect(url);
}

}  // namespace content
