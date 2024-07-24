// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_DETAIL_VIEW_ACTION_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_DETAIL_VIEW_ACTION_HANDLER_H_

#include "ios/chrome/browser/ui/whats_new/data_source/whats_new_item.h"

class GURL;

// Delegate protocol to handle communication from the detail view to the
// mediator.
@protocol WhatsNewDetailViewActionHandler

// Invoked when a user interacts with the primary button for a specific
// `WhatsNewEntryId`.
- (void)didTapActionButton:(WhatsNewType)type
             primaryAction:(WhatsNewPrimaryAction)primaryAction
        baseViewController:(UIViewController*)baseViewController;

// Invoked when a user interacts with the learn more button for a specific
// `WhatsNewEntryId`, which will open a new tab with the learn more url loaded.
- (void)didTapLearnMoreButton:(const GURL&)learnMoreURL type:(WhatsNewType)type;

// Invoked when a user interacts with the instructions button for a specific
// `WhatsNewEntryId`.
- (void)didTapInstructions:(WhatsNewType)type;

@end

#endif  // IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_DETAIL_VIEW_ACTION_HANDLER_H_
