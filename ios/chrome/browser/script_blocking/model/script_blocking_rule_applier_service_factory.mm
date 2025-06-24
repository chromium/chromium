// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/script_blocking/model/script_blocking_rule_applier_service_factory.h"

#import <memory>

#import "ios/chrome/browser/script_blocking/model/script_blocking_rule_applier_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/content_manager/content_rule_list_manager.h"

constexpr char kScriptBlockingRuleApplierServiceKey[] =
    "ScriptBlockingRuleApplierService";

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
    : ProfileKeyedServiceFactoryIOS(kScriptBlockingRuleApplierServiceKey,
                                    TestingCreation::kNoServiceForTests) {}

std::unique_ptr<KeyedService>
ScriptBlockingRuleApplierServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* browser_state) const {
  return std::make_unique<ScriptBlockingRuleApplierService>(
      web::ContentRuleListManager::FromBrowserState(browser_state));
}
