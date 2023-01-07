// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/test/providers/app_distribution/test_app_distribution.h"

#include "ios/public/provider/chrome/browser/app_distribution/app_distribution_api.h"

namespace ios {
namespace provider {
namespace test {
namespace {

// Enumeration used to record the possible state of the app distribution
// notifications.
enum class NotificationsState {
  kUnspecified,
  kScheduled,
  kCanceled,
};

// Global storing the app distribution notifications state.
NotificationsState g_notifications_state = NotificationsState::kUnspecified;

}  // anonymous namespace

bool AreAppDistributionNotificationsScheduled() {
  return g_notifications_state == NotificationsState::kScheduled;
}

bool AreAppDistributionNotificationsCanceled() {
  return g_notifications_state == NotificationsState::kCanceled;
}

void ResetAppDistributionNotificationsState() {
  g_notifications_state = NotificationsState::kUnspecified;
}

}  // namespace test

std::string GetBrandCode() {
  // Test has no brand code.
  return std::string();
}

void ScheduleAppDistributionNotifications(
    const scoped_refptr<network::SharedURLLoaderFactory>& url_loader_factory,
    bool is_first_run) {
  // Record that the app distribution notifications have been scheduled.
  test::g_notifications_state = test::NotificationsState::kScheduled;
}

void CancelAppDistributionNotifications() {
  // Record that the app distribution notifications have been canceled.
  test::g_notifications_state = test::NotificationsState::kCanceled;
}

void InitializeFirebase(base::Time install_date, bool is_first_run) {
  // Nothing to do for tests.
}

}  // namespace provider
}  // namespace ios
