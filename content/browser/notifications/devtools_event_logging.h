// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NOTIFICATIONS_DEVTOOLS_EVENT_LOGGING_H_
#define CONTENT_BROWSER_NOTIFICATIONS_DEVTOOLS_EVENT_LOGGING_H_

#include <string>

#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/time/time.h"

class GURL;

namespace content {

class BrowserContext;
struct NotificationDatabaseData;

namespace notifications {

bool ShouldLogNotificationEventToDevTools(BrowserContext* browser_context,
                                          const GURL& origin);

void LogNotificationDisplayedEventToDevTools(
    BrowserContext* browser_context,
    const NotificationDatabaseData& data);

void LogNotificationClosedEventToDevTools(BrowserContext* browser_context,
                                          const NotificationDatabaseData& data);

void LogNotificationScheduledEventToDevTools(
    BrowserContext* browser_context,
    const NotificationDatabaseData& data,
    base::Time show_trigger_timestamp);

void LogNotificationClickedEventToDevTools(
    BrowserContext* browser_context,
    const NotificationDatabaseData& data,
    const base::Optional<int>& action_index,
    const base::Optional<base::string16>& reply);

}  // namespace notifications
}  // namespace content

#endif  // CONTENT_BROWSER_NOTIFICATIONS_DEVTOOLS_EVENT_LOGGING_H_
