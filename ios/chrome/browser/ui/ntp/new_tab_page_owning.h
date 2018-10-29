// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_OWNING_H_
#define IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_OWNING_H_

#include <UIKit/UIKit.h>

// TODO(crbug.com/826369) Helper protocol to bridge the similarities between
// NewTabPageController and NewTabpageCoordinator.  This can be removed when
// NewTabPageController is removed.
@protocol NewTabPageOwning

// Exposes content inset of contentSuggestions collectionView to ensure all of
// content is visible under the bottom toolbar.
@property(nonatomic) UIEdgeInsets contentInset;

// Animates the NTP fakebox to the focused position and focuses the real
// omnibox.
- (void)focusFakebox;

// Called when a snapshot of the content will be taken.
- (void)willUpdateSnapshot;

// The scroll offset of this native view.
- (CGPoint)scrollOffset;

// The current NTP view.
- (UIView*)view;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_OWNING_H_
