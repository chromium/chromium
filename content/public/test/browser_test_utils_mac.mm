// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/browser_test_utils.h"

#import <AppKit/AppKit.h>

namespace content {

bool EnableNativeWindowActivation() {
  // Do not downgrade the activation policy.
  if (NSApp.activationPolicy > NSApplicationActivationPolicyProhibited) {
    return true;
  }

  // NSApplicationActivationPolicyAccessory is the least permissive policy that
  // still allows for programmatic activation.
  return [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory]
             ? true
             : false;
}

bool MacOSVersionSupportsDarkMode() {
  if (@available(macOS 10.14, *)) {
    return true;
  }
  return false;
}

}  // namespace content
