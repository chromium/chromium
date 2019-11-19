// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_CARD_NAME_FIX_FLOW_VIEW_BRIDGE_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_CARD_NAME_FIX_FLOW_VIEW_BRIDGE_H_

#import <UIKit/UIKit.h>

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "components/autofill/core/browser/ui/payments/card_name_fix_flow_controller.h"
#include "components/autofill/core/browser/ui/payments/card_name_fix_flow_view.h"

namespace autofill {

class CardNameFixFlowController;

class CardNameFixFlowViewBridge : public CardNameFixFlowView {
 public:
  CardNameFixFlowViewBridge(CardNameFixFlowController* controller,
                            UIViewController* base_view_controller);
  ~CardNameFixFlowViewBridge() override;

  // CardNameFixFlowView:
  void Show() override;
  void ControllerGone() override;

  CardNameFixFlowController* GetController();

  // Called when the user confirms their name.
  void OnConfirmedName(const base::string16& confirmed_name);

  // Called when the user cancels the card name fix flow.
  void OnDismissed();

  // Closes the view.
  void PerformClose();

  // Deletes self. This should only be called by CardNameFixFlowViewController
  // after it finishes dismissing its own UI elements.
  void DeleteSelf();

 protected:
  UIViewController* view_controller_;

 private:
  // The controller |this| queries for logic and state.
  CardNameFixFlowController* controller_;  // weak

  // Weak reference to the view controller used to present UI.
  __weak UIViewController* presenting_view_controller_;

  base::WeakPtrFactory<CardNameFixFlowViewBridge> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CardNameFixFlowViewBridge);
};

}  // namespace autofill

@interface CardNameFixFlowViewController : UITableViewController

// Designated initializer. |bridge| must not be null.
- (instancetype)initWithBridge:(autofill::CardNameFixFlowViewBridge*)bridge
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_CARD_NAME_FIX_FLOW_VIEW_BRIDGE_H_
