// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_PREFERENCES_UTIL_H_
#define CONTENT_BROWSER_RENDERER_PREFERENCES_UTIL_H_

#include "content/common/content_export.h"

namespace blink {
struct RendererPreferences;
}  // namespace blink

namespace content {

class BrowserContext;

// Invokes ContentBrowserClient::UpdateRendererPreferencesForWorker and then
// applies content-specific overrides as needed.
CONTENT_EXPORT void UpdateRendererPreferencesForWorkerHelper(
    BrowserContext* context,
    blink::RendererPreferences* preferences);

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_PREFERENCES_UTIL_H_
