// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/shell/browser/bluetooth/ios/shell_bluetooth_chooser_coordinator.h"

#import "base/strings/sys_string_conversions.h"
#import "content/shell/browser/bluetooth/ios/shell_bluetooth_chooser_mediator.h"
#import "content/shell/browser/bluetooth/ios/shell_bluetooth_device_list_view_controller.h"
#import "ui/gfx/native_widget_types.h"

@implementation ShellBluetoothChooserCoordinator
- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                     title:(NSString*)title
                          bluetoothChooser:(content::ShellBluetoothChooserIOS*)
                                               bluetoothChooser {
  if (!(self = [super init])) {
    return nil;
  }

  _deviceListViewController =
      [[ShellDeviceListViewController alloc] initWithTitle:title];
  _deviceListViewController.modalPresentationStyle = UIModalPresentationPopover;
  _deviceListViewController.popoverPresentationController.delegate =
      _deviceListViewController;
  _deviceListViewController.popoverPresentationController.sourceView =
      baseViewController.view;
  _deviceListViewController.popoverPresentationController.sourceRect =
      baseViewController.view.bounds;

  _bluetoothChooserMediator = [[ShellBluetoothChooserMediator alloc]
      initWithBluetoothChooser:bluetoothChooser];
  _deviceListViewController.listDelegate = _bluetoothChooserMediator;
  _bluetoothChooserMediator.consumer = _deviceListViewController;
  [baseViewController presentViewController:_deviceListViewController
                                   animated:true
                                 completion:nil];
  return self;
}

namespace content {

ShellBluetoothChooserCoordinatorHolder::ShellBluetoothChooserCoordinatorHolder(
    gfx::NativeWindow native_window,
    content::ShellBluetoothChooserIOS* bluetooth_chooser_ios,
    const std::u16string& title) {
  coordinator_ = [[ShellBluetoothChooserCoordinator alloc]
      initWithBaseViewController:native_window.Get().rootViewController
                           title:base::SysUTF16ToNSString(title)
                bluetoothChooser:bluetooth_chooser_ios];
}

ShellBluetoothChooserCoordinatorHolder::
    ~ShellBluetoothChooserCoordinatorHolder() = default;

ShellBluetoothChooserMediator*
ShellBluetoothChooserCoordinatorHolder::getMediator() {
  return coordinator_.bluetoothChooserMediator;
}

}  // namespace content

@end
