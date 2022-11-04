// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/preloading_decider.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"

namespace content {
DOCUMENT_USER_DATA_KEY_IMPL(PreloadingDecider);

PreloadingDecider::PreloadingDecider(content::RenderFrameHost* rfh)
    : DocumentUserData<PreloadingDecider>(rfh) {
  preconnect_delegate_ =
      GetContentClient()->browser()->CreateAnchorElementPreconnectDelegate(
          render_frame_host());
}

PreloadingDecider::~PreloadingDecider() = default;

void PreloadingDecider::OnPointerDown(const GURL& url) {
  if (preconnect_delegate_)
    preconnect_delegate_->MaybePreconnect(url);
}

}  // namespace content
