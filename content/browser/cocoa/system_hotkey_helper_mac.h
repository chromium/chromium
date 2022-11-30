// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COCOA_SYSTEM_HOTKEY_HELPER_MAC_H_
#define CONTENT_BROWSER_COCOA_SYSTEM_HOTKEY_HELPER_MAC_H_

#include "base/callback.h"
#include "base/files/file_path.h"
#include "content/common/content_export.h"

namespace content {

class SystemHotkeyMap;

// Guaranteed to not be NULL.
CONTENT_EXPORT SystemHotkeyMap* GetSystemHotkeyMap();

CONTENT_EXPORT void SetSystemHotkeyPlistPathForTesting(
    base::FilePath& file_path);
CONTENT_EXPORT void WaitForEventsForTesting();

}  // namespace content

#endif  // CONTENT_BROWSER_COCOA_SYSTEM_HOTKEY_HELPER_MAC_H_
