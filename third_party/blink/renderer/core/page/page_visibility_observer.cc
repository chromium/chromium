// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/page_visibility_observer.h"

#include "third_party/blink/renderer/core/page/page.h"

namespace blink {

PageVisibilityObserver::PageVisibilityObserver(Page* page) {
  SetPage(page);
}

void PageVisibilityObserver::ObserverSetWillBeCleared() {
  page_ = nullptr;
}

void PageVisibilityObserver::SetPage(Page* page) {
  if (page == page_)
    return;

  if (page_)
    page_->PageVisibilityObserverSet().erase(this);

  page_ = page;

  if (page_)
    page_->PageVisibilityObserverSet().insert(this);
}

void PageVisibilityObserver::Trace(Visitor* visitor) const {
  visitor->Trace(page_);
}

}  // namespace blink
