// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_KIOSK_KIOSK_DELEGATE_H_
#define EXTENSIONS_BROWSER_KIOSK_KIOSK_DELEGATE_H_

#include "extensions/common/extension_id.h"

namespace extensions {

// Delegate to provide various Kiosk mode functionality. At some point, we'll
// have the KioskChromeAppManager outside of Chrome. We can then directly use it
// as a delegate but till then, this class is mostly a wrapper to it. Note:
// Kiosk mode is not supported on other platforms but this delegate needs to
// exist since on AppShell, KioskMode will exist on multiple platforms.
class KioskDelegate {
 public:
  KioskDelegate() = default;
  virtual ~KioskDelegate() = default;

  virtual bool IsAutoLaunchedKioskApp(const ExtensionId& id) const = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_KIOSK_KIOSK_DELEGATE_H_
