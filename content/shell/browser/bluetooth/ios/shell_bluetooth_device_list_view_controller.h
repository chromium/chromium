// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_BLUETOOTH_IOS_SHELL_BLUETOOTH_DEVICE_LIST_VIEW_CONTROLLER_H_
#define CONTENT_SHELL_BROWSER_BLUETOOTH_IOS_SHELL_BLUETOOTH_DEVICE_LIST_VIEW_CONTROLLER_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "content/shell/browser/bluetooth/ios/shell_bluetooth_device_list_consumer.h"

@class DeviceInfo;

@protocol ShellBluetoothDeviceListDelegate;

// The ViewController that has UITableView to show the bluetooth device list.
@interface ShellDeviceListViewController
    : UITableViewController <ShellBluetoothDeviceListConsumer,
                             UIPopoverPresentationControllerDelegate>

// The view controller this coordinator was initialized with.
@property(weak, nonatomic, readonly) UIViewController* baseViewController;

// Has device IDs according to cell index. It's used to get a device ID when a
// cell index is known.
@property(nonatomic, strong) NSMutableArray<NSString*>* deviceIDs;

// Has device information. It's used to get a device information such as a
// device name when a device ID is known.
@property(nonatomic, strong)
    NSMutableDictionary<NSString*, DeviceInfo*>* deviceInfos;

@property(nonatomic, assign) NSInteger selectedRowIndex;

@property(nonatomic, copy) NSString* listTitle;

// Delegate used to handle the device list.
@property(nonatomic, weak) id<ShellBluetoothDeviceListDelegate> listDelegate;

- (instancetype)initWithTitle:(NSString*)title;

- (void)updateSelectedInformation:(bool)selected;
@end
#endif  // CONTENT_SHELL_BROWSER_BLUETOOTH_IOS_SHELL_BLUETOOTH_DEVICE_LIST_VIEW_CONTROLLER_H_
