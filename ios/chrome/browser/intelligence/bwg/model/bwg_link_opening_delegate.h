// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_LINK_OPENING_DELEGATE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_LINK_OPENING_DELEGATE_H_

#import <UIKit/UIKit.h>

// Delegate for BWG link opening.
@protocol BWGLinkOpeningDelegate

// Opens a given URL in a new tab.
- (void)openURLInNewTab:(NSString*)URL;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_LINK_OPENING_DELEGATE_H_
