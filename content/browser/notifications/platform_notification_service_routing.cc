// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notifications/platform_notification_service_routing.h"

#include "content/browser/notifications/virtual_platform_notification_service.h"
#include "content/public/browser/browser_context.h"

namespace content {

PlatformNotificationService* GetPlatformNotificationServiceFor(
    BrowserContext* browser_context) {
  if (auto* service = VirtualPlatformNotificationService::FromBrowserContext(
          browser_context)) {
    return service;
  }
  return browser_context->GetPlatformNotificationService();
}

PlatformNotificationService* GetPlatformNotificationServiceForUkmEvent(
    BrowserContext* browser_context,
    const std::string& notification_id) {
  if (VirtualPlatformNotificationService::ConsumeSkippedUkmEvent(
          browser_context, notification_id)) {
    return nullptr;
  }
  return GetPlatformNotificationServiceFor(browser_context);
}

}  // namespace content
