// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_TAB_PICKER_CONSUMER_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_TAB_PICKER_CONSUMER_H_

#import <Foundation/Foundation.h>

// The composebox tab picker consumer.
@protocol ComposeboxTabPickerConsumer <NSObject>

/// Updates the UI with selected tabs count.
- (void)setSelectedTabsCount:(NSUInteger)tabsCount;

/// Enables or disables the done button. Default value is NO.
- (void)setDoneButtonEnabled:(BOOL)enabled;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_TAB_PICKER_CONSUMER_H_
