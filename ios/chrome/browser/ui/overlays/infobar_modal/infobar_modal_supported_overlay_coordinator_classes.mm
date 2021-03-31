// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/infobar_modal/infobar_modal_supported_overlay_coordinator_classes.h"

#import "ios/chrome/browser/ui/overlays/infobar_modal/passwords/password_infobar_modal_overlay_coordinator.h"
#import "ios/chrome/browser/ui/overlays/infobar_modal/save_address_profile/save_address_profile_infobar_modal_overlay_coordinator.h"
#import "ios/chrome/browser/ui/overlays/infobar_modal/save_card/save_card_infobar_modal_overlay_coordinator.h"
#import "ios/chrome/browser/ui/overlays/infobar_modal/translate/translate_infobar_modal_overlay_coordinator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace infobar_modal {

NSArray<Class>* GetSupportedOverlayCoordinatorClasses() {
  return @[
    [PasswordInfobarModalOverlayCoordinator class],
    [TranslateInfobarModalOverlayCoordinator class],
    [SaveAddressProfileInfobarModalOverlayCoordinator class],
    [SaveCardInfobarModalOverlayCoordinator class]
  ];
}

}  // infobar_modal
