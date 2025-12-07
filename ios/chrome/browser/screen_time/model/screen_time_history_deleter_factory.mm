// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/screen_time/model/screen_time_history_deleter_factory.h"

#import "base/no_destructor.h"
#import "components/keyed_service/core/service_access_type.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/screen_time/model/features.h"
#import "ios/chrome/browser/screen_time/model/screen_time_history_deleter.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

// static
ScreenTimeHistoryDeleter* ScreenTimeHistoryDeleterFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<ScreenTimeHistoryDeleter>(
      profile, /*create=*/true);
}

// static
ScreenTimeHistoryDeleterFactory*
ScreenTimeHistoryDeleterFactory::GetInstance() {
  static base::NoDestructor<ScreenTimeHistoryDeleterFactory> instance;
  return instance.get();
}

ScreenTimeHistoryDeleterFactory::ScreenTimeHistoryDeleterFactory()
    : ProfileKeyedServiceFactoryIOS("ScreenTimeHistoryDeleter",
                                    ProfileSelection::kRedirectedInIncognito,
                                    TestingCreation::kNoServiceForTests) {
  DependsOn(ios::HistoryServiceFactory::GetInstance());
}

ScreenTimeHistoryDeleterFactory::~ScreenTimeHistoryDeleterFactory() {}

std::unique_ptr<KeyedService>
ScreenTimeHistoryDeleterFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  if (!IsScreenTimeIntegrationEnabled()) {
    return nullptr;
  }

  history::HistoryService* history_service =
      ios::HistoryServiceFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS);
  return std::make_unique<ScreenTimeHistoryDeleter>(history_service);
}
