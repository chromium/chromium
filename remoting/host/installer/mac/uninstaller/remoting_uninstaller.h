// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_INSTALLER_MAC_UNINSTALLER_REMOTING_UNINSTALLER_H_
#define REMOTING_HOST_INSTALLER_MAC_UNINSTALLER_REMOTING_UNINSTALLER_H_

#import <Cocoa/Cocoa.h>

@interface RemotingUninstaller : NSObject {
}

- (OSStatus)remotingUninstall;

@end

#endif  // REMOTING_HOST_INSTALLER_MAC_UNINSTALLER_REMOTING_UNINSTALLER_H_