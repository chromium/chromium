// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/infobar_banner_overlay_mediator_factory.h"

#import <memory>

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/no_destructor.h"
#import "base/notreached.h"
#import "ios/chrome/browser/infobars/model/infobar_type.h"
#import "ios/chrome/browser/overlays/model/public/default/default_infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_support.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/autofill_address_profile/save_address_profile_infobar_banner_overlay_mediator.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/collaboration_group/collaboration_group_infobar_banner_overlay_mediator.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/collaboration_out_of_date/collaboration_out_of_date_infobar_banner_overlay_mediator.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/confirm/confirm_infobar_banner_overlay_mediator.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/passwords/password_infobar_banner_overlay_mediator.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/permissions/permissions_infobar_banner_overlay_mediator.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/safe_browsing/enhanced_safe_browsing_infobar_overlay_mediator.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/save_card/save_card_infobar_banner_overlay_mediator.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/save_cvc/save_cvc_infobar_banner_overlay_mediator.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/sync_error/sync_error_infobar_banner_overlay_mediator.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/tailored_security/tailored_security_infobar_banner_overlay_mediator.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/translate/translate_infobar_banner_overlay_mediator.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_mediator_util.h"

namespace {

// Returns the list of supported mediator classes.
NSArray<Class>* GetSupportedMediatorClasses() {
  return @[
    [PasswordInfobarBannerOverlayMediator class],
    [ConfirmInfobarBannerOverlayMediator class],
    [TranslateInfobarBannerOverlayMediator class],
    [SaveCardInfobarBannerOverlayMediator class],
    [SaveCVCInfobarBannerOverlayMediator class],
    [SaveAddressProfileInfobarBannerOverlayMediator class],
    [PermissionsBannerOverlayMediator class],
    [TailoredSecurityInfobarBannerOverlayMediator class],
    [SyncErrorInfobarBannerOverlayMediator class],
    [EnhancedSafeBrowsingBannerOverlayMediator class],
  ];
}

// Returns the mediator corresponding to the given `infobar_type` for `request`.
InfobarBannerOverlayMediator* CreateMediatorForInfobarType(
    InfobarType infobar_type,
    OverlayRequest* request) {
  Class mediator_class = nil;

  switch (infobar_type) {
    case InfobarType::kInfobarTypePasswordSave:
    case InfobarType::kInfobarTypePasswordUpdate:
      mediator_class = [PasswordInfobarBannerOverlayMediator class];
      break;
    case InfobarType::kInfobarTypePermissions:
      mediator_class = [PermissionsBannerOverlayMediator class];
      break;
    case InfobarType::kInfobarTypeTailoredSecurityService:
      mediator_class = [TailoredSecurityInfobarBannerOverlayMediator class];
      break;
    case InfobarType::kInfobarTypeSaveCard:
      mediator_class = [SaveCardInfobarBannerOverlayMediator class];
      break;
    case InfobarType::kInfobarTypeSaveCvc:
      mediator_class = [SaveCVCInfobarBannerOverlayMediator class];
      break;
    case InfobarType::kInfobarTypeSyncError:
      mediator_class = [SyncErrorInfobarBannerOverlayMediator class];
      break;
    case InfobarType::kInfobarTypeTranslate:
      mediator_class = [TranslateInfobarBannerOverlayMediator class];
      break;
    case InfobarType::kInfobarTypeEnhancedSafeBrowsing:
      mediator_class = [EnhancedSafeBrowsingBannerOverlayMediator class];
      break;
    case InfobarType::kInfobarTypeCollaborationGroup:
      mediator_class = [CollaborationGroupInfobarBannerOverlayMediator class];
      break;
    case InfobarType::kInfobarTypeCollaborationOutOfDate:
      mediator_class =
          [CollaborationOutOfDateInfobarBannerOverlayMediator class];
      break;
    default:
      NOTREACHED() << "Received unsupported infobarType.";
  }

  return [[mediator_class alloc] initWithRequest:request];
}

}  // namespace

@implementation InfobarBannerOverlayMediator (Factory)

#pragma mark - Public

+ (const OverlayRequestSupport*)requestSupport {
  static base::NoDestructor<std::unique_ptr<const OverlayRequestSupport>>
      _requestSupport(
          CreateAggregateSupportForMediators(GetSupportedMediatorClasses()));
  return _requestSupport->get();
}

+ (instancetype)mediatorForRequest:(OverlayRequest*)request {
  if (DefaultInfobarOverlayRequestConfig::RequestSupport()->IsRequestSupported(
          request)) {
    DefaultInfobarOverlayRequestConfig* config =
        request->GetConfig<DefaultInfobarOverlayRequestConfig>();
    return CreateMediatorForInfobarType(config->infobar_type(), request);
  }

  InfobarBannerOverlayMediator* mediator =
      base::apple::ObjCCast<InfobarBannerOverlayMediator>(
          GetMediatorForRequest(GetSupportedMediatorClasses(), request));
  CHECK(mediator) << "None of the supported mediator classes support request.";
  return mediator;
}

@end
