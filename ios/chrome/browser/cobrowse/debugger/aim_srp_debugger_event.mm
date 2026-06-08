// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/debugger/aim_srp_debugger_event.h"

#import "third_party/lens_server_proto/aim_communication.pb.h"

@implementation AimSRPDebuggerEvent

- (instancetype)initWithDirection:(AimSRPMessageDirection)direction
               clientToAimMessage:(const lens::ClientToAimMessage&)message {
  self = [super init];
  if (self) {
    _timestamp = [NSDate date];
    _direction = direction;
    [self parseClientToAimMessage:message];
  }
  return self;
}

- (instancetype)initWithDirection:(AimSRPMessageDirection)direction
               aimToClientMessage:(const lens::AimToClientMessage&)message {
  self = [super init];
  if (self) {
    _timestamp = [NSDate date];
    _direction = direction;
    [self parseAimToClientMessage:message];
  }
  return self;
}

- (void)parseClientToAimMessage:(const lens::ClientToAimMessage&)message {
  if (message.has_handshake_ping()) {
    _messageName = @"HandshakePing";
  } else if (message.has_submit_query()) {
    _messageName = @"SubmitQuery";
  } else if (message.has_open_threads_view()) {
    _messageName = @"OpenThreadsView";
  } else if (message.has_set_cobrowsing_display_mode()) {
    _messageName = @"SetCobrowsingDisplayMode";
  } else if (message.has_injected_input_update()) {
    _messageName = @"InjectedInputUpdate";
  } else {
    _messageName = @"Unknown ClientToAimMessage";
  }
}

- (void)parseAimToClientMessage:(const lens::AimToClientMessage&)message {
  if (message.has_handshake_response()) {
    _messageName = @"HandshakeResponse";
  } else if (message.has_hide_input()) {
    _messageName = @"HideInput";
  } else if (message.has_restore_input()) {
    _messageName = @"RestoreInput";
  } else if (message.has_enter_basic_mode()) {
    _messageName = @"EnterBasicMode";
  } else if (message.has_exit_basic_mode()) {
    _messageName = @"ExitBasicMode";
  } else if (message.has_update_thread_context_library()) {
    _messageName = @"UpdateThreadContextLibrary";
  } else if (message.has_notify_zero_state_rendered()) {
    _messageName = @"NotifyZeroStateRendered";
  } else if (message.has_set_chrome_desktop_input_plate_configuration()) {
    _messageName = @"SetChromeDesktopInputPlateConfiguration";
  } else if (message.has_inject_input()) {
    _messageName = @"InjectInput";
  } else if (message.has_remove_injected_input()) {
    _messageName = @"RemoveInjectedInput";
  } else if (message.has_unlock_input()) {
    _messageName = @"UnlockInput";
  } else if (message.has_lock_input()) {
    _messageName = @"LockInput";
  } else if (message.has_open_link_in_side_panel_mode()) {
    _messageName = @"OpenLinkInSidePanelMode";
  } else {
    _messageName = @"Unknown AimToClientMessage";
  }
}

@end
