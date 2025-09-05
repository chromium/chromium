// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/script_blocking/model/script_blocking_rule_applier_service_factory.h"

#import <memory>

#import "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_features.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/privacy_sandbox/tracking_protection_settings.h"
#import "ios/chrome/browser/privacy_sandbox/tracking_protection_settings_factory.h"
#import "ios/chrome/browser/script_blocking/model/script_blocking_rule_applier_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/content_manager/content_rule_list_manager.h"

// static
ScriptBlockingRuleApplierServiceFactory*
ScriptBlockingRuleApplierServiceFactory::GetInstance() {
  static base::NoDestructor<ScriptBlockingRuleApplierServiceFactory> instance;
  return instance.get();
}

// static
ScriptBlockingRuleApplierService*
ScriptBlockingRuleApplierServiceFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<ScriptBlockingRuleApplierService>(
          profile, /*create=*/true);
}

ScriptBlockingRuleApplierServiceFactory::
    ScriptBlockingRuleApplierServiceFactory()
    : ProfileKeyedServiceFactoryIOS("ScriptBlockingRuleApplierService",
                                    ServiceCreation::kCreateWithProfile,
                                    TestingCreation::kNoServiceForTests,
                                    ProfileSelection::kOwnInstanceInIncognito) {
  DependsOn(TrackingProtectionSettingsFactory::GetInstance());
}

std::unique_ptr<KeyedService>
ScriptBlockingRuleApplierServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<ScriptBlockingRuleApplierService>(
      web::ContentRuleListManager::FromBrowserState(profile),
      TrackingProtectionSettingsFactory::GetForProfile(profile),
      profile->IsOffTheRecord());
}
