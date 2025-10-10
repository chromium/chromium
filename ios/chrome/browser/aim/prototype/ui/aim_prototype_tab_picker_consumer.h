// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AIM_PROTOTYPE_UI_AIM_PROTOTYPE_TAB_PICKER_CONSUMER_H_
#define IOS_CHROME_BROWSER_AIM_PROTOTYPE_UI_AIM_PROTOTYPE_TAB_PICKER_CONSUMER_H_

#import <Foundation/Foundation.h>

// The Aim prototype tab picker consumer.
@protocol AimPrototypeTabPickerConsumer <NSObject>

/// Updates the UI with selected tabs count.
- (void)setSelectedTabsCount:(NSUInteger)tabsCount;

@end

#endif  // IOS_CHROME_BROWSER_AIM_PROTOTYPE_UI_AIM_PROTOTYPE_TAB_PICKER_CONSUMER_H_
