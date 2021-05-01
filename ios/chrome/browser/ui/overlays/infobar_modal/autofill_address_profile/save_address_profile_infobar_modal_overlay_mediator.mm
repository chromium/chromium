// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/infobar_modal/autofill_address_profile/save_address_profile_infobar_modal_overlay_mediator.h"

#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/save_address_profile_infobar_modal_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/save_address_profile_infobar_modal_overlay_responses.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_save_address_profile_modal_consumer.h"
#import "ios/chrome/browser/ui/overlays/infobar_modal/infobar_modal_overlay_coordinator+modal_configuration.h"
#import "ios/chrome/browser/ui/overlays/overlay_request_mediator+subclassing.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using autofill_address_profile_infobar_overlays::
    SaveAddressProfileModalRequestConfig;
using save_address_profile_infobar_modal_responses::
    PresentAddressProfileSettings;

@interface SaveAddressProfileInfobarModalOverlayMediator ()
// The save address profile modal config from the request.
@property(nonatomic, assign, readonly)
    SaveAddressProfileModalRequestConfig* config;
@end

@implementation SaveAddressProfileInfobarModalOverlayMediator

#pragma mark - Accessors

- (SaveAddressProfileModalRequestConfig*)config {
  return self.request
             ? self.request->GetConfig<SaveAddressProfileModalRequestConfig>()
             : nullptr;
}

- (void)setConsumer:(id<InfobarSaveAddressProfileModalConsumer>)consumer {
  if (_consumer == consumer)
    return;

  _consumer = consumer;

  SaveAddressProfileModalRequestConfig* config = self.config;
  if (!_consumer || !config)
    return;

  NSDictionary* prefs = @{
    kAddressPrefKey : base::SysUTF16ToNSString(config->GetAddress()),
    kPhonePrefKey : base::SysUTF16ToNSString(config->GetPhoneNumber()),
    kEmailPrefKey : base::SysUTF16ToNSString(config->GetEmailAddress()),
    kCurrentAddressProfileSavedPrefKey :
        @(config->current_address_profile_saved())
  };
  [_consumer setupModalViewControllerWithPrefs:prefs];
}

#pragma mark - OverlayRequestMediator

+ (const OverlayRequestSupport*)requestSupport {
  return SaveAddressProfileModalRequestConfig::RequestSupport();
}

#pragma mark - InfobarSaveAddressProfileModalDelegate

- (void)presentAddressProfileSettings {
  // Receiving this delegate callback when the request is null means that the
  // present address profile settings button was tapped after the request was
  // cancelled, but before the modal UI has finished being dismissed.
  if (!self.request)
    return;

  [self dispatchResponse:OverlayResponse::CreateWithInfo<
                             PresentAddressProfileSettings>()];
  [self dismissInfobarModal:nil];
}

@end
