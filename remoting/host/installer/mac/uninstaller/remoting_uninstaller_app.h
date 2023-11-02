// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_INSTALLER_MAC_UNINSTALLER_REMOTING_UNINSTALLER_APP_H_
#define REMOTING_HOST_INSTALLER_MAC_UNINSTALLER_REMOTING_UNINSTALLER_APP_H_

#import <Cocoa/Cocoa.h>

@interface RemotingUninstallerAppDelegate : NSObject {
}

- (IBAction)uninstall:(id)sender;
- (IBAction)cancel:(id)sender;

- (IBAction)handleMenuClose:(NSMenuItem*)sender;
@end

#endif  // REMOTING_HOST_INSTALLER_MAC_UNINSTALLER_REMOTING_UNINSTALLER_APP_H_