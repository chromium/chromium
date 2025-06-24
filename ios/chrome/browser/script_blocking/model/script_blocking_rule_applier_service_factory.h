// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SCRIPT_BLOCKING_MODEL_SCRIPT_BLOCKING_RULE_APPLIER_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SCRIPT_BLOCKING_MODEL_SCRIPT_BLOCKING_RULE_APPLIER_SERVICE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ScriptBlockingRuleApplierService;
class ProfileIOS;

// Factory for creating ScriptBlockingRuleApplierService instances.
class ScriptBlockingRuleApplierServiceFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  static ScriptBlockingRuleApplierServiceFactory* GetInstance();
  static ScriptBlockingRuleApplierService* GetForProfile(ProfileIOS* profile);

 private:
  friend class base::NoDestructor<ScriptBlockingRuleApplierServiceFactory>;

  ScriptBlockingRuleApplierServiceFactory();
  ~ScriptBlockingRuleApplierServiceFactory() override = default;

  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* state) const override;
};

#endif  // IOS_CHROME_BROWSER_SCRIPT_BLOCKING_MODEL_SCRIPT_BLOCKING_RULE_APPLIER_SERVICE_FACTORY_H_
