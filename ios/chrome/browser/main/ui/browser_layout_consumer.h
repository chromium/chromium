// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_MAIN_UI_BROWSER_LAYOUT_CONSUMER_H_
#define IOS_CHROME_BROWSER_MAIN_UI_BROWSER_LAYOUT_CONSUMER_H_

#import <UIKit/UIKit.h>

// Protocol for consumers of layout updates from the browser container.
@protocol BrowserLayoutConsumer <NSObject>

// Layout inset provided by the parent container.
// Used to layout primary toolbars and headers.
@property(nonatomic, assign) CGFloat topToolbarInset;

@end

#endif  // IOS_CHROME_BROWSER_MAIN_UI_BROWSER_LAYOUT_CONSUMER_H_
