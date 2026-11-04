// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/ui/ttc_voice_plate_view.h"

#import "base/functional/bind.h"
#import "base/location.h"
#import "base/time/time.h"
#import "base/timer/timer.h"
#import "ios/chrome/browser/ai_prototyping/ttc/ui/ttc_voice_plate_consumer.h"
#import "ios/chrome/browser/ai_prototyping/ttc/ui/ttc_voice_plate_mutator.h"
#import "ios/chrome/browser/ai_prototyping/ttc/ui/ttc_waveform_view.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

NSString* const kTTCVoicePlateCloseButtonAccessibilityIdentifier =
    @"TTCVoicePlateCloseButtonAccessibilityIdentifier";
NSString* const kTTCVoicePlateErrorBannerAccessibilityIdentifier =
    @"TTCVoicePlateErrorBannerAccessibilityIdentifier";
NSString* const kTTCVoicePlatePlayPauseButtonAccessibilityIdentifier =
    @"TTCVoicePlatePlayPauseButtonAccessibilityIdentifier";

namespace {

// Layout and dimension constants for the 3-element separated voice plate bar:
// [ 48x48 Play/Pause circle ] --14pt-- [ 136x56 Waveform pill ] --14pt-- [
// 48x48 Close circle ]
constexpr CGFloat kCenterPillWidth = 136.0;
constexpr CGFloat kCenterPillHeight = 56.0;
constexpr CGFloat kCenterPillCornerRadius = 28.0;
constexpr CGFloat kCircleButtonSize = 48.0;
constexpr CGFloat kCircleButtonCornerRadius = 24.0;
constexpr CGFloat kControlBarSpacing = 14.0;
constexpr CGFloat kActionSymbolPointSize = 18.0;
constexpr float kVoicePlateShadowOpacity = 0.12f;
constexpr CGFloat kVoicePlateShadowRadius = 8.0;
constexpr CGFloat kVoicePlateShadowOffsetY = 2.0;
constexpr CGFloat kBorderHairlineWidth = 0.5;
constexpr CGFloat kCloseButtonBackgroundAlpha = 0.18;
constexpr CGFloat kCloseButtonBorderAlpha = 0.35;
constexpr CGFloat kDisabledButtonAlpha = 0.45;

constexpr CGFloat kOuterStackSpacing = 8.0;
constexpr CGFloat kErrorChipPaddingHorizontal = 12.0;
constexpr CGFloat kErrorChipPaddingVertical = 4.0;
constexpr CGFloat kErrorChipCornerRadius = 8.0;

constexpr base::TimeDelta kErrorAutoDismissDelay = base::Seconds(4);

// UI strings.
NSString* const kCloseSessionAccessibilityLabel = @"Close session";
NSString* const kPauseSessionAccessibilityLabel = @"Pause session";
NSString* const kResumeSessionAccessibilityLabel = @"Resume session";
NSString* const kStartSessionAccessibilityLabel = @"Start session";

}  // namespace

@implementation TTCVoicePlateView {
  TTCSessionUIState _currentState;
  BOOL _isMicMuted;

  UIVisualEffectView* _blurEffectView;
  UILabel* _errorLabel;
  UIView* _errorChipContainer;
  UIButton* _playPauseButton;
  UIButton* _closeButton;
  TTCWaveformView* _waveformView;

  base::OneShotTimer _errorDismissTimer;
}

