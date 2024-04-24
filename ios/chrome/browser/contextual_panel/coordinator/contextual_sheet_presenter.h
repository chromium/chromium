// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_COORDINATOR_CONTEXTUAL_SHEET_PRESENTER_H_
#define IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_COORDINATOR_CONTEXTUAL_SHEET_PRESENTER_H_

#import <UIKit/UIKit.h>

// Protocol for an object that can position the contextual sheet in the view
// hierarchy.
@protocol ContextualSheetPresenter

// Asks the presenter to insert the given `contextualSheet` into the correct
// place in the view hierarchy.
- (void)insertContextualSheet:(UIView*)contextualSheet;

@end

#endif  // IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_COORDINATOR_CONTEXTUAL_SHEET_PRESENTER_H_
