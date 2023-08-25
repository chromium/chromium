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

namespace {

// Used to defer all page activity in cases where the embedder wishes to run
// a nested event loop. Using a stack enables nesting of message loop
// invocations.
Vector<std::unique_ptr<ScopedPagePauser>>& PagePauserStack() {
  DEFINE_STATIC_LOCAL(Vector<std::unique_ptr<ScopedPagePauser>>, pauser_stack,
                      ());
  return pauser_stack;
}

Vector<std::unique_ptr<ScopedBrowsingContextGroupPauser>>&
BrowsingContextGroupPauserStack() {
  DEFINE_STATIC_LOCAL(Vector<std::unique_ptr<ScopedBrowsingContextGroupPauser>>,
                      pauser_stack, ());
  return pauser_stack;
}

}  // namespace

// static
std::unique_ptr<WebScopedPagePauser> WebScopedPagePauser::Create(
    WebLocalFrameImpl& frame) {
  return std::unique_ptr<WebScopedPagePauser>(new WebScopedPagePauser(frame));
}

WebScopedPagePauser::WebScopedPagePauser(WebLocalFrameImpl& frame) {
  if (base::FeatureList::IsEnabled(
          features::kPausePagesPerBrowsingContextGroup)) {
    Page* page = WebFrame::ToCoreFrame(frame)->GetPage();
    CHECK(page);
    BrowsingContextGroupPauserStack().push_back(
        std::make_unique<ScopedBrowsingContextGroupPauser>(*page));
  } else {
    PagePauserStack().push_back(std::make_unique<ScopedPagePauser>());
  }
}

WebScopedPagePauser::~WebScopedPagePauser() {
  if (base::FeatureList::IsEnabled(
          features::kPausePagesPerBrowsingContextGroup)) {
    CHECK_NE(BrowsingContextGroupPauserStack().size(), 0u);
    BrowsingContextGroupPauserStack().pop_back();
  } else {
    CHECK_NE(PagePauserStack().size(), 0u);
    PagePauserStack().pop_back();
  }
}

}  // namespace blink
