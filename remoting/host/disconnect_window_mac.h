// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DISCONNECT_WINDOW_MAC_H_
#define REMOTING_HOST_DISCONNECT_WINDOW_MAC_H_

#import <Cocoa/Cocoa.h>

#include <string>

#include "base/functional/callback.h"

// Controller for the disconnect window which allows the host user to
// quickly disconnect a session.
@interface DisconnectWindowController : NSWindowController

- (instancetype)initWithCallback:(base::OnceClosure)disconnect_callback
                        username:(const std::string&)username
                          window:(NSWindow*)window;
- (void)initializeWindow;
- (void)stopSharing:(id)sender;
@end

// A floating window with a custom border. The custom border and background
// content is defined by DisconnectView. Declared here so that it can be
// instantiated via a xib.
@interface DisconnectWindow : NSWindow
@end

// The custom background/border for the DisconnectWindow. Declared here so that
// it can be instantiated via a xib.
@interface DisconnectView : NSView
@end

#endif  // REMOTING_HOST_DISCONNECT_WINDOW_MAC_H_
