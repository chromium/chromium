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

// Returns true if Isolated Web Apps are enabled.
CONTENT_EXPORT bool AreIsolatedWebAppsEnabled(BrowserContext* browser_context);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ISOLATED_WEB_APPS_POLICY_H_
