// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_PICKER_UI_TAB_PICKER_MUTATOR_H_
#define IOS_CHROME_BROWSER_TAB_PICKER_UI_TAB_PICKER_MUTATOR_H_

// The tab's picker mutator.
@protocol TabPickerMutator

// Extract and attaches the selected tabs.
- (void)attachSelectedTabs;

@end

#endif  // IOS_CHROME_BROWSER_TAB_PICKER_UI_TAB_PICKER_MUTATOR_H_
