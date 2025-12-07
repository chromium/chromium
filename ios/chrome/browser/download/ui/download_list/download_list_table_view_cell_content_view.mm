// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/ui/download_list/download_list_table_view_cell_content_view.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/download/ui/download_list/download_list_table_view_cell_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_view.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"

namespace {
/// Size for the file icon image in the download list cells.
constexpr CGFloat kFileIconImageSize = 44.0;
constexpr CGFloat kProgressViewHeight = 2.0;
}  // namespace

@implementation DownloadListTableViewCellContentView {
  DownloadListTableViewCellContentConfiguration* _configuration;
  TableViewCellContentView* _cellContentView;
  UIProgressView* _progressView;
}

- (instancetype)initWithConfiguration:
    (DownloadListTableViewCellContentConfiguration*)configuration {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _configuration = [configuration copy];

    // Create the underlying cell content view.
    _cellContentView = base::apple::ObjCCastStrict<TableViewCellContentView>(
        [configuration.cellContentConfiguration makeContentView]);
    _cellContentView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_cellContentView];

    // Create the progress view.
    _progressView = [[UIProgressView alloc]
        initWithProgressViewStyle:UIProgressViewStyleDefault];
    _progressView.translatesAutoresizingMaskIntoConstraints = NO;
    _progressView.hidden = !configuration.showProgress;
    _progressView.progress = configuration.progress;
    _progressView.progressTintColor = configuration.progressTintColor;
    _progressView.trackTintColor = configuration.progressTrackTintColor;
    [self addSubview:_progressView];

    [self setupConstraints];
    [self updateConfiguration];
  }
  return self;
}

#pragma mark - Private

// Sets up the constraints for the subviews.
- (void)setupConstraints {
  // Cell content view constraints - fill the entire view.
  [NSLayoutConstraint activateConstraints:@[
    [_cellContentView.topAnchor constraintEqualToAnchor:self.topAnchor],
    [_cellContentView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
    [_cellContentView.trailingAnchor
        constraintEqualToAnchor:self.trailingAnchor],
    [_cellContentView.bottomAnchor
        constraintEqualToAnchor:_progressView.topAnchor],
  ]];

  // Progress view constraints - aligned at the bottom with horizontal padding.
  [NSLayoutConstraint activateConstraints:@[
    [_progressView.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
    [_progressView.leadingAnchor
        constraintEqualToAnchor:self.leadingAnchor
                       constant:kFileIconImageSize + kTableViewImagePadding +
                                kTableViewHorizontalSpacing],
    [_progressView.trailingAnchor
        constraintEqualToAnchor:self.trailingAnchor
                       constant:-kTableViewHorizontalSpacing],
    [_progressView.heightAnchor constraintEqualToConstant:kProgressViewHeight],
  ]];
}

// Updates the view based on the current configuration.
- (void)updateConfiguration {
  // Update the cell content view configuration.
  _cellContentView.configuration = _configuration.cellContentConfiguration;

  // Update progress view.
  _progressView.hidden = !_configuration.showProgress;
  _progressView.progress = _configuration.progress;
  _progressView.progressTintColor = _configuration.progressTintColor;
  _progressView.trackTintColor = _configuration.progressTrackTintColor;
}

#pragma mark - UIContentView

- (id<UIContentConfiguration>)configuration {
  return _configuration;
}

- (void)setConfiguration:(id<UIContentConfiguration>)configuration {
  DownloadListTableViewCellContentConfiguration* downloadConfiguration =
      (DownloadListTableViewCellContentConfiguration*)configuration;

  if (![_configuration isEqual:downloadConfiguration]) {
    _configuration = [downloadConfiguration copy];
    [self updateConfiguration];
  }
}

- (BOOL)supportsConfiguration:(id<UIContentConfiguration>)configuration {
  return [configuration
      isKindOfClass:[DownloadListTableViewCellContentConfiguration class]];
}

#pragma mark - ChromeContentView

- (BOOL)hasCustomAccessibilityActivationPoint {
  return NO;
}

@end
