// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_MODEL_BOOKMARK_MODEL_METRICS_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_METRICS_MODEL_BOOKMARK_MODEL_METRICS_SERVICE_FACTORY_H_

#import "ios/chrome/browser/metrics/model/bookmark_model_metrics_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// A factory that owns all BookmarkModelMetricsService and associate
// the to ProfileIOS instances.
class BookmarkModelMetricsServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          BookmarkModelMetricsServiceFactory,
          BookmarkModelMetricsService> {
 public:
  explicit BookmarkModelMetricsServiceFactory(PassKey key);

 private:
  // BrowserContextKeyedServiceFactory:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_METRICS_MODEL_BOOKMARK_MODEL_METRICS_SERVICE_FACTORY_H_
