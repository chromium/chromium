// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/sample/model/sample_panel_model_factory.h"

#import "base/no_destructor.h"
#import "ios/chrome/browser/contextual_panel/sample/model/sample_panel_model.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

// static
SamplePanelModel* SamplePanelModelFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<SamplePanelModel>(
      profile, /*create=*/true);
}

// static
SamplePanelModelFactory* SamplePanelModelFactory::GetInstance() {
  static base::NoDestructor<SamplePanelModelFactory> instance;
  return instance.get();
}

SamplePanelModelFactory::SamplePanelModelFactory()
    : ProfileKeyedServiceFactoryIOS("SamplePanelModel") {}

SamplePanelModelFactory::~SamplePanelModelFactory() {}

std::unique_ptr<KeyedService> SamplePanelModelFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return std::make_unique<SamplePanelModel>();
}
