// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_CONTAINER_TOOLBAR_COLLAPSING_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_CONTAINER_TOOLBAR_COLLAPSING_H_

#import <UIKit/UIKit.h>

// Protocol for UI that displays collapsible toolbars.
@protocol ToolbarCollapsing<NSObject>

// The height of the toolber when fully expanded.
@property(nonatomic, readonly) CGFloat expandedToolbarHeight;

// The height of the toolbar when fully collapsed.
@property(nonatomic, readonly) CGFloat collapsedToolbarHeight;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_CONTAINER_TOOLBAR_COLLAPSING_H_
