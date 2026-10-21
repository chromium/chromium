// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_FILE_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_FILE_SERVICE_FACTORY_H_

#import "ios/chrome/browser/download/model/download_file_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Factory for DownloadFileService.
class DownloadFileServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<DownloadFileServiceFactory,
                                                DownloadFileService> {
 public:
  explicit DownloadFileServiceFactory(PassKey key);

 private:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_FILE_SERVICE_FACTORY_H_
