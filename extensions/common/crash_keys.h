// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_CRASH_KEYS_H_
#define EXTENSIONS_COMMON_CRASH_KEYS_H_

#include <set>

#include "extensions/common/extension_id.h"

namespace extensions::crash_keys {

// Sets the list of "active" extensions in this process. We overload "active" to
// mean different things depending on the process type:
// - browser: all enabled extensions
// - renderer: the unique set of extension ids from all content scripts
// - extension: the id of each extension running in this process (there can be
//   multiple because of process collapsing).
void SetActiveExtensions(const std::set<ExtensionId>& extensions);

}  // namespace extensions::crash_keys

#endif  // EXTENSIONS_COMMON_CRASH_KEYS_H_
