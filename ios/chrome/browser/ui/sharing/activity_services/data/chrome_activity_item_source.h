// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SHARING_ACTIVITY_SERVICES_DATA_CHROME_ACTIVITY_ITEM_SOURCE_H_
#define IOS_CHROME_BROWSER_UI_SHARING_ACTIVITY_SERVICES_DATA_CHROME_ACTIVITY_ITEM_SOURCE_H_

#import <UIKit/UIKit.h>

// Base protocol for activity item sources in Chrome.
@protocol ChromeActivityItemSource <UIActivityItemSource>

// Property for the set of activity types that we want to be excluded from the
// activity view when this item source is part of the activity items.
@property(nonatomic, readonly) NSSet* excludedActivityTypes;

@end

#endif  // IOS_CHROME_BROWSER_UI_SHARING_ACTIVITY_SERVICES_DATA_CHROME_ACTIVITY_ITEM_SOURCE_H_
