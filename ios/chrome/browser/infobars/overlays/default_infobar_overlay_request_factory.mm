// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/default_infobar_overlay_request_factory.h"

#import "base/check.h"
#import "components/infobars/core/infobar_delegate.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/infobars/infobar_type.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/confirm_infobar_banner_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/password_infobar_banner_overlay.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/permissions_infobar_banner_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/save_address_profile_infobar_banner_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/save_card_infobar_banner_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/sync_error_infobar_banner_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/tailored_security_service_infobar_banner_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/translate_infobar_banner_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/password_infobar_modal_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/permissions/permissions_modal_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/save_address_profile_infobar_modal_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/save_card_infobar_modal_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/translate_infobar_modal_overlay_request_config.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

std::unique_ptr<OverlayRequest> DefaultInfobarOverlayRequestFactory(
    InfoBarIOS* infobar_ios,
    InfobarOverlayType overlay_type) {
  DCHECK(infobar_ios);
  switch (infobar_ios->infobar_type()) {
    case InfobarType::kInfobarTypePasswordSave:
    case InfobarType::kInfobarTypePasswordUpdate:
      switch (overlay_type) {
        case InfobarOverlayType::kBanner:
          return OverlayRequest::CreateWithConfig<
              PasswordInfobarBannerOverlayRequestConfig>(infobar_ios);

        case InfobarOverlayType::kModal:
          return OverlayRequest::CreateWithConfig<
              PasswordInfobarModalOverlayRequestConfig>(infobar_ios);

        default:
          return nullptr;
      }

    case InfobarType::kInfobarTypeTranslate:
      switch (overlay_type) {
        case InfobarOverlayType::kBanner:
          return OverlayRequest::CreateWithConfig<
              translate_infobar_overlays::TranslateBannerRequestConfig>(
              infobar_ios);

        case InfobarOverlayType::kModal:
          return OverlayRequest::CreateWithConfig<
              translate_infobar_overlays::TranslateModalRequestConfig>(
              infobar_ios);

        default:
          return nullptr;
      }

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

    case InfobarType::kInfobarTypeSaveCard:
      switch (overlay_type) {
        case InfobarOverlayType::kBanner:
          return OverlayRequest::CreateWithConfig<
              save_card_infobar_overlays::SaveCardBannerRequestConfig>(
              infobar_ios);

        case InfobarOverlayType::kModal:
          return OverlayRequest::CreateWithConfig<
              save_card_infobar_overlays::SaveCardModalRequestConfig>(
              infobar_ios);

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

    case InfobarType::kInfobarTypePermissions:
      switch (overlay_type) {
        case InfobarOverlayType::kBanner:
          return OverlayRequest::CreateWithConfig<
              PermissionsBannerRequestConfig>(infobar_ios);
        case InfobarOverlayType::kModal:
          return OverlayRequest::CreateWithConfig<
              PermissionsInfobarModalOverlayRequestConfig>(infobar_ios);
        default:
          return nullptr;
      }

    case InfobarType::kInfobarTypeTailoredSecurityService:
      if (overlay_type == InfobarOverlayType::kBanner) {
        return OverlayRequest::CreateWithConfig<
            tailored_security_service_infobar_overlays::
                TailoredSecurityServiceBannerRequestConfig>(infobar_ios);
      }
      return nullptr;
    case InfobarType::kInfobarTypeSyncError:
      if (overlay_type == InfobarOverlayType::kBanner) {
        return OverlayRequest::CreateWithConfig<
            sync_error_infobar_overlays::SyncErrorBannerRequestConfig>(
            infobar_ios);
      }
      return nullptr;
    default:
      return nullptr;
  }
}
