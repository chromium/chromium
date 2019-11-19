// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/web_scoped_page_pauser.h"

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

}  // namespace

// static
std::unique_ptr<WebScopedPagePauser> WebScopedPagePauser::Create() {
  return std::unique_ptr<WebScopedPagePauser>(new WebScopedPagePauser);
}

WebScopedPagePauser::WebScopedPagePauser() {
  PagePauserStack().push_back(std::make_unique<ScopedPagePauser>());
}

WebScopedPagePauser::~WebScopedPagePauser() {
  DCHECK_NE(PagePauserStack().size(), 0u);
  PagePauserStack().pop_back();
}

}  // namespace blink
