// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_SHELL_APPLICATION_MAC_H_
#define CONTENT_SHELL_BROWSER_SHELL_APPLICATION_MAC_H_

#include "base/mac/scoped_sending_event.h"
#include "base/message_loop/message_pump_apple.h"

@interface ShellCrApplication
    : NSApplication <CrAppProtocol, CrAppControlProtocol>

// CrAppProtocol:
- (BOOL)isHandlingSendEvent;

// CrAppControlProtocol:
- (void)setHandlingSendEvent:(BOOL)handlingSendEvent;

- (IBAction)newDocument:(id)sender;

@end

#endif  // CONTENT_SHELL_BROWSER_SHELL_APPLICATION_MAC_H_
