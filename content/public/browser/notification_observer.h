// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NOTIFICATION_OBSERVER_H_
#define CONTENT_PUBLIC_BROWSER_NOTIFICATION_OBSERVER_H_

#include "content/common/content_export.h"

namespace content {

class NotificationDetails;
class NotificationSource;

// This is the base class for notification observers. When a matching
// notification is posted to the notification service, Observe is called.
class CONTENT_EXPORT NotificationObserver {
 public:
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) = 0;

 protected:
  virtual ~NotificationObserver() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NOTIFICATION_OBSERVER_H_
