// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_MODEL_CONTEXTUAL_PANEL_MODEL_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_MODEL_CONTEXTUAL_PANEL_MODEL_SERVICE_FACTORY_H_

#import "ios/chrome/browser/contextual_panel/model/contextual_panel_model_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that owns all ContextualPanelModelServices and associates them with
// profiles.
class ContextualPanelModelServiceFactory
    : public TypedProfileKeyedServiceFactoryIOS<
          ContextualPanelModelServiceFactory,
          ContextualPanelModelService> {
 public:
  explicit ContextualPanelModelServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_MODEL_CONTEXTUAL_PANEL_MODEL_SERVICE_FACTORY_H_
