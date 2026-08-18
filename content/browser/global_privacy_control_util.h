// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GLOBAL_PRIVACY_CONTROL_UTIL_H_
#define CONTENT_BROWSER_GLOBAL_PRIVACY_CONTROL_UTIL_H_

#include "content/common/content_export.h"

namespace content {

// DevTools uses this to override the profile's GPC setting and require the
// header to be sent or not sent.
CONTENT_EXPORT void UpdateGlobalPrivacyControlDevToolsOverride(bool new_gpc);

// DevTools uses this to reset the DevTools GPC setting override when the
// session disconnects.
CONTENT_EXPORT void ResetGlobalPrivacyControlDevToolsOverride();

// Returns true if the profile setting (or DevTools setting override) is
// enabled, but does not depend on the feature. Use this to determine the
// setting sent down to the renderer only. Do not gate feature access or headers
// using this, use `IsGlobalPrivacyControlFeature(AndSetting)Enabled` instead.
CONTENT_EXPORT bool IsGlobalPrivacyControlSettingEnabled();

}  // namespace content

#endif  // CONTENT_BROWSER_GLOBAL_PRIVACY_CONTROL_UTIL_H_
