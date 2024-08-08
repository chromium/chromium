// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_notification/model/content_notification_service.h"
#import "ios/public/provider/chrome/browser/content_notification/content_notification_api.h"
#import "url/gurl.h"

namespace ios {
namespace provider {
namespace {

// Dummy ContentNotificationService implementation used for tests.
class TestContentNotificationService final : public ContentNotificationService {
 public:
  // ContentNotificationService implementation:
  GURL GetDestinationUrl(NSDictionary<NSString*, id>* payload) final {
    return GURL::EmptyGURL();
  }
  NSDictionary<NSString*, NSString*>* GetFeedbackPayload(
      NSDictionary<NSString*, id>* payload) final {
    return nil;
  }
  void SendNAUForConfiguration(
      ContentNotificationNAUConfiguration* configuration) final {
    return;
  }
};

}  // anonymous namespace

std::unique_ptr<ContentNotificationService> CreateContentNotificationService(
    ContentNotificationConfiguration* config) {
  return std::make_unique<TestContentNotificationService>();
}

}  // namespace provider
}  // namespace ios
