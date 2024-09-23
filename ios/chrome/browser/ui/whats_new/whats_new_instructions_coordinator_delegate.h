// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_INSTRUCTIONS_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_INSTRUCTIONS_COORDINATOR_DELEGATE_H_

#import "ios/chrome/browser/ui/whats_new/whats_new_instructions_coordinator.h"

// Delegate protocol to handle communication from the instructions view to the
// parent coordinator.
@protocol WhatsNewInstructionsViewDelegate

// Invoked to request the delegate to dismiss the half screen instructions view.
- (void)dismissWhatsNewInstructionsCoordinator:
    (WhatsNewInstructionsCoordinator*)coordinator;

@end

#endif  // IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_INSTRUCTIONS_COORDINATOR_DELEGATE_H_
