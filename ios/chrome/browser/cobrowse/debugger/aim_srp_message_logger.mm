// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/debugger/aim_srp_message_logger.h"

#import "ios/chrome/browser/cobrowse/debugger/aim_srp_debugger_event.h"

@implementation AimSRPMessageLogger {
  NSMutableArray<AimSRPDebuggerEvent*>* _events;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _events = [NSMutableArray array];
  }
  return self;
}

- (NSArray<AimSRPDebuggerEvent*>*)events {
  return [_events copy];
}

- (void)logClientToAimMessage:(const lens::ClientToAimMessage&)message {
  AimSRPDebuggerEvent* event =
      [[AimSRPDebuggerEvent alloc] initWithDirection:kClientToAim
                                  clientToAimMessage:message];
  [_events addObject:event];
}

- (void)logAimToClientMessage:(const lens::AimToClientMessage&)message {
  AimSRPDebuggerEvent* event =
      [[AimSRPDebuggerEvent alloc] initWithDirection:kAimToClient
                                  aimToClientMessage:message];
  [_events addObject:event];
}

- (void)clearEvents {
  [_events removeAllObjects];
}

@end
