// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_FRE_VIEW_CONTROLLER_PROTOCOL_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_FRE_VIEW_CONTROLLER_PROTOCOL_H_

#import <UIKit/UIKit.h>

// Protocol that Gemini view controllers conform to in order to share the same
// logic.
@protocol GeminiFREViewControllerProtocol <NSObject>

// Content height of the FRE Gemini UI.
- (CGFloat)contentHeight;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_FRE_VIEW_CONTROLLER_PROTOCOL_H_
