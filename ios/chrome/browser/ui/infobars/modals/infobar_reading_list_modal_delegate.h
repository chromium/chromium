// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_READING_LIST_MODAL_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_READING_LIST_MODAL_DELEGATE_H_

#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_delegate.h"

// Delegate to handle Reading List Infobar Modal actions.
@protocol InfobarReadingListModalDelegate <InfobarModalDelegate>

// Indicates the user chose to be never asked to save to Reading List.
- (void)neverAskToAddToReadingList;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_READING_LIST_MODAL_DELEGATE_H_
