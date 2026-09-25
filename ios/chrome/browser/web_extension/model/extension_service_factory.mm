// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_extension/model/extension_service_factory.h"

#import <utility>

#import "base/check_deref.h"
#import "base/ios/ios_util.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/universal_optout/model/eligibility_utils.h"
#import "ios/chrome/browser/universal_optout/model/universal_optout_service_factory.h"
#import "ios/chrome/browser/web_extension/model/extension_service.h"
#import "ios/chrome/browser/web_extension/model/extension_service_impl.h"
#import "ios/web/public/extension/extension_controller.h"
#import "ios/web/public/web_client.h"

// static
ExtensionService* ExtensionServiceFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<ExtensionService>(
      profile, /*create=*/true);
}

// static
ExtensionServiceFactory* ExtensionServiceFactory::GetInstance() {
  static base::NoDestructor<ExtensionServiceFactory> instance;
  return instance.get();
}

namespace {

// Returns whether the current OS version is supported. The GPC extension is
// only supported on iOS versions between iOS 18.4 and iOS 27.
bool IsSupportedOS() {
  if (@available(iOS 18.4, *)) {
    return !base::ios::IsRunningOnIOS27OrLater();
  }
  return false;
}

std::unique_ptr<KeyedService> BuildExtensionService(ProfileIOS* profile) {
  // Give the opportunity for the test hook to override the factory from
  // the provider (allowing EG tests to use a fake ExtensionService).
  if (auto extension_service = tests_hook::CreateExtensionService(profile)) {
    return extension_service;
  }

  if (!IsSupportedOS()) {
    return nullptr;
  }

  universal_optout::UniversalOptOutService* optout_service =
      universal_optout::UniversalOptOutServiceFactory::GetForProfile(profile);

  const web::UniversalOptOutState opt_out_state =
      universal_optout::GetUniversalOptOutState(profile->GetPrefs(),
                                                optout_service);
  if (opt_out_state == web::UniversalOptOutState::kNotEligible) {
    return nullptr;
  }

  if (@available(iOS 18.4, *)) {
    std::unique_ptr<web::ExtensionController> extension_controller =
        web::ExtensionController::Create();
    if (!extension_controller) {
      return nullptr;
    }
    auto service = std::make_unique<ExtensionServiceImpl>(
        CHECK_DEREF(profile->GetPrefs()), std::move(extension_controller));
    service->Initialize(opt_out_state);
    return service;
  }
  return nullptr;
}

}  // namespace

// static
ExtensionServiceFactory::TestingFactory
ExtensionServiceFactory::GetDefaultFactory() {
  return base::BindOnce(&BuildExtensionService);
}

ExtensionServiceFactory::ExtensionServiceFactory()
    : ProfileKeyedServiceFactoryIOS("ExtensionService",
                                    ProfileSelection::kRedirectedInIncognito,
                                    ServiceCreation::kCreateWithProfile,
                                    TestingCreation::kNoServiceForTests) {
  DependsOn(universal_optout::UniversalOptOutServiceFactory::GetInstance());
}

ExtensionServiceFactory::~ExtensionServiceFactory() = default;

std::unique_ptr<KeyedService> ExtensionServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return BuildExtensionService(profile);
}
