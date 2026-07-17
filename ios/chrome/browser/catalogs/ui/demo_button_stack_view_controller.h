// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CATALOGS_UI_DEMO_BUTTON_STACK_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_CATALOGS_UI_DEMO_BUTTON_STACK_VIEW_CONTROLLER_H_

#import "ios/chrome/common/ui/button_stack/button_stack_view_controller.h"

// A view controller used to demonstrate an implementation example of
// `ButtonStackViewController`.
@interface DemoButtonStackViewController : ButtonStackViewController

// Designated initializer, `scrollableContent` determines whether the
// contentView needs to be large enough to be scrollable.
- (instancetype)initWithConfiguration:(ButtonStackConfiguration*)configuration
                    scrollableContent:(BOOL)isContentScrollable
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithConfiguration:(ButtonStackConfiguration*)configuration
    NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_CATALOGS_UI_DEMO_BUTTON_STACK_VIEW_CONTROLLER_H_
