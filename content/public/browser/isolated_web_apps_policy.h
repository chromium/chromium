// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CONTENT_PUBLIC_BROWSER_ISOLATED_WEB_APPS_POLICY_H_
#define CONTENT_PUBLIC_BROWSER_ISOLATED_WEB_APPS_POLICY_H_

#include "content/common/content_export.h"

namespace content {

class BrowserContext;

// A centralized place for making a decision about Isolated Web Apps (IWAs).
// For more information about IWAs, see:
// https://github.com/WICG/isolated-web-apps/blob/main/README.md

class CONTENT_EXPORT IsolatedWebAppsPolicy {
 public:
  IsolatedWebAppsPolicy(const IsolatedWebAppsPolicy&) = delete;
  IsolatedWebAppsPolicy& operator=(const IsolatedWebAppsPolicy&) = delete;
  IsolatedWebAppsPolicy() = delete;

  // Returns true if Isolated Web Apps are enabled.
  static bool AreIsolatedWebAppsEnabled(BrowserContext* browser_context);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ISOLATED_WEB_APPS_POLICY_H_
