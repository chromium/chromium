// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_GUEST_VIEW_FEATURE_UTIL_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_GUEST_VIEW_FEATURE_UTIL_H_

namespace content {
class BrowserContext;
}

namespace extensions {

// Whether MPArch related <webview> behaviour changes are in effect. This
// considers the state of the feature flag and enterprise policy controlling
// this.
bool AreWebviewMPArchBehaviorsEnabled(content::BrowserContext* browser_context);

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_GUEST_VIEW_FEATURE_UTIL_H_
