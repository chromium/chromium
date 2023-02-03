// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/public/browser/isolated_web_apps_policy.h"

#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"

namespace content {
// static
bool IsolatedWebAppsPolicy::AreIsolatedWebAppsEnabled(
    BrowserContext* browser_context) {
  return GetContentClient()->browser()->AreIsolatedWebAppsEnabled(
      browser_context);
}
}  // namespace content