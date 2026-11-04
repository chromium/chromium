// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/ui/ttc_voice_plate_view.h"

#import "ios/chrome/browser/ai_prototyping/ttc/ui/ttc_voice_plate_consumer.h"
#import "ios/chrome/browser/ai_prototyping/ttc/ui/ttc_waveform_view.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// Layout and dimension constants for the central frosted-glass waveform pill.
constexpr CGFloat kCenterPillWidth = 136.0;
constexpr CGFloat kCenterPillHeight = 56.0;
constexpr CGFloat kCenterPillCornerRadius = 28.0;
constexpr float kVoicePlateShadowOpacity = 0.12f;
constexpr CGFloat kVoicePlateShadowRadius = 8.0;
constexpr CGFloat kVoicePlateShadowOffsetY = 2.0;
constexpr CGFloat kBorderHairlineWidth = 0.5;

}  // namespace

@implementation TTCVoicePlateView {
  TTCSessionUIState _currentState;
  BOOL _isMicMuted;

  UIVisualEffectView* _blurEffectView;
  TTCWaveformView* _waveformView;
}

#pragma mark - Initialization

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _currentState = TTCSessionUIState::kIdle;

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

    [self addSubview:pillContainer];
    AddSameConstraints(pillContainer, self);

    AddSizeConstraints(pillContainer,
                       CGSizeMake(kCenterPillWidth, kCenterPillHeight));
    AddSameCenterConstraints(_waveformView, _blurEffectView.contentView);

    [self registerForTraitChanges:@[ [UITraitUserInterfaceStyle class] ]
                       withAction:@selector(updateBorderColors)];

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
  // Implemented in follow-up error chip slice.
}

#pragma mark - Private UI Helpers

// Updates the waveform state for `state`.
- (void)updateControlsForState:(TTCSessionUIState)state {
  switch (state) {
    case TTCSessionUIState::kIdle:
    case TTCSessionUIState::kError:
      _waveformView.state = TTCWaveformState::kDots;
      break;

    case TTCSessionUIState::kConnecting:
      _waveformView.state = TTCWaveformState::kProcessing;
      break;

    case TTCSessionUIState::kListening:
      _waveformView.state =
          _isMicMuted ? TTCWaveformState::kDots : TTCWaveformState::kListening;
      break;
  }
}

// Refreshes CGColor border colors when the user interface style changes.
- (void)updateBorderColors {
  _blurEffectView.layer.borderColor =
      [[UIColor colorNamed:kSeparatorColor]
          resolvedColorWithTraitCollection:self.traitCollection]
          .CGColor;
}

@end
