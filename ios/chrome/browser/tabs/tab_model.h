// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_TAB_MODEL_H_
#define IOS_CHROME_BROWSER_TABS_TAB_MODEL_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

class Browser;

// A legacy class that encapsulates some lifecycle business logic for tabs in
// a WebStateList. This class is moribund (see crbug.com/783777).
@interface TabModel : NSObject
// Initializes a TabModel from a browser.
- (instancetype)initWithBrowser:(Browser*)browser;

- (instancetype)init NS_UNAVAILABLE;

// Tells the receiver to disconnect from the model object it depends on. This
// should be called before destroying the browser that the receiver was
// initialized with.
// It is safe to call this method multiple times.
// At this point the tab model will no longer ever be active, and will likely be
// deallocated soon.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_TABS_TAB_MODEL_H_
