// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/picture_in_picture/ui/picture_in_picture_view_controller.h"

#import <AVFoundation/AVFoundation.h>
#import <AVKit/AVKit.h>

#import "base/cancelable_callback.h"
#import "base/functional/bind.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/strcat.h"
#import "base/task/sequenced_task_runner.h"
#import "base/time/time.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/picture_in_picture/public/picture_in_picture_configuration.h"
#import "ios/chrome/browser/picture_in_picture/ui/picture_in_picture_mutator.h"
#import "ios/chrome/browser/picture_in_picture/ui/picture_in_picture_player_view.h"
#import "ios/chrome/browser/shared/public/commands/picture_in_picture_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
// Key path for time control status.
NSString* const kKeyPathTimeControlStatus = @"timeControlStatus";
// Key path for video rect.
NSString* const kKeyPathVideoRect = @"videoRect";
// Delay to wait before checking if the app was restored from picture in
// picture or manually (App switcher, App icon...).
constexpr base::TimeDelta kAppRestoreDelay = base::Milliseconds(50);
// Delay to wait before auto-hiding controls.
constexpr base::TimeDelta kControlsHideDelay = base::Seconds(3);
// Duration for controls fade animation.
constexpr NSTimeInterval kControlsAnimationDuration = 0.3;
// Point size for play/pause button symbol.
constexpr CGFloat kPlayPauseButtonPointSize = 25.0;
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
  PictureInPicturePlayerView* _playerView;
  // The player for picture in picture.
  AVQueuePlayer* _player;
  // The player looper for picture in picture.
  AVPlayerLooper* _playerLooper;
  // The picture in picture controller.
  AVPictureInPictureController* _pipController;
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
  // The overlay view for controls.
  UIView* _controlsOverlayView;
  // The play/pause button.
  UIButton* _playPauseButton;
  // The closure to hide controls after a delay.
  base::CancelableOnceClosure _hideControlsClosure;
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
    _feature = feature;
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
  // Set the frame of the controls overlay to match the actual video content
  // rect.
  _controlsOverlayView.frame = _playerView.playerLayer.videoRect;
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
  _playerView = [[PictureInPicturePlayerView alloc] init];
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

  AVPlayerItem* playerItem = [AVPlayerItem playerItemWithURL:_videoURL];
  _player = [AVQueuePlayer queuePlayerWithItems:@[ playerItem ]];
  _playerLooper = [AVPlayerLooper playerLooperWithPlayer:_player
                                            templateItem:playerItem];

  _playerView.playerLayer.videoGravity = AVLayerVideoGravityResizeAspect;
  _playerView.playerLayer.player = _player;

  _pipController = [[AVPictureInPictureController alloc]
      initWithPlayerLayer:_playerView.playerLayer];
  _pipController.delegate = self;
  _pipController.canStartPictureInPictureAutomaticallyFromInline = YES;

  // Add observer for accessibility focus changes to show controls.
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(accessibilityElementFocused:)
             name:UIAccessibilityElementFocusedNotification
           object:nil];

  // Add observer for time control status to detect when the player is playing.
  [_player addObserver:self
            forKeyPath:kKeyPathTimeControlStatus
               options:NSKeyValueObservingOptionNew
               context:nil];

  // Add observer for video rect changes to update overlay frame.
  [_playerView.playerLayer addObserver:self
                            forKeyPath:kKeyPathVideoRect
                               options:NSKeyValueObservingOptionNew
                               context:nil];

  [self configureControls];

  // Start playing the video.
  [_player play];
}

// Configures the controls overlay and button.
- (void)configureControls {
  // Create overlay.
  _controlsOverlayView = [[UIView alloc] init];
  _controlsOverlayView.backgroundColor = [UIColor colorWithWhite:0.0 alpha:0.4];
  _controlsOverlayView.alpha = 0.0;
  [self.contentView addSubview:_controlsOverlayView];

  // Create play/pause button.
  _playPauseButton = [UIButton buttonWithType:UIButtonTypeCustom];
  _playPauseButton.translatesAutoresizingMaskIntoConstraints = NO;
  _playPauseButton.alpha = 0.0;
  _playPauseButton.tintColor = [UIColor whiteColor];

  UIImage* pauseImage =
      DefaultSymbolWithPointSize(kPauseFillSymbol, kPlayPauseButtonPointSize);
  [_playPauseButton setImage:pauseImage forState:UIControlStateNormal];
  _playPauseButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_PICTURE_IN_PICTURE_PAUSE);

  [_playPauseButton addTarget:self
                       action:@selector(togglePlayPause)
             forControlEvents:UIControlEventTouchUpInside];

  [self.contentView addSubview:_playPauseButton];

  [NSLayoutConstraint activateConstraints:@[
    [_playPauseButton.centerXAnchor
        constraintEqualToAnchor:_playerView.centerXAnchor],
    [_playPauseButton.centerYAnchor
        constraintEqualToAnchor:_playerView.centerYAnchor],
    [_playPauseButton.widthAnchor
        constraintEqualToConstant:kPlayPauseButtonPointSize * 2],
    [_playPauseButton.heightAnchor
        constraintEqualToConstant:kPlayPauseButtonPointSize * 2],
  ]];

  _playPauseButton.backgroundColor = [UIColor colorWithWhite:0.2 alpha:0.5];
  _playPauseButton.layer.cornerRadius = kPlayPauseButtonPointSize;
  _playPauseButton.clipsToBounds = YES;

  // Add tap gesture to toggle controls.
  UITapGestureRecognizer* tapGesture =
      [[UITapGestureRecognizer alloc] initWithTarget:self
                                              action:@selector(toggleControls)];
  [self.contentView addGestureRecognizer:tapGesture];
  self.contentView.userInteractionEnabled = YES;

  self.contentView.accessibilityElements = @[ _playerView, _playPauseButton ];

  [self showControls];
}

