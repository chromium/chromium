// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RENDERER_PREFERENCES_UTIL_H_
#define CONTENT_PUBLIC_BROWSER_RENDERER_PREFERENCES_UTIL_H_

#include "content/common/content_export.h"

namespace blink {
struct RendererPreferences;
}  // namespace blink

namespace content {

// Updates |prefs| from system settings.
CONTENT_EXPORT void UpdateFontRendererPreferencesFromSystemSettings(
    blink::RendererPreferences* prefs);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RENDERER_PREFERENCES_UTIL_H_
