// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_MODAL_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_MODAL_DELEGATE_H_

#import <Foundation/Foundation.h>

#include "base/ios/block_types.h"

// Delegate to handle InfobarModal actions.
// TODO(crbug.com/40668000): Update this protocol to be tied with an infobar
// modal view controller rather than plain id types.
@protocol InfobarModalDelegate

// Asks the delegate to dismiss the InfobarModal.
- (void)dismissInfobarModal:(id)infobarModal;

// Called when the InfobarModal was Accepted. Meaning it will perform the main
// action.
- (void)modalInfobarButtonWasAccepted:(id)infobarModal;

// Called when the InfobarModal was dismissed.
- (void)modalInfobarWasDismissed:(id)infobarModal;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_MODAL_DELEGATE_H_
