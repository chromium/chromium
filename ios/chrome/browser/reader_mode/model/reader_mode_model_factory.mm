// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/reader_mode_model_factory.h"

#import "base/no_destructor.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_model.h"

// static
ReaderModeModel* ReaderModeModelFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<ReaderModeModel>(
      profile, /*create=*/true);
}

// static
ReaderModeModelFactory* ReaderModeModelFactory::GetInstance() {
  static base::NoDestructor<ReaderModeModelFactory> instance;
  return instance.get();
}

ReaderModeModelFactory::ReaderModeModelFactory()
    : ProfileKeyedServiceFactoryIOS("ReaderModeModel",
                                    ProfileSelection::kRedirectedInIncognito) {}

ReaderModeModelFactory::~ReaderModeModelFactory() = default;

std::unique_ptr<KeyedService> ReaderModeModelFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<ReaderModeModel>();
}
