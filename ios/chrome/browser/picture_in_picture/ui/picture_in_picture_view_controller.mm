// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/picture_in_picture/ui/picture_in_picture_view_controller.h"

#import <AVFoundation/AVFoundation.h>
#import <AVKit/AVKit.h>

#import "base/functional/bind.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/strcat.h"
#import "base/task/sequenced_task_runner.h"
#import "base/time/time.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/picture_in_picture/public/picture_in_picture_configuration.h"
#import "ios/chrome/browser/picture_in_picture/ui/picture_in_picture_mutator.h"
#import "ios/chrome/browser/shared/public/commands/picture_in_picture_commands.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
// Key path for time control status.
NSString* const kKeyPathTimeControlStatus = @"timeControlStatus";
// Delay to wait before checking if the app was restored from picture in
// picture or manually (App switcher, App icon...).
constexpr base::TimeDelta kAppRestoreDelay = base::Milliseconds(50);
// Accessibility label for the picture in picture view controller.
NSString* accessibilityLabel(PictureInPictureFeature feature) {
  switch (feature) {
    case PictureInPictureFeature::kDefaultBrowser:
      return l10n_util::GetNSString(
          IDS_IOS_DEFAULT_BROWSER_PIP_ACCESSIBILITY_ANNOUNCEMENT);
  }
}
}  // namespace

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
  // The feature for picture in picture.
  PictureInPictureFeature _feature;
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
  // Flag set to true if the picture in picture should auto start.
  BOOL _shouldAutoStartPictureInPicture;
  // Flag set to true if the app was restored to foreground by the user tapping
  // on the fullscreen button within the picture in picture window.
  BOOL _restoredFromPictureInPicture;
  // Flag set to true if the app was restored to foreground by the user
  // manually reopening the app or by the picture in picture fullscreen button.
  BOOL _appWasRestored;
  // The time when picture in picture started.
  base::TimeTicks _pipStartTime;
}

- (instancetype)initWithTitle:(NSString*)title
           primaryButtonTitle:(NSString*)primaryButtonTitle
                     videoURL:(NSURL*)videoURL
                      feature:(PictureInPictureFeature)feature {
  ButtonStackConfiguration* buttonConfiguration =
      [[ButtonStackConfiguration alloc] init];
  buttonConfiguration.primaryActionString = primaryButtonTitle;
  if ((self = [super initWithConfiguration:buttonConfiguration])) {
    _title = title;
    _primaryButtonTitle = primaryButtonTitle;
    _videoURL = videoURL;
    _shouldAutoStartPictureInPicture = YES;
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

#pragma mark - Public

- (void)dismissIfNotPipRestore {
  __weak __typeof(self) weakSelf = self;
  _appWasRestored = YES;
  // Delay execution by `kAppRestoreDelay` to allow
  // `restoreUserInterfaceForPictureInPictureStopWithCompletionHandler` to fire
  // first. This lets us distinguish a manual launch (which dismisses
  // everything) from a PiP restore (which preserves the UI).
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(^{
        [weakSelf handleAppRestore];
      }),
      kAppRestoreDelay);
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
  // Configure the player view.
  _playerView = [[UIView alloc] init];
  _playerView.isAccessibilityElement = YES;
  _playerView.accessibilityTraits = UIAccessibilityTraitStartsMediaSession;
  _playerView.accessibilityLabel = accessibilityLabel(_feature);
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

  // Configure the player.
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

  // Add observer for time control status to detect when the player is playing.
  [_player addObserver:self
            forKeyPath:kKeyPathTimeControlStatus
               options:NSKeyValueObservingOptionNew
               context:nil];

  // Start playing the video.
  [_player play];
}

// Handles the app restore after picture in picture.
- (void)handleAppRestore {
  if (_restoredFromPictureInPicture) {
    [self recordAppRestoration:PictureInPictureAppRestoration::
                                   kPictureInPictureFullscreenButton];
    return;
  }

  if (_pipController.isPictureInPictureActive) {
    [self recordAppRestoration:PictureInPictureAppRestoration::kManual];
    _playerView.alpha = 0.0f;
    [_pipController stopPictureInPicture];
    [_playerView removeFromSuperview];
    [self recordDismissalReason:PictureInPictureDismissalReason::
                                    kManualAppRestoration];
    [_handler dismissPictureInPicture];
  }
}

// Removes the observer for time control status.
- (void)dealloc {
  [_player removeObserver:self
               forKeyPath:kKeyPathTimeControlStatus
                  context:nil];
}

// Observes the time control status of the player and trigger feature
// destination if the player is playing.
- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary<NSKeyValueChangeKey, id>*)change
                       context:(void*)context {
  if ([keyPath isEqualToString:kKeyPathTimeControlStatus]) {
    __weak __typeof(self) weakSelf = self;
    // Ensure the player is fully running and playing before backgrounding the
    // app and avoid a race condition.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(^{
          [weakSelf triggerFeatureDestination];
        }));
    return;
  }

  [super observeValueForKeyPath:keyPath
                       ofObject:object
                         change:change
                        context:context];
}

