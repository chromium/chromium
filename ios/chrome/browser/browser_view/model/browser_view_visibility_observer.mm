// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browser_view/model/browser_view_visibility_observer.h"

#import "base/check.h"

BrowserViewVisibilityObserver::BrowserViewVisibilityObserver() = default;

BrowserViewVisibilityObserver::~BrowserViewVisibilityObserver() {
  CHECK(!IsInObserverList())
      << "BrowserViewVisibilityObserver needs to be removed from the observer "
         "list before their destruction.";
}
