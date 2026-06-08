// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COBROWSE_DEBUGGER_AIM_SRP_DEBUGGER_EVENT_H_
#define IOS_CHROME_BROWSER_COBROWSE_DEBUGGER_AIM_SRP_DEBUGGER_EVENT_H_

#import <Foundation/Foundation.h>

namespace lens {
class ClientToAimMessage;
class AimToClientMessage;
}  // namespace lens

typedef NS_ENUM(NSInteger, AimSRPMessageDirection) {
  kClientToAim,
  kAimToClient,
};

// Represents a single communication event to or from the AIM SRP.
@interface AimSRPDebuggerEvent : NSObject

@property(nonatomic, readonly) NSDate* timestamp;
@property(nonatomic, readonly) AimSRPMessageDirection direction;
@property(nonatomic, readonly, copy) NSString* messageName;

- (instancetype)initWithDirection:(AimSRPMessageDirection)direction
               clientToAimMessage:(const lens::ClientToAimMessage&)message;

- (instancetype)initWithDirection:(AimSRPMessageDirection)direction
               aimToClientMessage:(const lens::AimToClientMessage&)message;

@end

#endif  // IOS_CHROME_BROWSER_COBROWSE_DEBUGGER_AIM_SRP_DEBUGGER_EVENT_H_
