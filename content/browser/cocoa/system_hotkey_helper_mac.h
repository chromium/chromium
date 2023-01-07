// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COCOA_SYSTEM_HOTKEY_HELPER_MAC_H_
#define CONTENT_BROWSER_COCOA_SYSTEM_HOTKEY_HELPER_MAC_H_

namespace content {

class SystemHotkeyMap;

// Guaranteed to not be NULL.
SystemHotkeyMap* GetSystemHotkeyMap();

}  // namespace content

#endif  // CONTENT_BROWSER_COCOA_SYSTEM_HOTKEY_HELPER_MAC_H_
