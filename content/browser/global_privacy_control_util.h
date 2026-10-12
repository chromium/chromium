// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GLOBAL_PRIVACY_CONTROL_UTIL_H_
#define CONTENT_BROWSER_GLOBAL_PRIVACY_CONTROL_UTIL_H_

#include <optional>

#include "content/common/content_export.h"

namespace content {

// DevTools uses this to override the profile's GPC setting and require the
// header to be sent or not sent.
CONTENT_EXPORT void UpdateGlobalPrivacyControlDevToolsOverride(bool new_gpc);

// DevTools uses this to reset the DevTools GPC setting override when the
// session disconnects.
CONTENT_EXPORT void ResetGlobalPrivacyControlDevToolsOverride();

// Returns the DevTools GPC setting override, if one is set. When set, it takes
// precedence over the embedder's profile setting; when unset (std::nullopt),
// callers should fall back to the embedder's setting. This does not depend on
// the feature. Use this to determine the setting sent down to the renderer
// only. Do not gate feature access or headers using this, use
// `IsGlobalPrivacyControlFeature(AndSetting)Enabled` instead.
CONTENT_EXPORT std::optional<bool> GetGlobalPrivacyControlDevToolsOverride();

}  // namespace content

#endif  // CONTENT_BROWSER_GLOBAL_PRIVACY_CONTROL_UTIL_H_
