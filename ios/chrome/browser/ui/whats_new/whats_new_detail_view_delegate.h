// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_DETAIL_VIEW_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_DETAIL_VIEW_DELEGATE_H_

#import "ios/chrome/browser/ui/whats_new/whats_new_instructions_coordinator.h"

@class WhatsNewDetailViewController;
@class WhatsNewScreenshotViewController;

// Delegate protocol to handle communication from the detail view to the
// parent coordinator.
@protocol WhatsNewDetailViewDelegate

// Invoked to request the delegate to dismiss the detail view.
- (void)dismissWhatsNewDetailView:
    (WhatsNewDetailViewController*)whatsNewDetailViewController;

// Invoked to request the delegate to dismiss the instruction half screen view.
- (void)dismissWhatsNewInstructionsCoordinator:
    (WhatsNewInstructionsCoordinator*)coordinator;

// Invoked to request the delegate to dismiss the screenshow view.
- (void)dismissWhatsNewScreenshotViewController:
    (WhatsNewScreenshotViewController*)whatsNewScreenshotViewController;

@end

#endif  // IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_DETAIL_VIEW_DELEGATE_H_
