// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COBROWSE_DEBUGGER_AIM_SRP_MESSAGE_LOGGER_H_
#define IOS_CHROME_BROWSER_COBROWSE_DEBUGGER_AIM_SRP_MESSAGE_LOGGER_H_

#import <Foundation/Foundation.h>

@class AimSRPDebuggerEvent;

namespace lens {
class ClientToAimMessage;
class AimToClientMessage;
}  // namespace lens

// A log storage class that manages in-memory AIM SRP communication events.
@interface AimSRPMessageLogger : NSObject

@property(nonatomic, readonly) NSArray<AimSRPDebuggerEvent*>* events;

- (void)logClientToAimMessage:(const lens::ClientToAimMessage&)message;
- (void)logAimToClientMessage:(const lens::AimToClientMessage&)message;
- (void)clearEvents;

@end

#endif  // IOS_CHROME_BROWSER_COBROWSE_DEBUGGER_AIM_SRP_MESSAGE_LOGGER_H_
