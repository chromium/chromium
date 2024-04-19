// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/shell/browser/bluetooth/ios/shell_bluetooth_device_list_view_controller.h"

#import "content/shell/browser/bluetooth/ios/shell_bluetooth_device_list_delegate.h"

// Has device information to display it on a cell of UITableView.
@interface DeviceInfo : NSObject

@property(nonatomic, copy) NSString* deviceID;

@property(nonatomic, copy) NSString* deviceName;

@property(nonatomic, assign) BOOL gattConnected;

@property(nonatomic, assign) NSInteger signalStrengthLevel;

@property(nonatomic, assign) NSInteger cellIndex;

- (instancetype)initWithDeviceID:(NSString*)deviceID
                      deviceName:(NSString*)deviceName
                   gattConnected:(BOOL)gattConnected
             signalStrengthLevel:(NSInteger)signalStrengthLevel
                       cellIndex:(NSInteger)cellIndex;

- (void)updateDeviceName:(NSString*)deviceName
           gattConnected:(BOOL)gattConnected
     signalStrengthLevel:(NSInteger)signalStrengthLevel;
@end

@implementation DeviceInfo
- (instancetype)initWithDeviceID:(NSString*)deviceID
                      deviceName:(NSString*)deviceName
                   gattConnected:(BOOL)gattConnected
             signalStrengthLevel:(NSInteger)signalStrengthLevel
                       cellIndex:(NSInteger)cellIndex {
  if ((self = [super init])) {
    _deviceID = [deviceID copy];
    _deviceName = [deviceName copy];
    _gattConnected = gattConnected;
    _signalStrengthLevel = signalStrengthLevel;
    _cellIndex = cellIndex;
  }
  return self;
}

- (void)updateDeviceName:(NSString*)deviceName
           gattConnected:(BOOL)gattConnected
     signalStrengthLevel:(NSInteger)signalStrengthLevel {
  self.deviceName = [deviceName copy];
  self.gattConnected = gattConnected;
  self.signalStrengthLevel = signalStrengthLevel;
}
@end

@implementation ShellDeviceListViewController
- (instancetype)initWithTitle:(NSString*)title {
  if (!(self = [super init])) {
    return nil;
  }
  _selectedRowIndex = -1;
  _listTitle = [title copy];

  return self;
}

- (NSString*)tableView:(UITableView*)tableView
    titleForHeaderInSection:(NSInteger)section {
  return self.listTitle;
}

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView {
  return 1;
}

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  return self.deviceIDs.count;
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  static NSString* cellIdentifier = @"cell";
  UITableViewCell* cell =
      [tableView dequeueReusableCellWithIdentifier:cellIdentifier];

  if (cell == nil) {
    cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleSubtitle
                                  reuseIdentifier:cellIdentifier];
  }
  NSString* deviceID = [self.deviceIDs objectAtIndex:indexPath.row];
  cell.textLabel.text = deviceID;
  DeviceInfo* info = [self.deviceInfos objectForKey:deviceID];
  cell.detailTextLabel.text = info.deviceName;
  // TODO(crbug.com/40263537): Display more information if required.
  return cell;
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  self.selectedRowIndex = indexPath.row;
  [self dismissViewControllerAnimated:YES completion:Nil];
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.deviceIDs = [[NSMutableArray alloc] init];
  self.deviceInfos = [[NSMutableDictionary alloc] init];
}

- (void)viewDidDisappear:(BOOL)animated {
  bool selected = (self.selectedRowIndex != -1);
  [self updateSelectedInformation:selected];
}

- (void)updateSelectedInformation:(bool)selected {
  NSString* selectedText = @"";
  if (selected) {
    NSIndexPath* indexPath = [NSIndexPath indexPathForRow:_selectedRowIndex
                                                inSection:0];
    UITableViewCell* selectedCell =
        [self.tableView cellForRowAtIndexPath:indexPath];
    selectedText = selectedCell.textLabel.text;
  }
  [self.listDelegate updateSelectedInfo:selected selectedText:selectedText];
}

#pragma mark - ShellBluetoothDeviceListConsumer

- (void)addOrUpdateDevice:(NSString*)deviceID
         shouldUpdateName:(bool)shouldUpdateName
               deviceName:(NSString*)deviceName
            gattConnected:(bool)gattConnected
      signalStrengthLevel:(NSInteger)signalStrengthLevel {
  DeviceInfo* info = [self.deviceInfos objectForKey:deviceID];
  if (info) {
    if (!shouldUpdateName) {
      return;
    }
    if ([info.deviceName isEqualToString:deviceName] &&
        info.gattConnected == gattConnected &&
        info.signalStrengthLevel == signalStrengthLevel) {
      return;
    }
    [info updateDeviceName:deviceName
              gattConnected:gattConnected
        signalStrengthLevel:signalStrengthLevel];
    if (info.cellIndex == -1) {
      // This information is not added to the table yet.
      return;
    }
    NSIndexPath* updatingPath = [NSIndexPath indexPathForRow:info.cellIndex
                                                   inSection:0];
    UITableViewCell* updatingCell =
        [self.tableView cellForRowAtIndexPath:updatingPath];
    updatingCell.detailTextLabel.text = deviceName;
    // TODO(crbug.com/40263537): Display more information if required.
  } else {
    if (!self.isViewLoaded || !self.view.window) {
      // If the view disappears, it skips updating the table.
      return;
    }
    [self.deviceIDs addObject:deviceID];
    DeviceInfo* new_info =
        [[DeviceInfo alloc] initWithDeviceID:deviceID
                                  deviceName:deviceName
                               gattConnected:gattConnected
                         signalStrengthLevel:signalStrengthLevel
                                   cellIndex:-1];
    [self.deviceInfos setObject:new_info forKey:deviceID];
    [self.tableView beginUpdates];
    NSInteger rowIndex = self.deviceIDs.count - 1;
    NSIndexPath* newPath = [NSIndexPath indexPathForRow:rowIndex inSection:0];
    [new_info setCellIndex:rowIndex];
    [self.tableView insertRowsAtIndexPaths:@[ newPath ]
                          withRowAnimation:UITableViewRowAnimationAutomatic];
    [self.tableView endUpdates];
  }
}
@end