// Triggers the feature destination if this is the initial picture in
// picture start.
- (void)triggerFeatureDestination {
  switch (_player.timeControlStatus) {
    case AVPlayerTimeControlStatusPlaying:
      if (_pipController.isPictureInPictureActive) {
        // Do nothing if picture in picture is currently active.
        return;
      }

      // If this is not the initial picture in picture start, return
      // without triggering the feature destination.
      if (!_shouldAutoStartPictureInPicture) {
        return;
      }

      // Trigger feature destination.
      [_mutator startDestination];
      _shouldAutoStartPictureInPicture = NO;
      break;
    case AVPlayerTimeControlStatusPaused:
    case AVPlayerTimeControlStatusWaitingToPlayAtSpecifiedRate:
      break;
  }
}

// Records the app restoration.
- (void)recordAppRestoration:(PictureInPictureAppRestoration)appRestoration {
  base::UmaHistogramEnumeration(
      base::StrCat({"IOS.PictureInPicture.",
                    PictureInPictureFeatureToString(_feature),
                    ".AppRestoration"}),
      appRestoration);
}

// Records the session duration.
- (void)recordSessionDuration:(base::TimeDelta)sessionDuration {
  base::UmaHistogramLongTimes(
      base::StrCat({"IOS.PictureInPicture.",
                    PictureInPictureFeatureToString(_feature),
                    ".SessionDuration"}),
      sessionDuration);
}

// Records the start successful.
- (void)recordStartSuccessful:(BOOL)success {
  base::UmaHistogramBoolean(
      base::StrCat({"IOS.PictureInPicture.",
                    PictureInPictureFeatureToString(_feature),
                    ".StartSuccessful"}),
      success);
}

// Records the dismissal reason.
- (void)recordDismissalReason:(PictureInPictureDismissalReason)dismissalReason {
  base::UmaHistogramEnumeration(
      base::StrCat({"IOS.PictureInPicture.",
                    PictureInPictureFeatureToString(_feature),
                    ".DismissalReason"}),
      dismissalReason);
}

#pragma mark - AVPictureInPictureControllerDelegate

- (void)pictureInPictureControllerWillStartPictureInPicture:
    (AVPictureInPictureController*)pictureInPictureController {
}

- (void)pictureInPictureControllerDidStartPictureInPicture:
    (AVPictureInPictureController*)pictureInPictureController {
  _restoredFromPictureInPicture = NO;
  _appWasRestored = NO;
  _pipStartTime = base::TimeTicks::Now();
  [self recordStartSuccessful:YES];
}

- (void)pictureInPictureController:
            (AVPictureInPictureController*)pictureInPictureController
    failedToStartPictureInPictureWithError:(NSError*)error {
  [self recordStartSuccessful:NO];
}

- (void)pictureInPictureControllerWillStopPictureInPicture:
    (AVPictureInPictureController*)pictureInPictureController {
}

- (void)pictureInPictureControllerDidStopPictureInPicture:
    (AVPictureInPictureController*)pictureInPictureController {
  [self recordSessionDuration:base::TimeTicks::Now() - _pipStartTime];
  // If this method was called without an app restore, record a close
  // interaction.
  if (!_appWasRestored) {
    [self recordDismissalReason:PictureInPictureDismissalReason::
                                    kPictureInPictureCloseButton];
    [_handler dismissPictureInPicture];
  }
}

- (void)pictureInPictureController:
            (AVPictureInPictureController*)pictureInPictureController
    restoreUserInterfaceForPictureInPictureStopWithCompletionHandler:
        (void (^)(BOOL restored))completionHandler {
  _restoredFromPictureInPicture = YES;
  completionHandler(YES);
}

@end
