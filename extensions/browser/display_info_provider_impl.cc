// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/system_display/display_info_provider.h"

namespace extensions {

// static
DisplayInfoProvider* DisplayInfoProvider::Get() {
  if (!g_display_info_provider) {
    // Let the DisplayInfoProvider leak.
    g_display_info_provider =
        ExtensionsAPIClient::Get()->CreateDisplayInfoProvider().release();
  }
  return g_display_info_provider;
}

}  // namespace extensions
