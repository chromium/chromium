// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_BLUETOOTH_IOS_SHELL_BLUETOOTH_CHOOSER_MEDIATOR_H_
#define CONTENT_SHELL_BROWSER_BLUETOOTH_IOS_SHELL_BLUETOOTH_CHOOSER_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "content/shell/browser/bluetooth/ios/shell_bluetooth_device_list_delegate.h"

namespace content {
class ShellBluetoothChooserIOS;
}  // namespace content

@protocol ShellBluetoothDeviceListConsumer;

@interface ShellBluetoothChooserMediator
    : NSObject <ShellBluetoothDeviceListDelegate>

@property(nonatomic, assign)
    content::ShellBluetoothChooserIOS* bluetoothChooser;

// Consumer that is configured by this mediator.
@property(nonatomic, assign) id<ShellBluetoothDeviceListConsumer> consumer;

- (instancetype)initWithBluetoothChooser:
    (content::ShellBluetoothChooserIOS*)bluetoothChooser;

- (void)setConsumer:(id<ShellBluetoothDeviceListConsumer>)consumer;

- (void)addOrUpdateDevice:(NSString*)deviceID
         shouldUpdateName:(BOOL)shouldUpdateName
               deviceName:(NSString*)deviceName
            gattConnected:(BOOL)gattConnected
      signalStrengthLevel:(NSInteger)signalStrengthLevel;
@end

#endif  // CONTENT_SHELL_BROWSER_BLUETOOTH_IOS_SHELL_BLUETOOTH_CHOOSER_MEDIATOR_H_
