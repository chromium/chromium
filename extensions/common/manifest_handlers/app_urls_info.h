// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLERS_APP_URLS_INFO_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLERS_APP_URLS_INFO_H_

#include "extensions/common/extension.h"

namespace extensions {

class Extension;

// TODO(crbug.com/324534603): Convert ParseAppURLs() into a
// AppURLsInfo struct ManifestData.
bool ParseAppURLs(Extension& extension, std::u16string* error);

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLERS_APP_URLS_INFO_H_
