// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_SHELL_APPLICATION_MAC_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_SHELL_APPLICATION_MAC_H_

#include "base/mac/scoped_sending_event.h"
#include "base/message_loop/message_pump_apple.h"

// Headless shell uses |MessagePumpMac|, so it needs to implement the
// |CRAppProtocol|.
@interface HeadlessShellCrApplication
    : NSApplication<CrAppProtocol, CrAppControlProtocol>

// CrAppProtocol:
- (BOOL)isHandlingSendEvent;

// CrAppControlProtocol:
- (void)setHandlingSendEvent:(BOOL)handlingSendEvent;

@end

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_SHELL_APPLICATION_MAC_H_
