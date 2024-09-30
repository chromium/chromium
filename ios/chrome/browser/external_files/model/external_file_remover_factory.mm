// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/external_files/model/external_file_remover_factory.h"

#import <memory>
#import <utility>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/external_files/model/external_file_remover_impl.h"
#import "ios/chrome/browser/sessions/model/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

// static
ExternalFileRemover* ExternalFileRemoverFactory::GetForBrowserState(
    ProfileIOS* profile) {
  return GetForProfile(profile);
}

// static
ExternalFileRemover* ExternalFileRemoverFactory::GetForProfile(
    ProfileIOS* profile) {
  return static_cast<ExternalFileRemover*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

// static
ExternalFileRemoverFactory* ExternalFileRemoverFactory::GetInstance() {
  static base::NoDestructor<ExternalFileRemoverFactory> instance;
  return instance.get();
}

ExternalFileRemoverFactory::ExternalFileRemoverFactory()
    : BrowserStateKeyedServiceFactory(
          "ExternalFileRemoverService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(IOSChromeTabRestoreServiceFactory::GetInstance());
}

ExternalFileRemoverFactory::~ExternalFileRemoverFactory() {}

std::unique_ptr<KeyedService>
ExternalFileRemoverFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  return std::make_unique<ExternalFileRemoverImpl>(
      profile, IOSChromeTabRestoreServiceFactory::GetForProfile(profile));
}
