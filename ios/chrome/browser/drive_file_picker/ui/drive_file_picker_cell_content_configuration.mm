// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_cell_content_configuration.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// Alpha value for a disabled content view.
constexpr CGFloat kDriveFilePickerContentViewDisabledAlpha = 0.4;
// Point size of shortcut symbol image.
constexpr CGFloat kShortcutSymbolImagePointSize = 20;

}  // namespace

// Private `DriveFilePickerCellContentConfiguration` methods.
@interface DriveFilePickerCellContentConfiguration ()

// Initializes the configuration with `listContentConfiguration` and `enabled`.
- (instancetype)initWithListContentConfiguration:
                    (UIListContentConfiguration*)listContentConfiguration
                                         enabled:(BOOL)enabled
                                      isShortcut:(BOOL)isShortcut
    NS_DESIGNATED_INITIALIZER;

@end

#pragma mark - DriveFilePickerContentView

// Content view for a Drive file picker cell.
@interface DriveFilePickerContentView : UIView <UIContentView>

// Initializes the content view.
- (instancetype)initWithListContentView:(UIListContentView*)listContentView
                                enabled:(BOOL)enabled
                             isShortcut:(BOOL)isShortcut
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

@end

@implementation DriveFilePickerContentView {
  // A Drive file picker cell content view is a wrapper for `UIListContentView`.
  UIListContentView* _listContentView;
  // Whether the content view should appear as enabled.
  BOOL _enabled;
  // Whether the content view is a shortcut.
  BOOL _isShortcut;
  // Image view for the shortcut icon.
  UIImageView* _shortcutImageView;
}

- (instancetype)initWithListContentView:(UIListContentView*)listContentView
                                enabled:(BOOL)enabled
                             isShortcut:(BOOL)isShortcut {
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
    // Initialize `_isShortcut`.
    _isShortcut = isShortcut;
    [self updateShortcutImageView];
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
  _shortcutImageView.hidden = !configuration.isShortcut;
}

- (id<UIContentConfiguration>)configuration {
  return [[DriveFilePickerCellContentConfiguration alloc]
      initWithListContentConfiguration:_listContentView.configuration
                               enabled:_enabled
                            isShortcut:!_shortcutImageView.hidden];
}

#pragma mark - Private

// Updates `self.alpha` as a function of `_enabled`.
- (void)updateAlpha {
  self.alpha = _enabled ? 1.0 : kDriveFilePickerContentViewDisabledAlpha;
}

// Updates `_shortcutImageView` as a function of `_isShortcut`.
- (void)updateShortcutImageView {
  if (!_isShortcut) {
    _shortcutImageView.hidden = YES;
    return;
  }
  // If `_isShortcut` is YES then lazily initialize a nil `_shortcutImageView`.
  if (!_shortcutImageView) {
    UIImage* shortcutImage = DefaultSymbolWithPointSize(
        kArrowUTurnForwardCircleFillSymbol, kShortcutSymbolImagePointSize);
    shortcutImage = SymbolWithPalette(shortcutImage, @[
      [UIColor colorNamed:kGrey600Color],
      [UIColor colorNamed:kGroupedSecondaryBackgroundColor],
    ]);
    _shortcutImageView = [[UIImageView alloc] initWithImage:shortcutImage];
    _shortcutImageView.translatesAutoresizingMaskIntoConstraints = NO;
    [_listContentView addSubview:_shortcutImageView];
    [NSLayoutConstraint activateConstraints:@[
      [_shortcutImageView.centerXAnchor
          constraintEqualToAnchor:_listContentView.imageLayoutGuide
                                      .leadingAnchor],
      [_shortcutImageView.centerYAnchor
          constraintEqualToAnchor:_listContentView.imageLayoutGuide
                                      .bottomAnchor],
    ]];
  }
  _shortcutImageView.hidden = NO;
}

@end

#pragma mark - DriveFilePickerCellContentConfiguration

@implementation DriveFilePickerCellContentConfiguration

+ (instancetype)cellConfiguration {
  UIListContentConfiguration* listContentConfiguration =
      [UIListContentConfiguration cellConfiguration];
  return [[DriveFilePickerCellContentConfiguration alloc]
      initWithListContentConfiguration:listContentConfiguration
                               enabled:YES
                            isShortcut:NO];
}

- (instancetype)initWithListContentConfiguration:
                    (UIListContentConfiguration*)listContentConfiguration
                                         enabled:(BOOL)enabled
                                      isShortcut:(BOOL)isShortcut {
  self = [super init];
  if (self) {
    _listContentConfiguration = listContentConfiguration;
    _enabled = enabled;
    _isShortcut = isShortcut;
  }
  return self;
}

#pragma mark - NSObject

- (id)copyWithZone:(NSZone*)zone {
  UIListContentConfiguration* listContentConfiguration =
      [_listContentConfiguration copyWithZone:zone];
  return [[DriveFilePickerCellContentConfiguration alloc]
      initWithListContentConfiguration:listContentConfiguration
                               enabled:_enabled
                            isShortcut:_isShortcut];
}

#pragma mark - UIContentConfiguration

- (UIView<UIContentView>*)makeContentView {
  UIListContentView* listContentView =
      [_listContentConfiguration makeContentView];
  return [[DriveFilePickerContentView alloc]
      initWithListContentView:listContentView
                      enabled:_enabled
                   isShortcut:_isShortcut];
}

- (instancetype)updatedConfigurationForState:(id<UIConfigurationState>)state {
  UIListContentConfiguration* listContentConfiguration =
      [_listContentConfiguration updatedConfigurationForState:state];
  return [[DriveFilePickerCellContentConfiguration alloc]
      initWithListContentConfiguration:listContentConfiguration
                               enabled:_enabled
                            isShortcut:_isShortcut];
}

@end