#pragma mark - Initialization

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _currentState = TTCSessionUIState::kIdle;

    // Transient error chip floating above the voice plate controls.
    _errorLabel = [[UILabel alloc] init];
    _errorLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _errorLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    _errorLabel.textColor = [UIColor colorNamed:kRedColor];
    _errorLabel.textAlignment = NSTextAlignmentCenter;
    _errorLabel.numberOfLines = 0;
    _errorLabel.accessibilityIdentifier =
        kTTCVoicePlateErrorBannerAccessibilityIdentifier;

    _errorChipContainer = [[UIView alloc] init];
    _errorChipContainer.translatesAutoresizingMaskIntoConstraints = NO;
    _errorChipContainer.backgroundColor =
        [UIColor colorNamed:kSecondaryBackgroundColor];
    _errorChipContainer.layer.cornerRadius = kErrorChipCornerRadius;
    _errorChipContainer.layer.masksToBounds = YES;
    _errorChipContainer.hidden = YES;
    [_errorChipContainer addSubview:_errorLabel];
    AddSameConstraintsWithInsets(
        _errorLabel, _errorChipContainer,
        NSDirectionalEdgeInsetsMake(
            kErrorChipPaddingVertical, kErrorChipPaddingHorizontal,
            kErrorChipPaddingVertical, kErrorChipPaddingHorizontal));

    // Left circular Session Pause/Resume button.
    _playPauseButton = [UIButton buttonWithType:UIButtonTypeSystem];
    _playPauseButton.translatesAutoresizingMaskIntoConstraints = NO;
    _playPauseButton.layer.cornerRadius = kCircleButtonCornerRadius;
    _playPauseButton.layer.masksToBounds = YES;
    _playPauseButton.accessibilityIdentifier =
        kTTCVoicePlatePlayPauseButtonAccessibilityIdentifier;
    [_playPauseButton addTarget:self
                         action:@selector(playPauseButtonTapped:)
               forControlEvents:UIControlEventTouchUpInside];

    // Right circular Session Start (Play) / Stop (Close) button.
    _closeButton = [UIButton buttonWithType:UIButtonTypeSystem];
    _closeButton.translatesAutoresizingMaskIntoConstraints = NO;
    _closeButton.layer.cornerRadius = kCircleButtonCornerRadius;
    _closeButton.layer.masksToBounds = YES;
    _closeButton.accessibilityIdentifier =
        kTTCVoicePlateCloseButtonAccessibilityIdentifier;
    [_closeButton addTarget:self
                     action:@selector(closeButtonTapped:)
           forControlEvents:UIControlEventTouchUpInside];

    AddSquareConstraints(_playPauseButton, kCircleButtonSize);
    AddSquareConstraints(_closeButton, kCircleButtonSize);

    // Center waveform visualization view inside the frosted-glass pill.
    _waveformView = [[TTCWaveformView alloc] init];
    _waveformView.translatesAutoresizingMaskIntoConstraints = NO;

    UIBlurEffect* blurEffect =
        [UIBlurEffect effectWithStyle:UIBlurEffectStyleSystemChromeMaterial];
    _blurEffectView = [[UIVisualEffectView alloc] initWithEffect:blurEffect];
    _blurEffectView.translatesAutoresizingMaskIntoConstraints = NO;
    _blurEffectView.layer.cornerRadius = kCenterPillCornerRadius;
    _blurEffectView.layer.masksToBounds = YES;
    _blurEffectView.layer.borderWidth = kBorderHairlineWidth;
    _blurEffectView.layer.borderColor =
        [[UIColor colorNamed:kSeparatorColor]
            resolvedColorWithTraitCollection:self.traitCollection]
            .CGColor;
    [_blurEffectView.contentView addSubview:_waveformView];

    // Center pill container hosting shadow and enclosing the blur effect view.
    UIView* pillContainer = [[UIView alloc] init];
    pillContainer.translatesAutoresizingMaskIntoConstraints = NO;
    pillContainer.layer.cornerRadius = kCenterPillCornerRadius;
    pillContainer.layer.masksToBounds = NO;
    pillContainer.layer.shadowColor =
        [UIColor colorNamed:kSolidBlackColor].CGColor;
    pillContainer.layer.shadowOpacity = kVoicePlateShadowOpacity;
    pillContainer.layer.shadowRadius = kVoicePlateShadowRadius;
    pillContainer.layer.shadowOffset =
        CGSizeMake(0.0, kVoicePlateShadowOffsetY);
    [pillContainer addSubview:_blurEffectView];
    AddSameConstraints(_blurEffectView, pillContainer);

    AddSizeConstraints(pillContainer,
                       CGSizeMake(kCenterPillWidth, kCenterPillHeight));
    AddSameCenterConstraints(_waveformView, _blurEffectView.contentView);

    // Horizontal control row: [ Play/Pause Circle ] [ Center Pill ] [ Close
    // Circle ].
    UIStackView* controlsStack =
        [[UIStackView alloc] initWithArrangedSubviews:@[
          _playPauseButton, pillContainer, _closeButton
        ]];
    controlsStack.translatesAutoresizingMaskIntoConstraints = NO;
    controlsStack.axis = UILayoutConstraintAxisHorizontal;
    controlsStack.alignment = UIStackViewAlignmentCenter;
    controlsStack.spacing = kControlBarSpacing;

    [self registerForTraitChanges:@[ [UITraitUserInterfaceStyle class] ]
                       withAction:@selector(updateBorderColors)];

    // Outer vertical stack containing the error chip and the 3-element control
    // row.
    UIStackView* outerStack = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _errorChipContainer, controlsStack ]];
    outerStack.translatesAutoresizingMaskIntoConstraints = NO;
    outerStack.axis = UILayoutConstraintAxisVertical;
    outerStack.alignment = UIStackViewAlignmentCenter;
    outerStack.spacing = kOuterStackSpacing;

    [self addSubview:outerStack];
    AddSameConstraints(outerStack, self);

    [self updateControlsForState:_currentState];
  }
  return self;
}

