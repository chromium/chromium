// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/page_visibility_observer.h"

#include "third_party/blink/renderer/core/page/page.h"

namespace blink {

PageVisibilityObserver::PageVisibilityObserver(Page* page) {
  SetPage(page);
}

void PageVisibilityObserver::ObserverListWillBeCleared() {
  page_ = nullptr;
}

void PageVisibilityObserver::SetPage(Page* page) {
  if (page == page_)
    return;

  if (page_)
    page_->PageVisibilityObserverList().RemoveObserver(this);

  page_ = page;

  if (page_)
    page_->PageVisibilityObserverList().AddObserver(this);
}

void PageVisibilityObserver::Trace(Visitor* visitor) const {
  visitor->Trace(page_);
}

}  // namespace blink
