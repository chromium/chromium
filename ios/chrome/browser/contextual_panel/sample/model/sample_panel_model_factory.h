// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_SAMPLE_MODEL_SAMPLE_PANEL_MODEL_FACTORY_H_
#define IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_SAMPLE_MODEL_SAMPLE_PANEL_MODEL_FACTORY_H_

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class SamplePanelModel;

// Singleton that owns all SamplePanelModels and associates them with
// profiles.
class SamplePanelModelFactory : public BrowserStateKeyedServiceFactory {
 public:
  // TODO(crbug.com/358301380): remove this method.
  static SamplePanelModel* GetForBrowserState(ProfileIOS* profile);

  static SamplePanelModel* GetForProfile(ProfileIOS* profile);
  static SamplePanelModelFactory* GetInstance();

  SamplePanelModelFactory(const SamplePanelModelFactory&) = delete;
  SamplePanelModelFactory& operator=(const SamplePanelModelFactory&) = delete;

 private:
  friend class base::NoDestructor<SamplePanelModelFactory>;

  SamplePanelModelFactory();
  ~SamplePanelModelFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_SAMPLE_MODEL_SAMPLE_PANEL_MODEL_FACTORY_H_
