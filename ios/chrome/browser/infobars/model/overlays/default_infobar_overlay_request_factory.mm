// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/model/overlays/default_infobar_overlay_request_factory.h"

#import "base/check.h"
#import "components/infobars/core/infobar_delegate.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_type.h"
#import "ios/chrome/browser/overlays/model/public/default/default_infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/model/public/infobar_banner/confirm_infobar_banner_overlay_request_config.h"
#import "ios/chrome/browser/overlays/model/public/infobar_banner/save_address_profile_infobar_banner_overlay_request_config.h"
#import "ios/chrome/browser/overlays/model/public/infobar_modal/save_address_profile_infobar_modal_overlay_request_config.h"

std::unique_ptr<OverlayRequest> DefaultInfobarOverlayRequestFactory(
    InfoBarIOS* infobar_ios,
    InfobarOverlayType overlay_type) {
  DCHECK(infobar_ios);
  switch (infobar_ios->infobar_type()) {
    case InfobarType::kInfobarTypeTailoredSecurityService:
    case InfobarType::kInfobarTypePasswordSave:
    case InfobarType::kInfobarTypePasswordUpdate:
    case InfobarType::kInfobarTypePermissions:
    case InfobarType::kInfobarTypeSaveCard:
    case InfobarType::kInfobarTypeSyncError:
    case InfobarType::kInfobarTypeTranslate:
    case InfobarType::kInfobarTypeParcelTracking:
    case InfobarType::kInfobarTypeEnhancedSafeBrowsing:
      return OverlayRequest::CreateWithConfig<
          DefaultInfobarOverlayRequestConfig>(infobar_ios, overlay_type);

    case InfobarType::kInfobarTypeConfirm:
      switch (overlay_type) {
        case InfobarOverlayType::kBanner:
          return OverlayRequest::CreateWithConfig<
              confirm_infobar_overlays::ConfirmBannerRequestConfig>(
              infobar_ios);

        case InfobarOverlayType::kModal:
          return nullptr;

        default:
          return nullptr;
      }

    case InfobarType::kInfobarTypeSaveAutofillAddressProfile:
      switch (overlay_type) {
        case InfobarOverlayType::kBanner:
          return OverlayRequest::CreateWithConfig<
              autofill_address_profile_infobar_overlays::
                  SaveAddressProfileBannerRequestConfig>(infobar_ios);

        case InfobarOverlayType::kModal:
          return OverlayRequest::CreateWithConfig<
              autofill_address_profile_infobar_overlays::
                  SaveAddressProfileModalRequestConfig>(infobar_ios);

        default:
          return nullptr;
      }

    default:
      return nullptr;
  }
}
