// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ORCHESTRATOR_UI_BUNDLED_EDIT_VIEW_ANIMATEE_H_
#define IOS_CHROME_BROWSER_ORCHESTRATOR_UI_BUNDLED_EDIT_VIEW_ANIMATEE_H_

#import <UIKit/UIKit.h>

// An object that represents the edit state location bar for focusing animation.
@protocol EditViewAnimatee<NSObject>

// Sets the scale of the leading icon.
- (void)setLeadingIconScale:(CGFloat)scale;

// Toggles the visibility of the clear button.
- (void)setClearButtonFaded:(BOOL)faded;

@end

#endif  // IOS_CHROME_BROWSER_ORCHESTRATOR_UI_BUNDLED_EDIT_VIEW_ANIMATEE_H_
