// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/debugger/aim_srp_debugger_event.h"

#import "base/strings/sys_string_conversions.h"
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
    NSMutableArray<NSString*>* capabilities = [NSMutableArray array];
    for (int cap : message.handshake_ping().capabilities()) {
      [capabilities
          addObject:base::SysUTF8ToNSString(lens::FeatureCapability_Name(
                        static_cast<lens::FeatureCapability>(cap)))];
    }
    _messagePayload = [NSString
        stringWithFormat:@"Capabilities:\n  • %@",
                         [capabilities componentsJoinedByString:@"\n  • "]];
  } else if (message.has_submit_query()) {
    _messageName = @"SubmitQuery";
    const auto& payload = message.submit_query().payload();
    int addedInputsCount = 0;
    if (payload.has_added_inputs()) {
      addedInputsCount = payload.added_inputs().added_inputs_size();
    }
    _messagePayload = [NSString
        stringWithFormat:@"Query Text: %@\nAdded inputs count: %d",
                         base::SysUTF8ToNSString(payload.query_text()),
                         addedInputsCount];
  } else if (message.has_open_threads_view()) {
    _messageName = @"OpenThreadsView";
  } else if (message.has_set_cobrowsing_display_mode()) {
    _messageName = @"SetCobrowsingDisplayMode";
    _messagePayload = [NSString
        stringWithFormat:@"Display Mode: %d",
                         static_cast<int>(message.set_cobrowsing_display_mode()
                                              .params()
                                              .display_mode())];
  } else if (message.has_injected_input_update()) {
    _messageName = @"InjectedInputUpdate";
    const auto& payload = message.injected_input_update().payload();
    _messagePayload =
        [NSString stringWithFormat:@"ID: %@, Type: %d",
                                   base::SysUTF8ToNSString(payload.id()),
                                   static_cast<int>(payload.update_type())];
  } else {
    _messageName = @"Unknown ClientToAimMessage";
  }
}

- (void)parseAimToClientMessage:(const lens::AimToClientMessage&)message {
  if (message.has_handshake_response()) {
    _messageName = @"HandshakeResponse";
    NSMutableArray<NSString*>* capabilities = [NSMutableArray array];
    for (int cap : message.handshake_response().capabilities()) {
      [capabilities
          addObject:base::SysUTF8ToNSString(lens::FeatureCapability_Name(
                        static_cast<lens::FeatureCapability>(cap)))];
    }
    _messagePayload = [NSString
        stringWithFormat:@"Capabilities:\n  • %@",
                         [capabilities componentsJoinedByString:@"\n  • "]];
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
    _messagePayload =
        [NSString stringWithFormat:@"Contexts size: %d",
                                   message.update_thread_context_library()
                                       .contexts_size()];
  } else if (message.has_notify_zero_state_rendered()) {
    _messageName = @"NotifyZeroStateRendered";
    _messagePayload = [NSString
        stringWithFormat:@"Rendered: %@", message.notify_zero_state_rendered()
                                                  .is_zero_state_rendered()
                                              ? @"YES"
                                              : @"NO"];
  } else if (message.has_set_chrome_desktop_input_plate_configuration()) {
    _messageName = @"SetChromeDesktopInputPlateConfiguration";
    const auto& config = message.set_chrome_desktop_input_plate_configuration();
    _messagePayload = [NSString
        stringWithFormat:@"max_width: %d, max_height: %d, margin_bottom: %d, "
                         @"margin_left: %d",
                         config.max_width(), config.max_height(),
                         config.margin_bottom(), config.margin_left()];
  } else if (message.has_inject_input()) {
    _messageName = @"InjectInput";
    _messagePayload = [NSString
        stringWithFormat:@"Query text: %@, expand: %@, submit: %@",
                         base::SysUTF8ToNSString(
                             message.inject_input().query_text()),
                         message.inject_input().expand() ? @"YES" : @"NO",
                         message.inject_input().submit_after_injection()
                             ? @"YES"
                             : @"NO"];
  } else if (message.has_remove_injected_input()) {
    _messageName = @"RemoveInjectedInput";
    _messagePayload = [NSString
        stringWithFormat:@"ID: %@", base::SysUTF8ToNSString(
                                        message.remove_injected_input().id())];
  } else if (message.has_unlock_input()) {
    _messageName = @"UnlockInput";
  } else if (message.has_lock_input()) {
    _messageName = @"LockInput";
  } else if (message.has_open_link_in_side_panel_mode()) {
    _messageName = @"OpenLinkInSidePanelMode";
    _messagePayload = [NSString
        stringWithFormat:@"URL: %@",
                         base::SysUTF8ToNSString(
                             message.open_link_in_side_panel_mode().url())];
  } else {
    _messageName = @"Unknown AimToClientMessage";
  }
}

@end
