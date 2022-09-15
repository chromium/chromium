// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/mac/system_services.h"

#include <Carbon/Carbon.h>
#include <CoreFoundation/CoreFoundation.h>

#include "base/mac/mac_logging.h"

extern "C" {
OSStatus SetApplicationIsDaemon(Boolean isDaemon);
void _LSSetApplicationLaunchServicesServerConnectionStatus(
    uint64_t flags,
    bool (^connection_allowed)(CFDictionaryRef options));

// See
// https://github.com/WebKit/webkit/commit/8da694b0b3febcc262653d01a45e946ce91845ed.
void _CSCheckFixDisable() API_AVAILABLE(macosx(10.15));
}  // extern "C"

namespace sandbox {

void DisableLaunchServices() {
  // Allow the process to continue without a LaunchServices ASN. The
  // INIT_Process function in HIServices will abort if it cannot connect to
  // launchservicesd to get an ASN. By setting this flag, HIServices skips
  // that.
  OSStatus status = SetApplicationIsDaemon(true);
  OSSTATUS_LOG_IF(ERROR, status != noErr, status) << "SetApplicationIsDaemon";

  // Close any connections to launchservicesd and use an always-false predicate
  // to discourage future attempts to connect.
  _LSSetApplicationLaunchServicesServerConnectionStatus(
      0, ^bool(CFDictionaryRef options) {
        return false;
      });
}

void DisableCoreServicesCheckFix() {
  if (__builtin_available(macOS 10.15, *)) {
    _CSCheckFixDisable();
  }
}

}  // namespace sandbox
