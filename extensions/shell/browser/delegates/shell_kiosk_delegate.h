// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_DELEGATES_SHELL_KIOSK_DELEGATE_H_
#define EXTENSIONS_SHELL_BROWSER_DELEGATES_SHELL_KIOSK_DELEGATE_H_

#include "extensions/browser/kiosk/kiosk_delegate.h"
#include "extensions/common/extension_id.h"

namespace extensions {

// Delegate in AppShell that provides an extension/app API with Kiosk mode
// functionality.
class ShellKioskDelegate : public KioskDelegate {
 public:
  ShellKioskDelegate() = default;
  ~ShellKioskDelegate() override = default;

  // KioskDelegate overrides:
  bool IsAutoLaunchedKioskApp(const ExtensionId& id) const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_DELEGATES_SHELL_KIOSK_DELEGATE_H_
