// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_BWG_FRE_VIEW_CONTROLLER_PROTOCOL_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_BWG_FRE_VIEW_CONTROLLER_PROTOCOL_H_

#import <UIKit/UIKit.h>

// Protocol that BWG view controllers conform to in order to shared the same
// logic.
@protocol BWGFREViewControllerProtocol <NSObject>

// Content height of the FRE BWG UI.
- (CGFloat)contentHeight;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_BWG_FRE_VIEW_CONTROLLER_PROTOCOL_H_
