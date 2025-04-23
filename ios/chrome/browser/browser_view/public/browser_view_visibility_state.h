// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSER_VIEW_PUBLIC_BROWSER_VIEW_VISIBILITY_STATE_H_
#define IOS_CHROME_BROWSER_BROWSER_VIEW_PUBLIC_BROWSER_VIEW_VISIBILITY_STATE_H_

/// The reason why the browser view is invisible.
enum class BrowserViewVisibilityState {
  /// The browser view is not in the view hierarchy.
  kNotInViewHierarchy,
  /// The browser view will appear.
  kAppearing,
  /// The browser view is visible.
  kVisible,
  /// The browser view is the view hierarchy, but covered by omnibox popup.
  kCoveredByOmniboxPopup,
  /// The browser view is the view hierarchy, but covered by voice search.
  kCoveredByVoiceSearch,
  /// The browser view hidden by a modal.
  kCoveredByModal,
};

#endif  // IOS_CHROME_BROWSER_BROWSER_VIEW_PUBLIC_BROWSER_VIEW_VISIBILITY_STATE_H_
