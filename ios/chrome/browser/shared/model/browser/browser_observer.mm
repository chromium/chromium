// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/browser/browser_observer.h"

// It's a programming error for a BrowserObserver to destruct while still
// in an observer list.
BrowserObserver::~BrowserObserver() {
  CHECK(!IsInObserverList());
}
