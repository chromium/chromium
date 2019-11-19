// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ORIGIN_POLICY_COMMANDS_H_
#define CONTENT_PUBLIC_BROWSER_ORIGIN_POLICY_COMMANDS_H_

#include "content/common/content_export.h"

class GURL;

namespace content {

class BrowserContext;

// Instruct the Origin Policy throttle to disregard errors for the given URL.
//
// Intended use: This should be called by the browser when the user selects
// "proceed" on the security interstitial page for the given URL.
CONTENT_EXPORT void OriginPolicyAddExceptionFor(BrowserContext* browser_context,
                                                const GURL& url);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ORIGIN_POLICY_COMMANDS_H_
