// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_SAMPLE_MODEL_SAMPLE_PANEL_MODEL_FACTORY_H_
#define IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_SAMPLE_MODEL_SAMPLE_PANEL_MODEL_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class SamplePanelModel;

// Singleton that owns all SamplePanelModels and associates them with
// profiles.
class SamplePanelModelFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static SamplePanelModel* GetForProfile(ProfileIOS* profile);
  static SamplePanelModelFactory* GetInstance();

 private:
  friend class base::NoDestructor<SamplePanelModelFactory>;

  SamplePanelModelFactory();
  ~SamplePanelModelFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_SAMPLE_MODEL_SAMPLE_PANEL_MODEL_FACTORY_H_