#pragma mark - TTCVoicePlateConsumer

- (void)setSessionState:(TTCSessionUIState)state {
  if (_currentState == state) {
    return;
  }
  _currentState = state;
  if (state != TTCSessionUIState::kListening) {
    _isMicMuted = NO;
  }
  if (state != TTCSessionUIState::kError) {
    [self dismissErrorBanner];
  }
  [self updateControlsForState:state];
}

- (void)setMicMuted:(BOOL)micMuted {
  if (_isMicMuted == micMuted) {
    return;
  }
  _isMicMuted = micMuted;
  [self updateControlsForState:_currentState];
}

- (void)setMicEnergyLevel:(float)rms {
  _waveformView.audioLevel = rms;
}

- (void)showErrorMessage:(NSString*)errorMessage {
  _errorLabel.text = errorMessage;
  _errorChipContainer.hidden = NO;
  UIAccessibilityPostNotification(UIAccessibilityAnnouncementNotification,
                                  _errorLabel.text);

  __weak TTCVoicePlateView* weakSelf = self;
  _errorDismissTimer.Start(FROM_HERE, kErrorAutoDismissDelay, base::BindOnce(^{
                             [weakSelf dismissErrorBanner];
                           }));
}

#pragma mark - Actions

- (void)playPauseButtonTapped:(UIButton*)sender {
  [self.mutator toggleMicMuted];
}

- (void)closeButtonTapped:(UIButton*)sender {
  switch (_currentState) {
    case TTCSessionUIState::kIdle:
    case TTCSessionUIState::kError:
      [self.mutator startSession];
      break;
    case TTCSessionUIState::kConnecting:
    case TTCSessionUIState::kListening:
      [self.mutator stopSession];
      break;
  }
}

#pragma mark - Private UI Helpers

// Updates the waveform state, pause/resume button, and session button for
// `state`.
- (void)updateControlsForState:(TTCSessionUIState)state {
  switch (state) {
    case TTCSessionUIState::kIdle:
    case TTCSessionUIState::kError:
      _waveformView.state = TTCWaveformState::kDots;
      [self configurePlayPauseButtonEnabled:NO isPaused:NO];
      [self configureSessionButtonForActiveSession:NO];
      break;

    case TTCSessionUIState::kConnecting:
      _waveformView.state = TTCWaveformState::kProcessing;
      [self configurePlayPauseButtonEnabled:NO isPaused:NO];
      [self configureSessionButtonForActiveSession:YES];
      break;

    case TTCSessionUIState::kListening:
      _waveformView.state =
          _isMicMuted ? TTCWaveformState::kDots : TTCWaveformState::kListening;
      [self configurePlayPauseButtonEnabled:YES isPaused:_isMicMuted];
      [self configureSessionButtonForActiveSession:YES];
      break;
  }
}

