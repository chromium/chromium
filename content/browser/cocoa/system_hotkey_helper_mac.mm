// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cocoa/system_hotkey_helper_mac.h"

#include "base/apple/foundation_util.h"
#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "content/browser/cocoa/system_hotkey_map.h"

namespace {

constexpr auto* kSystemHotkeyPlistPath =
    "Preferences/com.apple.symbolichotkeys.plist";

content::SystemHotkeyMap LoadSystemHotkeyMap() {
  auto* hotkey_plist_url = base::apple::FilePathToNSURL(
      base::apple::GetUserLibraryPath().Append(kSystemHotkeyPlistPath));
  NSDictionary* dictionary =
      [NSDictionary dictionaryWithContentsOfURL:hotkey_plist_url error:nil];

  content::SystemHotkeyMap map;
  map.ParseDictionary(dictionary);
  return map;
}

}  // namespace

namespace content {

// static
SystemHotkeyMap* GetSystemHotkeyMap() {
  static base::NoDestructor<SystemHotkeyMap> instance(LoadSystemHotkeyMap());
  return instance.get();
}

}  // namespace content
