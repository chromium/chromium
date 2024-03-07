// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_MODEL_IOS_PUSH_NOTIFICATIONS_METRICS_PROVIDER_H_
#define IOS_CHROME_BROWSER_METRICS_MODEL_IOS_PUSH_NOTIFICATIONS_METRICS_PROVIDER_H_

#import "base/memory/raw_ptr.h"
#include "components/metrics/metrics_provider.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"

namespace signin {
class IdentityManager;
}

class IOSPushNotificationsMetricsProvider : public metrics::MetricsProvider {
 public:
  explicit IOSPushNotificationsMetricsProvider(
      signin::IdentityManager* identity_manager);
  IOSPushNotificationsMetricsProvider(
      const IOSPushNotificationsMetricsProvider&) = delete;
  IOSPushNotificationsMetricsProvider& operator=(
      const IOSPushNotificationsMetricsProvider&) = delete;

  ~IOSPushNotificationsMetricsProvider() override;

  // metrics::MetricsProvider
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;

 private:
  void ReportEnabledClientID(std::string histogram_name,
                             PushNotificationClientId client_id);

  raw_ptr<signin::IdentityManager> identity_manager_;
};

#endif  // IOS_CHROME_BROWSER_METRICS_MODEL_IOS_PUSH_NOTIFICATIONS_METRICS_PROVIDER_H_
