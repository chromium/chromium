// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/infobar_overlay_request_factory_impl.h"

#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/confirm_infobar_banner_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/save_address_profile_infobar_banner_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/save_card_infobar_banner_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/save_password_infobar_banner_overlay.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/translate_infobar_banner_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/update_password_infobar_banner_overlay.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/password_infobar_modal_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/save_address_profile_infobar_modal_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/save_card_infobar_modal_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/translate_infobar_modal_overlay_request_config.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using infobars::InfoBar;
using translate_infobar_overlays::TranslateBannerRequestConfig;
using translate_infobar_overlays::TranslateModalRequestConfig;
using confirm_infobar_overlays::ConfirmBannerRequestConfig;
using save_card_infobar_overlays::SaveCardBannerRequestConfig;
using save_card_infobar_overlays::SaveCardModalRequestConfig;
using autofill_address_profile_infobar_overlays::
    SaveAddressProfileBannerRequestConfig;
using autofill_address_profile_infobar_overlays::
    SaveAddressProfileModalRequestConfig;

InfobarOverlayRequestFactoryImpl::InfobarOverlayRequestFactoryImpl() {
  SetUpFactories(InfobarType::kInfobarTypePasswordSave,
                 CreateFactory<SavePasswordInfobarBannerOverlayRequestConfig>(),
                 CreateFactory<PasswordInfobarModalOverlayRequestConfig>());
  SetUpFactories(
      InfobarType::kInfobarTypePasswordUpdate,
      CreateFactory<UpdatePasswordInfobarBannerOverlayRequestConfig>(),
      CreateFactory<PasswordInfobarModalOverlayRequestConfig>());
  SetUpFactories(InfobarType::kInfobarTypeTranslate,
                 CreateFactory<TranslateBannerRequestConfig>(),
                 CreateFactory<TranslateModalRequestConfig>());
  SetUpFactories(InfobarType::kInfobarTypeConfirm,
                 CreateFactory<ConfirmBannerRequestConfig>(),
                 /*modal_factory=*/nullptr);
  SetUpFactories(InfobarType::kInfobarTypeSaveCard,
                 CreateFactory<SaveCardBannerRequestConfig>(),
                 CreateFactory<SaveCardModalRequestConfig>());
  SetUpFactories(InfobarType::kInfobarTypeSaveAutofillAddressProfile,
                 CreateFactory<SaveAddressProfileBannerRequestConfig>(),
                 CreateFactory<SaveAddressProfileModalRequestConfig>());
}

InfobarOverlayRequestFactoryImpl::~InfobarOverlayRequestFactoryImpl() = default;

std::unique_ptr<OverlayRequest>
InfobarOverlayRequestFactoryImpl::CreateInfobarRequest(
    InfoBar* infobar,
    InfobarOverlayType type) {
  DCHECK(infobar);
  InfoBarIOS* infobar_ios = static_cast<InfoBarIOS*>(infobar);
  FactoryHelper* factory = factory_storages_[infobar_ios->infobar_type()][type];
  return factory ? factory->CreateInfobarRequest(infobar_ios) : nullptr;
}

void InfobarOverlayRequestFactoryImpl::SetUpFactories(
    InfobarType type,
    std::unique_ptr<FactoryHelper> banner_factory,
    std::unique_ptr<FactoryHelper> modal_factory) {
  factory_storages_.emplace(
      type, FactoryHelperStorage(std::move(banner_factory),
                                 std::move(modal_factory)));
}

#pragma mark - FactoryHelperStorage

InfobarOverlayRequestFactoryImpl::FactoryHelperStorage::FactoryHelperStorage() =
    default;

InfobarOverlayRequestFactoryImpl::FactoryHelperStorage::FactoryHelperStorage(
    std::unique_ptr<FactoryHelper> banner_factory,
    std::unique_ptr<FactoryHelper> modal_factory) {
  factories_[InfobarOverlayType::kBanner] = std::move(banner_factory);
  factories_[InfobarOverlayType::kModal] = std::move(modal_factory);
}

InfobarOverlayRequestFactoryImpl::FactoryHelperStorage::FactoryHelperStorage(
    InfobarOverlayRequestFactoryImpl::FactoryHelperStorage&& storage) {
  factories_[InfobarOverlayType::kBanner] =
      std::move(storage.factories_[InfobarOverlayType::kBanner]);
  factories_[InfobarOverlayType::kModal] =
      std::move(storage.factories_[InfobarOverlayType::kModal]);
}

InfobarOverlayRequestFactoryImpl::FactoryHelperStorage::
    ~FactoryHelperStorage() = default;

InfobarOverlayRequestFactoryImpl::FactoryHelper*
    InfobarOverlayRequestFactoryImpl::FactoryHelperStorage::operator[](
        InfobarOverlayType type) {
  return factories_[type].get();
}
