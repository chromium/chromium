// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_empty_view.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

const CGFloat kSymbolSize = 18;
const CGFloat kIconCenterPadding = 20;
const CGFloat kMessageTopPadding = 8;
const CGFloat kMessageLeadingTrailingPadding = 20;

}  // namespace

@implementation DriveFilePickerEmptyView {
  NSString* _message;
  NSString* _symbolName;
}

- (instancetype)initWithMessage:(NSString*)message
                     symbolName:(NSString*)symbolName {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _message = message;
    _symbolName = symbolName;
  }
  return self;
}

+ (instancetype)emptyDriveFolderView {
  DriveFilePickerEmptyView* emptyFolderView = [[DriveFilePickerEmptyView alloc]
      initWithMessage:l10n_util::GetNSString(
                          IDS_IOS_DRIVE_FILE_PICKER_EMPTY_FOLDER_MESSAGE)
           symbolName:kFolderSymbol];
  [emptyFolderView configureAndLayoutSubviews];
  return emptyFolderView;
}

+ (instancetype)noMatchingResultView {
  DriveFilePickerEmptyView* emptyFolderView = [[DriveFilePickerEmptyView alloc]
      initWithMessage:l10n_util::GetNSString(
                          IDS_IOS_DRIVE_FILE_PICKER_NO_MATCHING_RESULTS_MESSAGE)
           symbolName:kMagnifyingglassSymbol];
  [emptyFolderView configureAndLayoutSubviews];
  return emptyFolderView;
}

// Configures the view and adds a layout for its subviews.
- (void)configureAndLayoutSubviews {
  UIImageView* folderIconView = [[UIImageView alloc]
      initWithImage:DefaultSymbolWithPointSize(_symbolName, kSymbolSize)];
  folderIconView.tintColor = [UIColor colorNamed:kTextSecondaryColor];
  folderIconView.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:folderIconView];

  UILabel* messageLabel = [[UILabel alloc] init];
  messageLabel.text = _message;
  messageLabel.numberOfLines = 0;
  messageLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  messageLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  messageLabel.textAlignment = NSTextAlignmentCenter;
  messageLabel.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:messageLabel];

  [NSLayoutConstraint activateConstraints:@[
    [folderIconView.centerXAnchor constraintEqualToAnchor:self.centerXAnchor],
    [folderIconView.centerYAnchor constraintEqualToAnchor:self.centerYAnchor
                                                 constant:-kIconCenterPadding],

    [messageLabel.topAnchor constraintEqualToAnchor:folderIconView.bottomAnchor
                                           constant:kMessageTopPadding],
    [messageLabel.centerXAnchor constraintEqualToAnchor:self.centerXAnchor],
    [messageLabel.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:self.leadingAnchor
                                    constant:kMessageLeadingTrailingPadding],
    [messageLabel.trailingAnchor
        constraintLessThanOrEqualToAnchor:self.trailingAnchor
                                 constant:-kMessageLeadingTrailingPadding]
  ]];
}

@end
