// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_RECORD_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_RECORD_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class DownloadRecordService;

// A factory to create a unique `DownloadRecordService` per
// profile.
class DownloadRecordServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static DownloadRecordServiceFactory* GetInstance();
  static DownloadRecordService* GetForProfile(ProfileIOS* profile);

 private:
  friend class base::NoDestructor<DownloadRecordServiceFactory>;

  DownloadRecordServiceFactory();
  ~DownloadRecordServiceFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_RECORD_SERVICE_FACTORY_H_
