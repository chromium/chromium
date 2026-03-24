// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/fullscreen/model/fullscreen_browser_agent.h"

#import "base/check.h"

FullscreenBrowserAgent::FullscreenBrowserAgent(Browser* browser)
    : BrowserUserData(browser) {}

FullscreenBrowserAgent::~FullscreenBrowserAgent() {}

void FullscreenBrowserAgent::AddObserver(
    FullscreenBrowserAgentObserver* observer) {
  observers_.AddObserver(observer);
}

void FullscreenBrowserAgent::RemoveObserver(
    FullscreenBrowserAgentObserver* observer) {
  observers_.RemoveObserver(observer);
}

void FullscreenBrowserAgent::InvalidateInsetRange() {
  min_insets_ = UIEdgeInsetsZero;
  max_insets_ = UIEdgeInsetsZero;

  updating_obscured_insets_ = true;
  for (auto& observer : observers_) {
    observer.WillUpdateObscuredInsetRange(this);
  }
  updating_obscured_insets_ = false;

  for (auto& observer : observers_) {
    observer.DidUpdateObscuredInsetRange(this);
  }
}

void FullscreenBrowserAgent::AddObscuredInsetRange(UIRectEdge edge,
                                                   CGFloat min,
                                                   CGFloat max) {
  CHECK(updating_obscured_insets_);
  CHECK(edge == UIRectEdgeTop || edge == UIRectEdgeBottom);
  if (edge == UIRectEdgeTop) {
    min_insets_.top += min;
    max_insets_.top += max;
  } else if (edge == UIRectEdgeBottom) {
    min_insets_.bottom += min;
    max_insets_.bottom += max;
  }
}
