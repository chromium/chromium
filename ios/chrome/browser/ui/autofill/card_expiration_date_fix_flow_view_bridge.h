// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_CARD_EXPIRATION_DATE_FIX_FLOW_VIEW_BRIDGE_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_CARD_EXPIRATION_DATE_FIX_FLOW_VIEW_BRIDGE_H_

#import <UIKit/UIKit.h>

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "components/autofill/core/browser/ui/payments/card_expiration_date_fix_flow_controller.h"
#include "components/autofill/core/browser/ui/payments/card_expiration_date_fix_flow_view.h"

namespace autofill {

class CardExpirationDateFixFlowViewBridge
    : public CardExpirationDateFixFlowView {
 public:
  CardExpirationDateFixFlowViewBridge(
      CardExpirationDateFixFlowController* controller,
      UIViewController* base_view_controller);
  ~CardExpirationDateFixFlowViewBridge() override;

  // CardExpirationDateFixFlowView:
  void Show() override;
  void ControllerGone() override;

  CardExpirationDateFixFlowController* GetController();

  // Called when the user confirms the expirationn date.
  void OnConfirmedExpirationDate(const base::string16& month,
                                 const base::string16& year);

  // Called when the user cancels the fix flow.
  void OnDismissed();

  // Closes the view.
  void PerformClose();

  // Deletes self. This should only be called by
  // CardExpirationDateFixFlowViewController after it finishes dismissing its
  // own UI elements.
  void DeleteSelf();

 protected:
  UIViewController* view_controller_;

 private:
  // The controller |this| queries for logic and state.
  CardExpirationDateFixFlowController* controller_;  // weak

  // Weak reference to the view controller used to present UI.
  __weak UIViewController* presenting_view_controller_;

  base::WeakPtrFactory<CardExpirationDateFixFlowViewBridge> weak_ptr_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(CardExpirationDateFixFlowViewBridge);
};

}  // namespace autofill

@interface CardExpirationDateFixFlowViewController : UITableViewController

// Designated initializer. |bridge| must not be null.
- (instancetype)initWithBridge:
    (autofill::CardExpirationDateFixFlowViewBridge*)bridge
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_CARD_EXPIRATION_DATE_FIX_FLOW_VIEW_BRIDGE_H_
