// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/picture_in_picture/ui/picture_in_picture_view_controller.h"

#import <AVFoundation/AVFoundation.h>
#import <AVKit/AVKit.h>

#import "ios/chrome/browser/picture_in_picture/public/picture_in_picture_configuration.h"
#import "ios/chrome/browser/picture_in_picture/ui/picture_in_picture_mutator.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"

@interface PictureInPictureViewController () <
    AVPictureInPictureControllerDelegate>
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
  // The player for picture in picture.
  AVQueuePlayer* _player;
  // The player looper for picture in picture.
  AVPlayerLooper* _playerLooper;
  // The picture in picture controller.
  AVPictureInPictureController* _pipController;
  // The player layer for picture in picture.
  AVPlayerLayer* _playerLayer;
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

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  _playerLayer.frame = _playerView.bounds;
}

#pragma mark - Private

// Configures audio for picture in picture.
- (void)configureAudio {
  AVAudioSession* session = [AVAudioSession sharedInstance];
  [session setCategory:AVAudioSessionCategoryPlayback
                  mode:AVAudioSessionModeMoviePlayback
               options:0
                 error:nil];
  [session setActive:YES error:nil];
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

  AVPlayerItem* playerItem = [AVPlayerItem playerItemWithURL:_videoURL];
  _player = [AVQueuePlayer queuePlayerWithItems:@[ playerItem ]];
  _playerLooper = [AVPlayerLooper playerLooperWithPlayer:_player
                                            templateItem:playerItem];
  _playerLayer = [AVPlayerLayer playerLayerWithPlayer:_player];
  _playerLayer.videoGravity = AVLayerVideoGravityResizeAspect;
  [_playerView.layer addSublayer:_playerLayer];

  _pipController =
      [[AVPictureInPictureController alloc] initWithPlayerLayer:_playerLayer];
  _pipController.delegate = self;
  _pipController.canStartPictureInPictureAutomaticallyFromInline = YES;

  [_player play];
}

#pragma mark - AVPictureInPictureControllerDelegate

- (void)pictureInPictureControllerWillStartPictureInPicture:
    (AVPictureInPictureController*)pictureInPictureController {
}

- (void)pictureInPictureControllerWillStopPictureInPicture:
    (AVPictureInPictureController*)pictureInPictureController {
}

- (void)pictureInPictureControllerDidStopPictureInPicture:
    (AVPictureInPictureController*)pictureInPictureController {
}

- (void)pictureInPictureController:
            (AVPictureInPictureController*)pictureInPictureController
    restoreUserInterfaceForPictureInPictureStopWithCompletionHandler:
        (void (^)(BOOL restored))completionHandler {
  completionHandler(YES);
}

@end
