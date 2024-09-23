// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_browser_main_parts.h"

#include "services/device/public/cpp/geolocation/system_geolocation_source_apple.h"

namespace content {

device::GeolocationSystemPermissionManager*
ShellBrowserMainParts::GetGeolocationSystemPermissionManager() {
  if (!device::GeolocationSystemPermissionManager::GetInstance()) {
    device::GeolocationSystemPermissionManager::SetInstance(
        device::SystemGeolocationSourceApple::
            CreateGeolocationSystemPermissionManager());
  }
  return device::GeolocationSystemPermissionManager::GetInstance();
}

}  // namespace content
