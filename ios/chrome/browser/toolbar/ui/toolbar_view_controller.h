// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_UI_TOOLBAR_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_TOOLBAR_UI_TOOLBAR_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/toolbar/ui/toolbar_consumer.h"

@class ToolbarButtonFactory;

// View controller for the toolbar.
@interface ToolbarViewController : UIViewController <ToolbarConsumer>

// Factory used to create the buttons.
@property(nonatomic, strong) ToolbarButtonFactory* buttonFactory;

@end

#endif  // IOS_CHROME_BROWSER_TOOLBAR_UI_TOOLBAR_VIEW_CONTROLLER_H_
