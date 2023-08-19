// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/shell/browser/bluetooth/ios/shell_bluetooth_chooser_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "content/shell/browser/bluetooth/ios/shell_bluetooth_chooser_ios.h"
#import "content/shell/browser/bluetooth/ios/shell_bluetooth_device_list_consumer.h"

@implementation ShellBluetoothChooserMediator
- (instancetype)initWithBluetoothChooser:
    (content::ShellBluetoothChooserIOS*)bluetoothChooser {
  if (!(self = [super init])) {
    return nil;
  }

  _bluetoothChooser = bluetoothChooser;
  return self;
}

- (void)addOrUpdateDevice:(NSString*)deviceID
         shouldUpdateName:(BOOL)shouldUpdateName
               deviceName:(NSString*)deviceName
            gattConnected:(BOOL)gattConnected
      signalStrengthLevel:(NSInteger)signalStrengthLevel {
  [self.consumer addOrUpdateDevice:deviceID
                  shouldUpdateName:shouldUpdateName
                        deviceName:deviceName
                     gattConnected:gattConnected
               signalStrengthLevel:signalStrengthLevel];
}

#pragma mark - ShellBluetoothDeviceListDelegate

- (void)updateSelectedInfo:(BOOL)selected selectedText:(NSString*)selectedText {
  std::string device_id = base::SysNSStringToUTF8(selectedText);
  content::ShellBluetoothChooserIOS::DialogClosedState state =
      selected ? content::ShellBluetoothChooserIOS::DialogClosedState::
                     kDialogItemSelected
               : content::ShellBluetoothChooserIOS::DialogClosedState::
                     kDialogCanceled;
  self.bluetoothChooser->OnDialogFinished(state, device_id);
}

@end
