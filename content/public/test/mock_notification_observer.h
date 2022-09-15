// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_MOCK_NOTIFICATION_OBSERVER_H_
#define CONTENT_PUBLIC_TEST_MOCK_NOTIFICATION_OBSERVER_H_

#include "content/public/browser/notification_observer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

class NotificationDetails;
class NotificationSource;

class MockNotificationObserver : public NotificationObserver {
 public:
  MockNotificationObserver();
  ~MockNotificationObserver() override;

  MOCK_METHOD3(Observe, void(int type,
                             const NotificationSource& source,
                             const NotificationDetails& details));
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_MOCK_NOTIFICATION_OBSERVER_H_
