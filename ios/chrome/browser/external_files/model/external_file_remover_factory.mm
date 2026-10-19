// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/external_files/model/external_file_remover_factory.h"

#import <memory>
#import <utility>

#import "ios/chrome/browser/external_files/model/external_file_remover_impl.h"
#import "ios/chrome/browser/sessions/model/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

ExternalFileRemoverFactory::ExternalFileRemoverFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(std::move(key),
                                         "ExternalFileRemoverService") {
  DependsOn(IOSChromeTabRestoreServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
ExternalFileRemoverFactory::BuildServiceInstanceFor(ProfileIOS* profile) const {
  return std::make_unique<ExternalFileRemoverImpl>(
      profile, IOSChromeTabRestoreServiceFactory::GetForProfile(profile));
}
