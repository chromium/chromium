// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_MODAL_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_MODAL_DELEGATE_H_

#import <Foundation/Foundation.h>

#include "base/ios/block_types.h"

// Delegate to handle InfobarModal actions.
@protocol InfobarModalDelegate

// Asks the delegate to dismiss the InfobarModal.
- (void)dismissInfobarModal:(id)sender
                   animated:(BOOL)animated
                 completion:(ProceduralBlock)completion;

// Called when the InfobarModal was Accepted. Meaning it will perform the main
// action.
- (void)modalInfobarButtonWasAccepted:(id)sender;

// Called when the InfobarModal was dismissed.
- (void)modalInfobarWasDismissed:(id)sender;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_MODAL_DELEGATE_H_