// Sets the alpha of the controls.
- (void)setControlsAlpha:(CGFloat)alpha {
  _controlsOverlayView.alpha = alpha;
  _playPauseButton.alpha = alpha;
}

// Shows controls with animation and auto-hides after a delay.
- (void)showControls {
  __weak __typeof(self) weakSelf = self;

  [UIView animateWithDuration:kControlsAnimationDuration
                   animations:^{
                     [weakSelf setControlsAlpha:1.0];
                   }];

  // Cancel any pending hide task and schedule a new one.
  _hideControlsClosure.Reset(base::BindOnce(^{
    [weakSelf hideControls];
  }));

  // Only schedule the hide task if VoiceOver is not running.
  if (!UIAccessibilityIsVoiceOverRunning()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, _hideControlsClosure.callback(), kControlsHideDelay);
  }
}

// Hides controls with animation.
- (void)hideControls {
  // Cancel any pending hide task if we are hiding manually.
  _hideControlsClosure.Cancel();

  __weak __typeof(self) weakSelf = self;
  [UIView animateWithDuration:kControlsAnimationDuration
                   animations:^{
                     [weakSelf setControlsAlpha:0.0];
                   }];
}

// Toggles controls visibility.
- (void)toggleControls {
  if (_controlsOverlayView.alpha > 0.0) {
    [self hideControls];
  } else {
    [self showControls];
  }
}

// Toggles play/pause.
- (void)togglePlayPause {
  if (_player.timeControlStatus == AVPlayerTimeControlStatusPlaying) {
    [_player pause];
    UIImage* playImage =
        DefaultSymbolWithPointSize(kPlayFillSymbol, kPlayPauseButtonPointSize);
    [_playPauseButton setImage:playImage forState:UIControlStateNormal];
    _playPauseButton.accessibilityLabel =
        l10n_util::GetNSString(IDS_IOS_PICTURE_IN_PICTURE_PLAY);
  } else {
    [_player play];
    UIImage* pauseImage =
        DefaultSymbolWithPointSize(kPauseFillSymbol, kPlayPauseButtonPointSize);
    [_playPauseButton setImage:pauseImage forState:UIControlStateNormal];
    _playPauseButton.accessibilityLabel =
        l10n_util::GetNSString(IDS_IOS_PICTURE_IN_PICTURE_PAUSE);
  }
  [self showControls];
}

// Handles the app restore after picture in picture.
- (void)handleAppRestore {
  if (_restoredFromPictureInPicture) {
    [self showControls];
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

// Handles accessibility focus changes.
- (void)accessibilityElementFocused:(NSNotification*)notification {
  id focusedElement = notification.userInfo[UIAccessibilityFocusedElementKey];
  id unfocusedElement =
      notification.userInfo[UIAccessibilityUnfocusedElementKey];

  // Show controls when player view or play/pause button is focused.
  if (focusedElement == _playerView || focusedElement == _playPauseButton) {
    [self showControls];
    return;
  }

  // Hide controls when player view or play/pause button is unfocused.
  if (unfocusedElement == _playerView || unfocusedElement == _playPauseButton) {
    [self hideControls];
  }
}

// Removes the observer for time control status and accessibility focus.
- (void)dealloc {
  [_player removeObserver:self
               forKeyPath:kKeyPathTimeControlStatus
                  context:nil];
  [_playerView.playerLayer removeObserver:self
                               forKeyPath:kKeyPathVideoRect
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

  // Invalidates the view layout when the video rect renders to update the
  // overlay frame.
  if ([keyPath isEqualToString:kKeyPathVideoRect]) {
    [self.view setNeedsLayout];
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
