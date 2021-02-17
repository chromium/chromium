// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_TEST_TEST_BACKGROUND_PAGE_READY_OBSERVER_H_
#define EXTENSIONS_TEST_TEST_BACKGROUND_PAGE_READY_OBSERVER_H_

#include "content/public/test/test_utils.h"
#include "extensions/common/extension_id.h"

namespace content {
class BrowserContext;
class NotificationDetails;
class NotificationSource;
}  // namespace content

namespace extensions {

// Allows to wait until the extension's background page becomes ready.
class ExtensionBackgroundPageReadyObserver final {
 public:
  ExtensionBackgroundPageReadyObserver(
      content::BrowserContext* browser_context,
      const extensions::ExtensionId& extension_id);
  ExtensionBackgroundPageReadyObserver(
      const ExtensionBackgroundPageReadyObserver&) = delete;
  ExtensionBackgroundPageReadyObserver& operator=(
      const ExtensionBackgroundPageReadyObserver&) = delete;
  ~ExtensionBackgroundPageReadyObserver();

  void Wait();

 private:
  // Callback which is used for |notification_observer_| for checking whether
  // the condition being awaited is met.
  bool IsNotificationRelevant(
      const content::NotificationSource& source,
      const content::NotificationDetails& details) const;

  content::BrowserContext* const browser_context_;
  const extensions::ExtensionId extension_id_;
  content::WindowedNotificationObserver notification_observer_;
};

}  // namespace extensions

#endif  // EXTENSIONS_TEST_TEST_BACKGROUND_PAGE_READY_OBSERVER_H_