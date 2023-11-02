// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_CLIENT_GESTURES_H_
#define REMOTING_IOS_CLIENT_GESTURES_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

@class RemotingClient;

@protocol ClientGesturesDelegate<NSObject>
- (void)keyboardShouldShow;
- (void)keyboardShouldHide;
- (void)menuShouldShow;
@end

@interface ClientGestures : NSObject

- (instancetype)initWithView:(UIView*)view client:(RemotingClient*)client;

@property(weak, nonatomic) id<ClientGesturesDelegate> delegate;

@end

#endif  //  REMOTING_IOS_CLIENT_GESTURES_H_
