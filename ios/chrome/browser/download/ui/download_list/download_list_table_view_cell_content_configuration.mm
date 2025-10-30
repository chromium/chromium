// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/ui/download_list/download_list_table_view_cell_content_configuration.h"

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "ios/chrome/browser/download/ui/download_list/download_list_item.h"
#import "ios/chrome/browser/download/ui/download_list/download_list_table_view_cell_content_view.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/image_content_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {
/// Size for the file icon image in the download list cells.
constexpr CGFloat kFileIconImageSize = 44.0;
}  // namespace

@implementation DownloadListTableViewCellContentConfiguration

- (instancetype)init {
  self = [super init];
  if (self) {
    self.cellContentConfiguration =
        [[TableViewCellContentConfiguration alloc] init];
    self.showProgress = NO;
    self.progress = 0.0f;
    self.showCancelButton = NO;
    self.progressTintColor = [UIColor colorNamed:kBlueColor];
    self.progressTrackTintColor = [UIColor colorNamed:kGrey400Color];
  }
  return self;
}

#pragma mark - Public

+ (instancetype)configurationWithDownloadListItem:(DownloadListItem*)item {
  DownloadListTableViewCellContentConfiguration* configuration =
      [[DownloadListTableViewCellContentConfiguration alloc] init];

  // Configure the underlying cell content configuration.
  configuration.cellContentConfiguration.title = item.fileName;
  configuration.cellContentConfiguration.subtitle = item.detailText;

  // Set subtitle color to red for failed downloads, use default for others.
  if (item.downloadState == web::DownloadTask::State::kFailed) {
    configuration.cellContentConfiguration.subtitleColor =
        [UIColor colorNamed:kRed600Color];
  }

  // Configure file icon.
  if (item.fileTypeIcon) {
    ImageContentConfiguration* imageConfiguration =
        [[ImageContentConfiguration alloc] init];
    imageConfiguration.image = item.fileTypeIcon;
    imageConfiguration.imageSize =
        CGSizeMake(kFileIconImageSize, kFileIconImageSize);
    configuration.cellContentConfiguration.leadingConfiguration =
        imageConfiguration;
  }

  // Configure progress for ongoing downloads.
  if (item.shouldShowProgressView) {
    configuration.showProgress = YES;
    configuration.progress = item.downloadProgress;
  }

  // Configure cancel button for cancellable downloads.
  configuration.showCancelButton = item.cancelable;

  return configuration;
}

#pragma mark - Public

+ (void)registerCellForTableView:(UITableView*)tableView {
  [tableView registerClass:TableViewCell.class
      forCellReuseIdentifier:NSStringFromClass(self.class)];
}

+ (TableViewCell*)dequeueTableViewCell:(UITableView*)tableView {
  TableViewCell* cell = [tableView
      dequeueReusableCellWithIdentifier:NSStringFromClass(self.class)];
  return cell;
}

#pragma mark - UIContentConfiguration

- (id<UIContentView>)makeContentView {
  return
      [[DownloadListTableViewCellContentView alloc] initWithConfiguration:self];
}

- (instancetype)updatedConfigurationForState:(id<UIConfigurationState>)state {
  return self;
}

#pragma mark - NSCopying

- (id)copyWithZone:(NSZone*)zone {
  DownloadListTableViewCellContentConfiguration* copy =
      [[DownloadListTableViewCellContentConfiguration allocWithZone:zone] init];

  // The updates to properties must be reflected in the copy method.
  // LINT.IfChange(Copy)
  copy.cellContentConfiguration =
      [self.cellContentConfiguration copyWithZone:zone];
  copy.showProgress = self.showProgress;
  copy.progress = self.progress;
  copy.showCancelButton = self.showCancelButton;
  copy.progressTintColor = self.progressTintColor;
  copy.progressTrackTintColor = self.progressTrackTintColor;
  // LINT.ThenChange(download_list_table_view_cell_content_configuration.h:Copy)

  return copy;
}

@end
