// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_APP_SETTINGS_REMOTING_SETTINGS_VIEW_CONTROLLER_H_
#define REMOTING_IOS_APP_SETTINGS_REMOTING_SETTINGS_VIEW_CONTROLLER_H_

#import <MaterialComponents/MaterialCollections.h>

#include "remoting/client/gesture_interpreter.h"

@protocol RemotingSettingsViewControllerDelegate<NSObject>

@optional  // Applies to all methods.

// Sets the setting to resize the host to fix the client window.
- (void)setResizeToFit:(BOOL)resizeToFit;
// Use the direct input mode for the current connection.
- (void)useDirectInputMode;
// Use the trackpad input mode for the current connection.
- (void)useTrackpadInputMode;
// Sends the key combo ctrl + alt + del to the host.
- (void)sendCtrAltDel;
// Sends the key Print Screen to the host.
- (void)sendPrintScreen;
// Sends feedback to the developers.
- (void)sendFeedback;

@end

// |RemotingSettingsViewController| is intended to be reinitialized before it
// is displayed. It only synchronizes to the delegate.
// TODO(nicholss): Perhaps we should remove the internal settings storage and
// get the state from the delegate directly. It will remove the sync issue.
@interface RemotingSettingsViewController : MDCCollectionViewController

@property(weak, nonatomic) id<RemotingSettingsViewControllerDelegate> delegate;
@property(nonatomic) remoting::GestureInterpreter::InputMode inputMode;
@property(nonatomic) BOOL shouldResizeHostToFit;

@end

#endif  // REMOTING_IOS_APP_SETTINGS_REMOTING_SETTINGS_VIEW_CONTROLLER_H_
