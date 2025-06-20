// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/model/lens_overlay_configuration_factory.h"

#import "base/check.h"
#import "ios/chrome/browser/lens/ui_bundled/lens_entrypoint.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_entrypoint.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/public/provider/chrome/browser/lens/lens_configuration.h"

namespace {

LensEntrypoint LensEntrypointFromOverlayEntrypoint(
    LensOverlayEntrypoint overlayEntrypoint) {
  switch (overlayEntrypoint) {
    case LensOverlayEntrypoint::kLocationBar:
      return LensEntrypoint::LensOverlayLocationBar;
    case LensOverlayEntrypoint::kOverflowMenu:
      return LensEntrypoint::LensOverlayOverflowMenu;
    case LensOverlayEntrypoint::kSearchImageContextMenu:
      return LensEntrypoint::ContextMenu;
    case LensOverlayEntrypoint::kLVFCameraCapture:
      return LensEntrypoint::LensOverlayLvfShutterButton;
    case LensOverlayEntrypoint::kLVFImagePicker:
      return LensEntrypoint::LensOverlayLvfGallery;
    case LensOverlayEntrypoint::kAIHub:
      return LensEntrypoint::LensOverlayAIHub;
    case LensOverlayEntrypoint::kFREPromo:
      return LensEntrypoint::LensOverlayFREPromo;
  }
}

}  // namespace

@implementation LensOverlayConfigurationFactory

- (LensConfiguration*)configurationForEntrypoint:
                          (LensOverlayEntrypoint)entrypoint
                                         profile:(ProfileIOS*)profile {
  LensEntrypoint lensEntrypoint =
      LensEntrypointFromOverlayEntrypoint(entrypoint);
  return [self configurationForLensEntrypoint:lensEntrypoint profile:profile];
}

- (LensConfiguration*)configurationForLensEntrypoint:(LensEntrypoint)entrypoint
                                             profile:(ProfileIOS*)profile {
  CHECK(profile, kLensOverlayNotFatalUntil);
  // Lens needs to have visibility into the user's identity and whether the
  // search should be incognito or not.
  LensConfiguration* configuration = [[LensConfiguration alloc] init];
  BOOL isIncognito = profile->IsOffTheRecord();
  configuration.isIncognito = isIncognito;
  configuration.singleSignOnService =
      GetApplicationContext()->GetSingleSignOnService();
  configuration.entrypoint = entrypoint;

  if (!isIncognito) {
    AuthenticationService* authenticationService =
        AuthenticationServiceFactory::GetForProfile(profile);
    id<SystemIdentity> identity = authenticationService->GetPrimaryIdentity(
        ::signin::ConsentLevel::kSignin);
    configuration.identity = identity;
  }
  configuration.localState = GetApplicationContext()->GetLocalState();
  return configuration;
}

@end
