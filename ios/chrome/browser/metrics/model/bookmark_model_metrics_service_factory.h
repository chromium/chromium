// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_MODEL_BOOKMARK_MODEL_METRICS_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_METRICS_MODEL_BOOKMARK_MODEL_METRICS_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class BookmarkModelMetricsService;
class ProfileIOS;

// A factory that owns all BookmarkModelMetricsService and associate
// the to ProfileIOS instances.
class BookmarkModelMetricsServiceFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  static BookmarkModelMetricsService* GetForProfile(ProfileIOS* profile);
  static BookmarkModelMetricsServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<BookmarkModelMetricsServiceFactory>;

  BookmarkModelMetricsServiceFactory();
  ~BookmarkModelMetricsServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_METRICS_MODEL_BOOKMARK_MODEL_METRICS_SERVICE_FACTORY_H_
