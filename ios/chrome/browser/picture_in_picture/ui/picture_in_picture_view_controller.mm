// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/picture_in_picture/ui/picture_in_picture_view_controller.h"

#import "ios/chrome/browser/picture_in_picture/public/picture_in_picture_configuration.h"
#import "ios/chrome/browser/picture_in_picture/ui/picture_in_picture_mutator.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"

@interface PictureInPictureViewController ()
@end

@implementation PictureInPictureViewController {
  // The title of the video.
  NSString* _title;
  // The primary button title of the video.
  NSString* _primaryButtonTitle;
  // The URL of the video.
  NSURL* _videoURL;
  // The player view for picture in picture.
  UIView* _playerView;
}

- (instancetype)initWithTitle:(NSString*)title
           primaryButtonTitle:(NSString*)primaryButtonTitle
                     videoURL:(NSURL*)videoURL {
  ButtonStackConfiguration* buttonConfiguration =
      [[ButtonStackConfiguration alloc] init];
  buttonConfiguration.primaryActionString = primaryButtonTitle;
  if ((self = [super initWithConfiguration:buttonConfiguration])) {
    _title = title;
    _primaryButtonTitle = primaryButtonTitle;
    _videoURL = videoURL;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.title = _title;
  self.scrollEnabled = false;
  self.showsVerticalScrollIndicator = false;

  [self configureAudio];
  [self configurePlayer];
}

#pragma mark - Private

// Configures audio for picture in picture.
- (void)configureAudio {
  // TODO(crbug.com/484430263): Configure audio for picture in picture.
}

// Configures player for picture in picture.
- (void)configurePlayer {
  _playerView = [[UIView alloc] init];
  _playerView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.contentView addSubview:_playerView];
  [NSLayoutConstraint activateConstraints:@[
    [_playerView.leadingAnchor
        constraintEqualToAnchor:self.contentView.leadingAnchor],
    [_playerView.trailingAnchor
        constraintEqualToAnchor:self.contentView.trailingAnchor],
    [_playerView.topAnchor constraintEqualToAnchor:self.contentView.topAnchor],
    [_playerView.bottomAnchor
        constraintEqualToAnchor:self.contentView.bottomAnchor],
  ]];
  // TODO(crbug.com/484430263): Configure player for picture in picture.
}

@end
