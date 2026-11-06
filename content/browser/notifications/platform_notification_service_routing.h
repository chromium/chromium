// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NOTIFICATIONS_PLATFORM_NOTIFICATION_SERVICE_ROUTING_H_
#define CONTENT_BROWSER_NOTIFICATIONS_PLATFORM_NOTIFICATION_SERVICE_ROUTING_H_

namespace content {

class BrowserContext;
class PlatformNotificationService;

// Returns the notification service that notifications of `browser_context`
// should be routed to. May return null.
PlatformNotificationService* GetPlatformNotificationServiceFor(
    BrowserContext* browser_context);

}  // namespace content

#endif  // CONTENT_BROWSER_NOTIFICATIONS_PLATFORM_NOTIFICATION_SERVICE_ROUTING_H_
