// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/download_file_service_factory.h"

#import "base/no_destructor.h"
#import "ios/chrome/browser/download/model/download_file_service.h"
#import "ios/chrome/browser/download/model/download_record_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

DownloadFileService* DownloadFileServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  CHECK(profile);
  return GetInstance()->GetServiceForProfileAs<DownloadFileService>(
      profile, /*create=*/true);
}

DownloadFileServiceFactory* DownloadFileServiceFactory::GetInstance() {
  static base::NoDestructor<DownloadFileServiceFactory> instance;
  return instance.get();
}

DownloadFileServiceFactory::DownloadFileServiceFactory()
    : ProfileKeyedServiceFactoryIOS("IOSDownloadFileService",
                                    ProfileSelection::kRedirectedInIncognito) {
  DependsOn(DownloadRecordServiceFactory::GetInstance());
}

DownloadFileServiceFactory::~DownloadFileServiceFactory() = default;

std::unique_ptr<KeyedService>
DownloadFileServiceFactory::BuildServiceInstanceFor(ProfileIOS* profile) const {
  // DownloadRecordService may be nullptr if IsDownloadListEnabled() is false.
  return std::make_unique<DownloadFileService>(
      DownloadRecordServiceFactory::GetForProfile(profile));
}
