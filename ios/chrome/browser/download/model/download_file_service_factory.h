// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_FILE_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_FILE_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class DownloadFileService;
class ProfileIOS;

// Factory for DownloadFileService.
class DownloadFileServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static DownloadFileService* GetForProfile(ProfileIOS* profile);
  static DownloadFileServiceFactory* GetInstance();

  DownloadFileServiceFactory(const DownloadFileServiceFactory&) = delete;
  DownloadFileServiceFactory& operator=(const DownloadFileServiceFactory&) =
      delete;

 private:
  friend class base::NoDestructor<DownloadFileServiceFactory>;

  DownloadFileServiceFactory();
  ~DownloadFileServiceFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_FILE_SERVICE_FACTORY_H_
