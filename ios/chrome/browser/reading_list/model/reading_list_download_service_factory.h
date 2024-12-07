// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READING_LIST_MODEL_READING_LIST_DOWNLOAD_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_READING_LIST_MODEL_READING_LIST_DOWNLOAD_SERVICE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ReadingListDownloadService;

// Singleton that creates the ReadingListDownloadService and associates that
// service with Profile.
class ReadingListDownloadServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static ReadingListDownloadService* GetForProfile(ProfileIOS* profile);
  static ReadingListDownloadServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<ReadingListDownloadServiceFactory>;

  ReadingListDownloadServiceFactory();
  ~ReadingListDownloadServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_READING_LIST_MODEL_READING_LIST_DOWNLOAD_SERVICE_FACTORY_H_
