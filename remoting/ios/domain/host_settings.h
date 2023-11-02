// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_DOMAIN_HOST_SETTINGS_H_
#define REMOTING_IOS_DOMAIN_HOST_SETTINGS_H_

#import <Foundation/Foundation.h>

typedef NS_ENUM(NSInteger, ClientInputMode) {
  ClientInputModeUndefined,
  ClientInputModeDirect,
  ClientInputModeTrackpad,
};

// A detail record for a Remoting Settings.
@interface HostSettings : NSObject<NSCoding>

// Various properties of the Remoting Settings.
@property(nonatomic, copy) NSString* hostId;
@property(nonatomic) ClientInputMode inputMode;
@property(nonatomic) BOOL shouldResizeHostToFit;

@end

#endif  //  REMOTING_IOS_DOMAIN_HOST_SETTINGS_H_
