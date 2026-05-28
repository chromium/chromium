// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MANUAL_FILL_UI_CARD_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MANUAL_FILL_UI_CARD_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/autofill/manual_fill/ui/card_consumer.h"
#import "ios/chrome/browser/autofill/manual_fill/ui/fallback_view_controller.h"

// This class presents a list of credit cards in a table view.
@interface CardViewController : FallbackViewController <ManualFillCardConsumer>

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MANUAL_FILL_UI_CARD_VIEW_CONTROLLER_H_
