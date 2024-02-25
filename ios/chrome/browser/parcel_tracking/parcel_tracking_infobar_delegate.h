// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PARCEL_TRACKING_PARCEL_TRACKING_INFOBAR_DELEGATE_H_
#define IOS_CHROME_BROWSER_PARCEL_TRACKING_PARCEL_TRACKING_INFOBAR_DELEGATE_H_

#import "base/memory/raw_ptr.h"
#import "components/infobars/core/confirm_infobar_delegate.h"
#import "ios/chrome/browser/parcel_tracking/parcel_tracking_step.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/parcel_tracking_opt_in_commands.h"
#import "ios/web/public/annotations/custom_text_checking_result.h"
#import "ios/web/public/web_state.h"

namespace commerce {
class ShoppingService;
}  // namespace commerce

// Delegate for infobar that prompts users to track package(s) and updates them
// when the package(s) are tracked or untracked.
class ParcelTrackingInfobarDelegate : public ConfirmInfoBarDelegate {
 public:
  ParcelTrackingInfobarDelegate(
      web::WebState* web_state,
      ParcelTrackingStep step,
      NSArray<CustomTextCheckingResult*>* parcel_list,
      id<ApplicationCommands> application_commands_handler,
      id<ParcelTrackingOptInCommands> parcel_tracking_commands_handler);

  ~ParcelTrackingInfobarDelegate() override;

  // Tracks the list of packages `parcel_list`. If `display_infobar` is true, an
  // infobar will be displayed to confirm the packages were tracked.
  void TrackPackages(bool display_infobar);
  // Untracks the list of packages `parcel_list`. If `display_infobar` is true,
  // an infobar will be displayed to confirm the packages were untracked.
  void UntrackPackages(bool display_infobar);
  // Opens a new NTP.
  void OpenNTP();
  // Sets the tracking step for the delegate.
  void SetStep(ParcelTrackingStep step);

  // Getters.
  NSArray<CustomTextCheckingResult*>* GetParcelList() const {
    return parcel_list_;
  }
  ParcelTrackingStep GetStep() const { return step_; }

  // ConfirmInfoBarDelegate implementation.
  InfoBarIdentifier GetIdentifier() const override;
  std::u16string GetMessageText() const override;
  bool EqualsDelegate(infobars::InfoBarDelegate* delegate) const override;

 private:
  raw_ptr<web::WebState> web_state_ = nullptr;
  ParcelTrackingStep step_;
  NSArray<CustomTextCheckingResult*>* parcel_list_;
  id<ApplicationCommands> application_commands_handler_;
  id<ParcelTrackingOptInCommands> parcel_tracking_commands_handler_;
  raw_ptr<commerce::ShoppingService> shopping_service_ = nullptr;
};

#endif  // IOS_CHROME_BROWSER_PARCEL_TRACKING_PARCEL_TRACKING_INFOBAR_DELEGATE_H_