// Configures `_playPauseButton` interactivity, icon, accessibility label, and
// surface styling for `enabled` and `isPaused`.
- (void)configurePlayPauseButtonEnabled:(BOOL)enabled isPaused:(BOOL)isPaused {
  _playPauseButton.enabled = enabled;
  _playPauseButton.alpha = enabled ? 1.0 : kDisabledButtonAlpha;
  _playPauseButton.accessibilityLabel = isPaused
                                            ? kResumeSessionAccessibilityLabel
                                            : kPauseSessionAccessibilityLabel;
  Symbol symbol = isPaused ? SymbolMicrophoneSlash : SymbolMicrophoneFill;
  [_playPauseButton setImage:SymbolWithPointSize(symbol, kActionSymbolPointSize)
                    forState:UIControlStateNormal];
  if (isPaused) {
    _playPauseButton.backgroundColor = [UIColor colorNamed:kBlueColor];
    _playPauseButton.tintColor = [UIColor colorNamed:kSolidButtonTextColor];
    _playPauseButton.layer.borderWidth = 0.0;
    _playPauseButton.layer.borderColor = nil;
  } else {
    _playPauseButton.backgroundColor =
        [UIColor colorNamed:kTertiaryBackgroundColor];
    _playPauseButton.tintColor = [UIColor colorNamed:kTextPrimaryColor];
    _playPauseButton.layer.borderWidth = kBorderHairlineWidth;
    _playPauseButton.layer.borderColor =
        [[UIColor colorNamed:kSeparatorColor]
            resolvedColorWithTraitCollection:self.traitCollection]
            .CGColor;
  }
}

// Configures `_closeButton` (right session button) as a red Close button when
// `isSessionActive` is YES, or an accent-blue Play (Start session) button when
// `isSessionActive` is NO.
- (void)configureSessionButtonForActiveSession:(BOOL)isSessionActive {
  if (isSessionActive) {
    UIColor* redColor = [UIColor colorNamed:kRedColor];
    _closeButton.accessibilityLabel = kCloseSessionAccessibilityLabel;
    [_closeButton
        setImage:SymbolWithPointSize(SymbolXMark, kActionSymbolPointSize)
        forState:UIControlStateNormal];
    _closeButton.backgroundColor =
        [redColor colorWithAlphaComponent:kCloseButtonBackgroundAlpha];
    _closeButton.tintColor = redColor;
    _closeButton.layer.borderWidth = kBorderHairlineWidth;
    _closeButton.layer.borderColor =
        [[redColor colorWithAlphaComponent:kCloseButtonBorderAlpha]
            resolvedColorWithTraitCollection:self.traitCollection]
            .CGColor;
  } else {
    _closeButton.accessibilityLabel = kStartSessionAccessibilityLabel;
    [_closeButton
        setImage:SymbolWithPointSize(SymbolPlayFill, kActionSymbolPointSize)
        forState:UIControlStateNormal];
    _closeButton.backgroundColor = [UIColor colorNamed:kBlueColor];
    _closeButton.tintColor = [UIColor colorNamed:kSolidButtonTextColor];
    _closeButton.layer.borderWidth = 0.0;
    _closeButton.layer.borderColor = nil;
  }
}

// Stops the auto-dismiss timer and hides the transient error chip.
- (void)dismissErrorBanner {
  _errorDismissTimer.Stop();
  _errorLabel.text = nil;
  _errorChipContainer.hidden = YES;
}

// Refreshes CGColor border colors when the user interface style changes.
- (void)updateBorderColors {
  _blurEffectView.layer.borderColor =
      [[UIColor colorNamed:kSeparatorColor]
          resolvedColorWithTraitCollection:self.traitCollection]
          .CGColor;
  if (_closeButton.layer.borderWidth > 0.0) {
    _closeButton.layer.borderColor =
        [[[UIColor colorNamed:kRedColor]
             colorWithAlphaComponent:kCloseButtonBorderAlpha]
            resolvedColorWithTraitCollection:self.traitCollection]
            .CGColor;
  }
  if (_playPauseButton.layer.borderWidth > 0.0) {
    _playPauseButton.layer.borderColor =
        [[UIColor colorNamed:kSeparatorColor]
            resolvedColorWithTraitCollection:self.traitCollection]
            .CGColor;
  }
}

@end
