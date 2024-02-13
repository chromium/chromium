// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/web_scoped_page_pauser.h"

#include "base/feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/page/scoped_browsing_context_group_pauser.h"
#include "third_party/blink/renderer/core/page/scoped_page_pauser.h"

namespace blink {

WebScopedPagePauser::WebScopedPagePauser(WebLocalFrameImpl& frame) {
  Page* page = WebFrame::ToCoreFrame(frame)->GetPage();
  CHECK(page);
  if (base::FeatureList::IsEnabled(
          features::kPausePagesPerBrowsingContextGroup)) {
    browsing_context_group_pauser_ =
        std::make_unique<ScopedBrowsingContextGroupPauser>(*page);
  } else {
    // Clear the page if we aren't showing the hud display.
    if (!base::FeatureList::IsEnabled(
            features::kShowHudDisplayForPausedPages)) {
      page = nullptr;
    }
    page_pauser_ = std::make_unique<ScopedPagePauser>(page);
  }
}

WebScopedPagePauser::~WebScopedPagePauser() = default;

}  // namespace blink
