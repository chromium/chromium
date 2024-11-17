// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_cell_content_configuration.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// Alpha value for a disabled content view.
constexpr CGFloat kDriveFilePickerContentViewDisabledAlpha = 0.4;

}  // namespace

// Private `DriveFilePickerCellContentConfiguration` methods.
@interface DriveFilePickerCellContentConfiguration ()

// Initializes the configuration with `listContentConfiguration` and `enabled`.
- (instancetype)initWithListContentConfiguration:
                    (UIListContentConfiguration*)listContentConfiguration
                                         enabled:(BOOL)enabled
    NS_DESIGNATED_INITIALIZER;

@end

#pragma mark - DriveFilePickerContentView

// Content view for a Drive file picker cell.
@interface DriveFilePickerContentView : UIView <UIContentView>

// Initializes the content view.
- (instancetype)initWithListContentView:(UIListContentView*)listContentView
                                enabled:(BOOL)enabled NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

@end

@implementation DriveFilePickerContentView {
  // A Drive file picker cell content view is a wrapper for `UIListContentView`.
  UIListContentView* _listContentView;
  // Whether the content view should appear as enabled.
  BOOL _enabled;
}

- (instancetype)initWithListContentView:(UIListContentView*)listContentView
                                enabled:(BOOL)enabled {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    // Initialize `_listContent`.
    _listContentView = listContentView;
    _listContentView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_listContentView];
    AddSameConstraints(self, _listContentView);
    // Initialize `_enabled`.
    _enabled = enabled;
    [self updateAlpha];
  }
  return self;
}

#pragma mark - UIContentView

- (void)setConfiguration:(id<UIContentConfiguration>)conf {
  DriveFilePickerCellContentConfiguration* configuration =
      base::apple::ObjCCast<DriveFilePickerCellContentConfiguration>(conf);
  // `conf` should always be a `DriveFilePickerCellContentConfiguration`.
  CHECK(configuration);
  _listContentView.configuration = configuration.listContentConfiguration;
  _enabled = configuration.enabled;
  [self updateAlpha];
}

- (id<UIContentConfiguration>)configuration {
  return [[DriveFilePickerCellContentConfiguration alloc]
      initWithListContentConfiguration:_listContentView.configuration
                               enabled:_enabled];
}

#pragma mark - Private

// Updates `self.alpha` as a function of `_enabled`.
- (void)updateAlpha {
  self.alpha = _enabled ? 1.0 : kDriveFilePickerContentViewDisabledAlpha;
}

@end

#pragma mark - DriveFilePickerCellContentConfiguration

@implementation DriveFilePickerCellContentConfiguration

+ (instancetype)cellConfiguration {
  UIListContentConfiguration* listContentConfiguration =
      [UIListContentConfiguration cellConfiguration];
  return [[DriveFilePickerCellContentConfiguration alloc]
      initWithListContentConfiguration:listContentConfiguration
                               enabled:YES];
}

- (instancetype)initWithListContentConfiguration:
                    (UIListContentConfiguration*)listContentConfiguration
                                         enabled:(BOOL)enabled {
  self = [super init];
  if (self) {
    _listContentConfiguration = listContentConfiguration;
    _enabled = enabled;
  }
  return self;
}

#pragma mark - NSObject

- (id)copyWithZone:(NSZone*)zone {
  UIListContentConfiguration* listContentConfiguration =
      [_listContentConfiguration copyWithZone:zone];
  return [[DriveFilePickerCellContentConfiguration alloc]
      initWithListContentConfiguration:listContentConfiguration
                               enabled:_enabled];
}

#pragma mark - UIContentConfiguration

- (UIView<UIContentView>*)makeContentView {
  UIListContentView* listContentView =
      [_listContentConfiguration makeContentView];
  return [[DriveFilePickerContentView alloc]
      initWithListContentView:listContentView
                      enabled:_enabled];
}

- (instancetype)updatedConfigurationForState:(id<UIConfigurationState>)state {
  UIListContentConfiguration* listContentConfiguration =
      [_listContentConfiguration updatedConfigurationForState:state];
  return [[DriveFilePickerCellContentConfiguration alloc]
      initWithListContentConfiguration:listContentConfiguration
                               enabled:_enabled];
}

@end
