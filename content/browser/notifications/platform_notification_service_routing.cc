// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notifications/platform_notification_service_routing.h"

#include "content/public/browser/browser_context.h"

namespace content {

PlatformNotificationService* GetPlatformNotificationServiceFor(
    BrowserContext* browser_context) {
  return browser_context->GetPlatformNotificationService();
}

}  // namespace content
