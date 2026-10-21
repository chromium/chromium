// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/sample/model/sample_panel_model_factory.h"

#import <utility>

#import "ios/chrome/browser/contextual_panel/sample/model/sample_panel_model.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

SamplePanelModelFactory::SamplePanelModelFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(std::move(key), "SamplePanelModel") {}

std::unique_ptr<KeyedService> SamplePanelModelFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<SamplePanelModel>();
}
