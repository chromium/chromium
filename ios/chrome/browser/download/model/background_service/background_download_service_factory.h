// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MODEL_BACKGROUND_SERVICE_BACKGROUND_DOWNLOAD_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MODEL_BACKGROUND_SERVICE_BACKGROUND_DOWNLOAD_SERVICE_FACTORY_H_

#import <memory>

#import "components/download/public/background_service/background_download_service.h"
#import "components/download/public/background_service/clients.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that owns all BackgroundDownloadServiceFactory and associates them
// with profiles.
class BackgroundDownloadServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          BackgroundDownloadServiceFactory,
          download::BackgroundDownloadService> {
 public:
  explicit BackgroundDownloadServiceFactory(PassKey key);

 private:
  friend class BackgroundDownloadServiceTest;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;

  std::unique_ptr<KeyedService> BuildServiceWithClients(
      ProfileIOS* profile,
      std::unique_ptr<download::DownloadClientMap> clients) const;
};

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_BACKGROUND_SERVICE_BACKGROUND_DOWNLOAD_SERVICE_FACTORY_H_
