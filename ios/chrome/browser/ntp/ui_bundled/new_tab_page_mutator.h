// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_MUTATOR_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_MUTATOR_H_

// Mutator for the NTP's view controller to update the mediator.
@protocol NewTabPageMutator

// The current scroll position to save in the NTP state.
@property(nonatomic, assign) CGFloat scrollPositionToSave;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_MUTATOR_H_
