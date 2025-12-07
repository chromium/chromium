// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/download_file_service_factory.h"

#import "base/no_destructor.h"
#import "ios/chrome/browser/download/model/download_file_service.h"
#import "ios/chrome/browser/download/model/download_record_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"

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
  // Only set dependency if the download list feature is enabled.
  if (IsDownloadListEnabled()) {
    DependsOn(DownloadRecordServiceFactory::GetInstance());
  }
}

DownloadFileServiceFactory::~DownloadFileServiceFactory() = default;

std::unique_ptr<KeyedService>
DownloadFileServiceFactory::BuildServiceInstanceFor(ProfileIOS* profile) const {
  // DownloadRecordService may be nullptr if IsDownloadListEnabled() is false.
  DownloadRecordService* download_record_service = nullptr;
  if (IsDownloadListEnabled()) {
    download_record_service =
        DownloadRecordServiceFactory::GetForProfile(profile);
  }
  return std::make_unique<DownloadFileService>(download_record_service);
}
