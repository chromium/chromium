// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/bwg/ui/gemini_camera_view_controller.h"

#import "ios/chrome/browser/settings/ui_bundled/bwg/coordinator/gemini_settings_mutator.h"
#import "ios/chrome/browser/settings/ui_bundled/bwg/utils/gemini_settings_metrics.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Section identifiers in the Gemini Camera settings table view.

enum SectionIdentifier {
  kSectionIdentifierCamera = kSectionIdentifierEnumZero,
};

enum ItemType {
  kItemTypeCamera = kItemTypeEnumZero,
  kItemTypeCameraFooter,
};

// Table identifier.
NSString* const kGeminiCameraViewTableIdentifier =
    @"GeminiCameraViewTableIdentifier";

// Row identifiers.
NSString* const kCameraCellId = @"CameraCellId";

}  // namespace

@implementation GeminiCameraViewController {
  // Switch item for toggling permissions for the camera.
  TableViewSwitchItem* _cameraSwitchItem;
  // Camera preference value.
  BOOL _cameraEnabled;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.accessibilityIdentifier = kGeminiCameraViewTableIdentifier;
  self.title = l10n_util::GetNSString(IDS_IOS_GEMINI_SETTINGS_CAMERA_TITLE);
  [self loadModel];
}

- (void)loadModel {
  [super loadModel];
  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:kSectionIdentifierCamera];

  _cameraSwitchItem =
      [[TableViewSwitchItem alloc] initWithType:kItemTypeCamera];
  _cameraSwitchItem.text =
      l10n_util::GetNSString(IDS_IOS_GEMINI_SETTINGS_CAMERA_TITLE);
  _cameraSwitchItem.target = self;
  _cameraSwitchItem.selector = @selector(cameraSwitchToggled:);
  _cameraSwitchItem.on = _cameraEnabled;
  _cameraSwitchItem.accessibilityIdentifier = kCameraCellId;

  TableViewLinkHeaderFooterItem* cameraFooterItem =
      [[TableViewLinkHeaderFooterItem alloc]
          initWithType:kItemTypeCameraFooter];
  cameraFooterItem.text =
      l10n_util::GetNSString(IDS_IOS_GEMINI_SETTINGS_CAMERA_FOOTER_TEXT);

  [model addItem:_cameraSwitchItem
      toSectionWithIdentifier:kSectionIdentifierCamera];
  [model setFooter:cameraFooterItem
      forSectionWithIdentifier:kSectionIdentifierCamera];
}

- (void)setCameraEnabled:(BOOL)enabled {
  _cameraEnabled = enabled;
  if ([self isViewLoaded]) {
    _cameraSwitchItem.on = _cameraEnabled;
    [self reconfigureCellsForItems:@[ _cameraSwitchItem ]];
  }
}

#pragma mark - Private

// Called from the Camera setting's UIControlEventValueChanged. Updates
// underlying camera permission pref.
- (void)cameraSwitchToggled:(UISwitch*)switchView {
  RecordGeminiCameraSettingsToggled(switchView.isOn);
  [self.mutator setCameraPermissionPref:switchView.isOn];
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  RecordGeminiCameraSettingsClose();
}

- (void)reportBackUserAction {
  RecordGeminiCameraSettingsBack();
}

@end
