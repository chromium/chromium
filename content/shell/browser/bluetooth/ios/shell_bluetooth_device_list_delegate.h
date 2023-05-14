// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_BLUETOOTH_IOS_SHELL_BLUETOOTH_DEVICE_LIST_DELEGATE_H_
#define CONTENT_SHELL_BROWSER_BLUETOOTH_IOS_SHELL_BLUETOOTH_DEVICE_LIST_DELEGATE_H_

#import <Foundation/Foundation.h>

// Delegate to handle actions.
@protocol ShellBluetoothDeviceListDelegate <NSObject>

// Method invoked when the user taps a item from the list.
- (void)updateSelectedInfo:(BOOL)selected selectedText:(NSString*)selectedText;

@end

#endif  // CONTENT_SHELL_BROWSER_BLUETOOTH_IOS_SHELL_BLUETOOTH_DEVICE_LIST_DELEGATE_H_
