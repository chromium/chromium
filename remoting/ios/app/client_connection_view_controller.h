// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_APP_CLIENT_CONNECTION_VIEW_CONTROLLER_H_
#define REMOTING_IOS_APP_CLIENT_CONNECTION_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@class HostInfo;

// This enumerated the different modes this Client Connection View can be in.
typedef NS_ENUM(NSInteger, ClientConnectionViewState) {
  ClientViewConnecting,
  ClientViewPinPrompt,
  ClientViewConnected,
  ClientViewReconnect,
  ClientViewClosed,
  ClientViewError,
};

// This is the view that shows the user feedback while the client connection is
// being established. If requested the view can also display the pin entry view.
// State communication for this view is handled by NSNotifications, it listens
// to kHostSessionStatusChanged events on the default NSNotificationCenter.
// Internally the notification is tied to [self setState] so view setup will
// work the same way if state is set directly.
@interface ClientConnectionViewController : UIViewController

- (instancetype)initWithHostInfo:(HostInfo*)hostInfo;

// Setting state will change the view
@property(nonatomic, assign) ClientConnectionViewState state;

@end

#endif  // REMOTING_IOS_APP_CLIENT_CONNECTION_VIEW_CONTROLLER_H_
