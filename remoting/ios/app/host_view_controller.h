// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_APP_HOST_VIEW_CONTROLLER_H_
#define REMOTING_IOS_APP_HOST_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#include "remoting/ios/display/gl_display_handler.h"

@class RemotingClient;

// We don't inherit it from GLKViewController since it uses its rendering loop,
// which will swap buffers when the GLRenderer is writing and causes screen
// tearing issues. Instead we use GlDisplayHandler to handle the rendering loop.
@interface HostViewController : UIViewController

- (id)initWithClient:(RemotingClient*)client;

@end

#endif  // REMOTING_IOS_APP_HOST_VIEW_CONTROLLER_H_
