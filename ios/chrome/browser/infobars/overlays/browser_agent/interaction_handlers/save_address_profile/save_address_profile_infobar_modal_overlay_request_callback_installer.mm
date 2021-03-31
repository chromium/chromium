// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/save_address_profile/save_address_profile_infobar_modal_overlay_request_callback_installer.h"

#include "base/check.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/infobars/infobar_ios.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/save_address_profile/save_address_profile_infobar_modal_interaction_handler.h"
#include "ios/chrome/browser/infobars/overlays/infobar_overlay_util.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/save_address_profile_infobar_modal_overlay_request_config.h"
#include "ios/chrome/browser/overlays/public/overlay_callback_manager.h"
#import "ios/chrome/browser/overlays/public/overlay_response.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using save_address_profile_infobar_overlays::
    SaveAddressProfileModalRequestConfig;

SaveAddressProfileInfobarModalOverlayRequestCallbackInstaller::
    SaveAddressProfileInfobarModalOverlayRequestCallbackInstaller(
        SaveAddressProfileInfobarModalInteractionHandler* interaction_handler)
    : InfobarModalOverlayRequestCallbackInstaller(
          SaveAddressProfileModalRequestConfig::RequestSupport(),
          interaction_handler),
      interaction_handler_(interaction_handler) {
  DCHECK(interaction_handler_);
}

SaveAddressProfileInfobarModalOverlayRequestCallbackInstaller::
    ~SaveAddressProfileInfobarModalOverlayRequestCallbackInstaller() = default;
