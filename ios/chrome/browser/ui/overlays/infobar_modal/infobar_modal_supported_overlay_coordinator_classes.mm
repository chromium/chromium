// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/infobar_modal/infobar_modal_supported_overlay_coordinator_classes.h"

#import "ios/chrome/browser/ui/overlays/infobar_modal/autofill_address_profile/save_address_profile_infobar_modal_overlay_coordinator.h"
#import "ios/chrome/browser/ui/overlays/infobar_modal/passwords/password_infobar_modal_overlay_coordinator.h"
#import "ios/chrome/browser/ui/overlays/infobar_modal/permissions/permissions_infobar_modal_overlay_coordinator.h"
#import "ios/chrome/browser/ui/overlays/infobar_modal/reading_list/reading_list_infobar_modal_overlay_coordinator.h"
#import "ios/chrome/browser/ui/overlays/infobar_modal/save_card/save_card_infobar_modal_overlay_coordinator.h"
#import "ios/chrome/browser/ui/overlays/infobar_modal/translate/translate_infobar_modal_overlay_coordinator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace infobar_modal {

NSArray<Class>* GetSupportedOverlayCoordinatorClasses() {
  NSMutableArray<Class>* coordinatorClasses =
      [[NSMutableArray alloc] initWithArray:@[
        [PasswordInfobarModalOverlayCoordinator class],
        [ReadingListInfobarModalOverlayCoordinator class],
        [SaveAddressProfileInfobarModalOverlayCoordinator class],
        [SaveCardInfobarModalOverlayCoordinator class],
        [TranslateInfobarModalOverlayCoordinator class],
      ]];
  if (@available(iOS 15.0, *)) {
    [coordinatorClasses
        addObject:[PermissionsInfobarModalOverlayCoordinator class]];
  }
  return [coordinatorClasses copy];
}

}  // infobar_modal
