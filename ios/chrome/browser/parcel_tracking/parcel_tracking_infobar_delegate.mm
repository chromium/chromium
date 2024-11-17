// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/parcel_tracking/parcel_tracking_infobar_delegate.h"

#import "components/commerce/core/proto/parcel.pb.h"
#import "components/commerce/core/shopping_service.h"
#import "ios/chrome/browser/commerce/model/shopping_service_factory.h"
#import "ios/chrome/browser/parcel_tracking/metrics.h"
#import "ios/chrome/browser/parcel_tracking/parcel_tracking_util.h"
#import "ios/chrome/browser/parcel_tracking/tracking_source.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"

ParcelTrackingInfobarDelegate::ParcelTrackingInfobarDelegate(
    web::WebState* web_state,
    ParcelTrackingStep step,
    NSArray<CustomTextCheckingResult*>* parcel_list,
    id<ApplicationCommands> application_commands_handler,
    id<ParcelTrackingOptInCommands> parcel_tracking_commands_handler)
    : web_state_(web_state),
      step_(step),
      parcel_list_(parcel_list),
      application_commands_handler_(application_commands_handler),
      parcel_tracking_commands_handler_(parcel_tracking_commands_handler) {
  shopping_service_ = commerce::ShoppingServiceFactory::GetForProfile(
      ProfileIOS::FromBrowserState(web_state->GetBrowserState()));
}

ParcelTrackingInfobarDelegate::~ParcelTrackingInfobarDelegate() = default;

#pragma mark - Public

void ParcelTrackingInfobarDelegate::TrackPackages(bool display_infobar) {
  // Track parcels and display an infobar to confirm that parcels are tracked.
  TrackParcels(shopping_service_, parcel_list_, std::string(),
               parcel_tracking_commands_handler_, display_infobar,
               TrackingSource::kInfobar);
}

void ParcelTrackingInfobarDelegate::UntrackPackages(bool display_infobar) {
  shopping_service_->StopTrackingParcels(
      ConvertCustomTextCheckingResult(parcel_list_),
      base::BindOnce(
          [](bool display_infobar,
             id<ParcelTrackingOptInCommands> parcel_tracking_commands_handler,
             NSArray<CustomTextCheckingResult*>* parcels, bool success) {
            if (success) {
              parcel_tracking::RecordParcelsUntracked(TrackingSource::kInfobar,
                                                      parcels.count);
              if (display_infobar) {
                [parcel_tracking_commands_handler
                    showParcelTrackingInfobarWithParcels:parcels
                                                 forStep:ParcelTrackingStep::
                                                             kPackageUntracked];
              }
            }
          },
          display_infobar, parcel_tracking_commands_handler_, parcel_list_));
}

void ParcelTrackingInfobarDelegate::OpenNTP() {
  [application_commands_handler_ openURLInNewTab:[OpenNewTabCommand command]];
}

void ParcelTrackingInfobarDelegate::SetStep(ParcelTrackingStep step) {
  step_ = step;
}

#pragma mark - ConfirmInfoBarDelegate

infobars::InfoBarDelegate::InfoBarIdentifier
ParcelTrackingInfobarDelegate::GetIdentifier() const {
  return PARCEL_TRACKING_INFOBAR_DELEGATE;
}

// Returns an empty message to satisfy implementation requirement for
// ConfirmInfoBarDelegate.
std::u16string ParcelTrackingInfobarDelegate::GetMessageText() const {
  return std::u16string();
}

bool ParcelTrackingInfobarDelegate::EqualsDelegate(
    infobars::InfoBarDelegate* delegate) const {
  return delegate->GetIdentifier() == GetIdentifier();
}
