// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/autofill_address_profile/save_address_profile_infobar_banner_interaction_handler.h"

#include "base/check.h"
#include "components/autofill/core/browser/autofill_save_update_address_profile_delegate_ios.h"
#include "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/infobars/overlays/infobar_overlay_request_cancel_handler.h"
#import "ios/chrome/browser/infobars/overlays/infobar_overlay_request_inserter.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/save_address_profile_infobar_banner_overlay_request_config.h"
#include "ios/chrome/browser/overlays/public/overlay_request_queue.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using save_address_profile_infobar_overlays::
    SaveAddressProfileBannerRequestConfig;

#pragma mark - InfobarBannerInteractionHandler

SaveAddressProfileInfobarBannerInteractionHandler::
    SaveAddressProfileInfobarBannerInteractionHandler()
    : InfobarBannerInteractionHandler(
          SaveAddressProfileBannerRequestConfig::RequestSupport()) {}

SaveAddressProfileInfobarBannerInteractionHandler::
    ~SaveAddressProfileInfobarBannerInteractionHandler() = default;

void SaveAddressProfileInfobarBannerInteractionHandler::MainButtonTapped(
    InfoBarIOS* infobar) {
  infobar->set_accepted(GetInfobarDelegate(infobar)->Accept());
}

void SaveAddressProfileInfobarBannerInteractionHandler::BannerVisibilityChanged(
    InfoBarIOS* infobar,
    bool visible) {
  if (!visible && !infobar->accepted())
    GetInfobarDelegate(infobar)->InfoBarDismissed();
}

void SaveAddressProfileInfobarBannerInteractionHandler::ShowModalButtonTapped(
    InfoBarIOS* infobar,
    web::WebState* web_state) {
  // Inform delegate that the modal is shown.
  GetInfobarDelegate(infobar)->set_modal_is_shown_to_true();

  InsertParams params(infobar);
  params.infobar = infobar;
  params.overlay_type = InfobarOverlayType::kModal;
  params.insertion_index = OverlayRequestQueue::FromWebState(
                               web_state, OverlayModality::kInfobarModal)
                               ->size();
  params.source = InfobarOverlayInsertionSource::kBanner;
  InfobarOverlayRequestInserter::FromWebState(web_state)->InsertOverlayRequest(
      params);
}

#pragma mark - Private

autofill::AutofillSaveUpdateAddressProfileDelegateIOS*
SaveAddressProfileInfobarBannerInteractionHandler::GetInfobarDelegate(
    InfoBarIOS* infobar) {
  autofill::AutofillSaveUpdateAddressProfileDelegateIOS* delegate =
      autofill::AutofillSaveUpdateAddressProfileDelegateIOS::
          FromInfobarDelegate(infobar->delegate());
  DCHECK(delegate);
  return delegate;
}
