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
  if (base::FeatureList::IsEnabled(
          features::kPausePagesPerBrowsingContextGroup)) {
    Page* page = WebFrame::ToCoreFrame(frame)->GetPage();
    CHECK(page);
    browsing_context_group_pauser_ =
        std::make_unique<ScopedBrowsingContextGroupPauser>(*page);
  } else {
    page_pauser_ = std::make_unique<ScopedPagePauser>();
  }
}

WebScopedPagePauser::~WebScopedPagePauser() = default;

}  // namespace blink
