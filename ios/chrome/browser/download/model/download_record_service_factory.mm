// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/download_record_service_factory.h"

#import <memory>
#import <utility>

#import "base/files/file_path.h"
#import "ios/chrome/browser/download/model/download_record_service_impl.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"

DownloadRecordServiceFactory::DownloadRecordServiceFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "IOSDownloadRecordService",
          ProfileSelection::kRedirectedInIncognito) {}

std::unique_ptr<KeyedService>
DownloadRecordServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  if (!IsDownloadListEnabled()) {
    return nullptr;
  }
  return std::make_unique<DownloadRecordServiceImpl>(profile->GetStatePath());
}
