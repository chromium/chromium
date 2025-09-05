// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/download_record_service_factory.h"

#import <memory>

#import "base/files/file_path.h"
#import "ios/chrome/browser/download/model/download_record_service_impl.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

DownloadRecordServiceFactory* DownloadRecordServiceFactory::GetInstance() {
  static base::NoDestructor<DownloadRecordServiceFactory> instance;
  return instance.get();
}

DownloadRecordService* DownloadRecordServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  CHECK(profile);
  return GetInstance()->GetServiceForProfileAs<DownloadRecordService>(
      profile, /*create=*/true);
}

DownloadRecordServiceFactory::DownloadRecordServiceFactory()
    : ProfileKeyedServiceFactoryIOS("IOSDownloadRecordService",
                                    ProfileSelection::kRedirectedInIncognito) {}

DownloadRecordServiceFactory::~DownloadRecordServiceFactory() = default;

std::unique_ptr<KeyedService>
DownloadRecordServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<DownloadRecordServiceImpl>(profile->GetStatePath());
}
