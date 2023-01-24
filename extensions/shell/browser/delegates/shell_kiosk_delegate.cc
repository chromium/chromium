// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/delegates/shell_kiosk_delegate.h"

namespace extensions {

bool ShellKioskDelegate::IsAutoLaunchedKioskApp(const ExtensionId& id) const {
  // Every app in AppShell is auto-launched and AppShell only runs in
  // kiosk mode.
  return true;
}

}  // namespace extensions
