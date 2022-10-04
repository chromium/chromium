// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_PRIMARY_ACTION_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_PRIMARY_ACTION_HANDLER_H_

#include "ios/chrome/browser/ui/whats_new/data_source/whats_new_data_source.h"

// Delegate protocol for `WhatsNewDetailViewController`
// to communicate with `WhatsNewMediator`.
@protocol WhatsNewPrimaryActionHandler

// Invoked when the action button is tapped.
// Invoked when a user interacts with the primary button for a specific
// `WhatsNewEntryId`.
- (void)didTapActionButton:(WhatsNewItem*)item;

@end

#endif  // IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_PRIMARY_ACTION_HANDLER_H_
