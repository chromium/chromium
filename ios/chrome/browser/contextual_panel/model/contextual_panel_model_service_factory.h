// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_MODEL_CONTEXTUAL_PANEL_MODEL_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_MODEL_CONTEXTUAL_PANEL_MODEL_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ContextualPanelModelService;

// Singleton that owns all ContextualPanelModelServices and associates them with
// profiles.
class ContextualPanelModelServiceFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  static ContextualPanelModelService* GetForProfile(ProfileIOS* profile);
  static ContextualPanelModelServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<ContextualPanelModelServiceFactory>;

  ContextualPanelModelServiceFactory();
  ~ContextualPanelModelServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_MODEL_CONTEXTUAL_PANEL_MODEL_SERVICE_FACTORY_H_
