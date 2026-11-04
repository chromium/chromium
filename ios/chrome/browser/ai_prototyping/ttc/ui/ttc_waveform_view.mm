// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/ui/ttc_waveform_view.h"

#import <algorithm>
#import <cmath>
#import <numbers>
#import <optional>
#import <vector>

#import "base/time/time.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

NSString* const kTTCWaveformViewAccessibilityIdentifier =
    @"TTCWaveformViewAccessibilityIdentifier";

namespace {

// Layout and geometry constants matching Desktop TTC full-size waveform.
constexpr size_t kBarCount = 9;
constexpr size_t kCenterBarIndex = kBarCount / 2;
constexpr CGFloat kBarWidth = 2.0;
constexpr CGFloat kBarSpacing = 6.0;
constexpr CGFloat kBarCornerRadius = 0.5;
constexpr CGFloat kDotSize = 2.0;

constexpr CGFloat kMinBarHeight = 4.0;
constexpr CGFloat kMaxBarHeight = 20.0;

constexpr CGFloat kDefaultViewWidth = 63.0;
constexpr CGFloat kDefaultViewHeight = 24.0;

// Processing wave parameters matching Desktop.
constexpr base::TimeDelta kProcessingWaveTravelDuration =
    base::Milliseconds(700);
constexpr base::TimeDelta kProcessingWavePauseDuration = base::Milliseconds(50);
constexpr base::TimeDelta kProcessingWavePeriod =
    kProcessingWaveTravelDuration + kProcessingWavePauseDuration;
constexpr CGFloat kProcessingWaveAmplitude = 5.0;
constexpr CGFloat kProcessingHighlightDotSize = 3.5;
constexpr double kProcessingPacketHalfWidth = 2.5;

// Audio response constants matching Desktop.
constexpr base::TimeDelta kHistoryStepInterval = base::Milliseconds(45);
constexpr float kSilenceThreshold = 0.005f;
constexpr float kAudioSensitivity = 8.0f;
constexpr float kInflectionInput = 0.75f;
constexpr float kInflectionOutputPercent = 0.75f;
constexpr float kBubblingSpeed = 0.40f;

// Maps `value` from [`from_min`, `from_max`] to [`to_min`, `to_max`].
float MapRange(float value,
               float from_min,
               float from_max,
               float to_min,
               float to_max) {
  const float input_range = from_max - from_min;
  if (input_range <= 0.0f) {
    return to_min;
  }
  const float progress = (value - from_min) / input_range;
  return to_min + progress * (to_max - to_min);
}

// Calculates the target peak bar height based on normalized raw audio level.
CGFloat CalculateBarHeight(CGFloat raw_level) {
  if (raw_level <= 0.0) {
    return kMinBarHeight;
  }
  const float normalized_level = std::sqrt(std::clamp(
      static_cast<float>(raw_level) * kAudioSensitivity, 0.0f, 1.0f));
  if (normalized_level <= kSilenceThreshold) {
    return kMinBarHeight;
  }
  const float inflection_output =
      static_cast<float>(kMaxBarHeight) * kInflectionOutputPercent;
  if (normalized_level < kInflectionInput) {
    return MapRange(normalized_level, kSilenceThreshold, kInflectionInput,
                    static_cast<float>(kMinBarHeight), inflection_output);
  }
  return MapRange(normalized_level, kInflectionInput, 1.0f, inflection_output,
                  static_cast<float>(kMaxBarHeight));
}

// Vertical center position and diameter for a single dot during `kProcessing`.
struct ProcessingDotState {
  CGFloat center_y = 0.0;
  CGFloat size = 0.0;
};

// Computes the visual state for dot `index` during `kProcessing`.
ProcessingDotState GetProcessingState(size_t index,
                                      base::TimeTicks now,
                                      base::TimeTicks start_time,
                                      CGFloat baseline_y) {
  if (start_time.is_null()) {
    return {baseline_y, kDotSize};
  }
  const base::TimeDelta elapsed = now - start_time;
  const base::TimeDelta cycle_time = elapsed % kProcessingWavePeriod;

  double wave_weight = 0.0;
  if (cycle_time < kProcessingWaveTravelDuration) {
    const double progress =
        static_cast<double>(cycle_time.InMicroseconds()) /
        static_cast<double>(kProcessingWaveTravelDuration.InMicroseconds());
    const double crest_pos =
        -kProcessingPacketHalfWidth +
        progress * (kBarCount - 1 + 2.0 * kProcessingPacketHalfWidth);
    const double dist = std::abs(static_cast<double>(index) - crest_pos);
    if (dist < kProcessingPacketHalfWidth) {
      wave_weight = 0.5 * (1.0 + std::cos(std::numbers::pi_v<double> * dist /
                                          kProcessingPacketHalfWidth));
    }
  }

  const CGFloat weight = static_cast<CGFloat>(wave_weight);
  return {baseline_y - kProcessingWaveAmplitude * weight,
          std::lerp(kDotSize, kProcessingHighlightDotSize, weight)};
}

}  // namespace

// Weak-referencing proxy target for `CADisplayLink` to avoid a retain cycle
// between `TTCWaveformView` and its display link.
@interface TTCWaveformDisplayLinkProxy : NSObject
- (instancetype)initWithTarget:(TTCWaveformView*)target
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;
- (void)displayLinkDidFire:(CADisplayLink*)link;
@end

@implementation TTCWaveformView {
  base::TimeTicks _processingStartTime;
  base::TimeTicks _lastPhysicsTime;
  std::vector<CGFloat> _currentBarHeights;

  // Audio level history queue for center-outward propagation.
  std::vector<CGFloat> _audioHistory;
  base::TimeDelta _historyTimer;
  std::optional<CGFloat> _activeVolume;
  CGFloat _lastVolume;
  BOOL _previousReadEmpty;

  CADisplayLink* _displayLink;
  TTCWaveformDisplayLinkProxy* _displayLinkProxy;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _state = TTCWaveformState::kDots;
    _audioLevel = 0.0;
    _currentBarHeights.assign(kBarCount, kMinBarHeight);
    _audioHistory.assign(kCenterBarIndex + 1, 0.0);
    _historyTimer = base::TimeDelta();
    _lastVolume = 0.0;
    _previousReadEmpty = NO;

    self.backgroundColor = [UIColor clearColor];
    self.accessibilityIdentifier = kTTCWaveformViewAccessibilityIdentifier;
    self.userInteractionEnabled = NO;

    _displayLinkProxy =
        [[TTCWaveformDisplayLinkProxy alloc] initWithTarget:self];
  }
  return self;
}

- (CGSize)intrinsicContentSize {
  return CGSizeMake(kDefaultViewWidth, kDefaultViewHeight);
}

- (void)didMoveToWindow {
  [super didMoveToWindow];
  if (!self.window) {
    [self stopDisplayLink];
    return;
  }
  if (self.state != TTCWaveformState::kDots) {
    [self startDisplayLinkIfNeeded];
  }
}

#pragma mark - Properties

- (void)setState:(TTCWaveformState)state {
  if (_state == state) {
    return;
  }
  _state = state;

  if (_state == TTCWaveformState::kProcessing) {
    _processingStartTime = base::TimeTicks::Now();
  } else {
    _processingStartTime = base::TimeTicks();
  }

  if (_state != TTCWaveformState::kListening) {
    _currentBarHeights.assign(kBarCount, kMinBarHeight);
    _audioHistory.assign(kCenterBarIndex + 1, 0.0);
    _historyTimer = base::TimeDelta();
    _activeVolume = std::nullopt;
    _lastVolume = 0.0;
    _previousReadEmpty = NO;
  }

  if (_state == TTCWaveformState::kDots) {
    [self stopDisplayLink];
  } else {
    [self startDisplayLinkIfNeeded];
  }

  [self setNeedsDisplay];
}

- (void)setAudioLevel:(float)audioLevel {
  _audioLevel = std::clamp(audioLevel, 0.0f, 1.0f);
  _activeVolume = _audioLevel;
}

#pragma mark - Display Link Lifecycle

// Advances waveform physics on each display frame and requests a redraw.
- (void)displayLinkDidFire:(CADisplayLink*)link {
  const base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta delta = _lastPhysicsTime.is_null()
                              ? base::Seconds(link.duration)
                              : (now - _lastPhysicsTime);
  _lastPhysicsTime = now;

  [self stepPhysicsWithDelta:delta];
  [self setNeedsDisplay];
}

// Starts the display link when attached to a window in an animated state.
- (void)startDisplayLinkIfNeeded {
  if (!self.window || self.state == TTCWaveformState::kDots) {
    return;
  }
  if (!_displayLink) {
    _displayLink =
        [CADisplayLink displayLinkWithTarget:_displayLinkProxy
                                    selector:@selector(displayLinkDidFire:)];
    [_displayLink addToRunLoop:[NSRunLoop mainRunLoop]
                       forMode:NSRunLoopCommonModes];
  }
  _lastPhysicsTime = base::TimeTicks::Now();
}

// Tears down and invalidates the display link.
- (void)stopDisplayLink {
  [_displayLink invalidate];
  _displayLink = nil;
}

#pragma mark - Drawing

- (void)drawRect:(CGRect)rect {
  CGContextRef context = UIGraphicsGetCurrentContext();
  if (!context) {
    return;
  }

  UIColor* fillColor = [UIColor colorNamed:kTextPrimaryColor];
  [fillColor setFill];

  const CGRect bounds = self.bounds;
  const CGFloat totalBarsWidth = (kBarCount - 1) * kBarSpacing + kBarWidth;
  const CGFloat startX = (CGRectGetWidth(bounds) - totalBarsWidth) / 2.0;
  const CGFloat baselineY = CGRectGetHeight(bounds) / 2.0;

  if (self.state == TTCWaveformState::kDots) {
    for (size_t i = 0; i < kBarCount; ++i) {
      const CGFloat dotX = startX + i * kBarSpacing;
      const CGRect dotRect =
          CGRectMake(dotX, baselineY - kDotSize / 2.0, kDotSize, kDotSize);
      CGContextFillEllipseInRect(context, dotRect);
    }
    return;
  }

  if (self.state == TTCWaveformState::kProcessing) {
    const base::TimeTicks now = base::TimeTicks::Now();
    for (size_t i = 0; i < kBarCount; ++i) {
      const CGFloat dotX = startX + i * kBarSpacing;
      const ProcessingDotState dotState =
          GetProcessingState(i, now, _processingStartTime, baselineY);
      // Expand swollen dot symmetrically around its resting horizontal center.
      const CGRect dotRect =
          CGRectMake(dotX - (dotState.size - kBarWidth) / 2.0,
                     dotState.center_y - dotState.size / 2.0, dotState.size,
                     dotState.size);
      CGContextFillEllipseInRect(context, dotRect);
    }
    return;
  }

  for (size_t i = 0; i < kBarCount; ++i) {
    const CGFloat barX = startX + i * kBarSpacing;
    const CGFloat barH = _currentBarHeights[i];
    const CGRect barRect =
        CGRectMake(barX, baselineY - barH / 2.0, kBarWidth, barH);
    UIBezierPath* path =
        [UIBezierPath bezierPathWithRoundedRect:barRect
                                   cornerRadius:kBarCornerRadius];
    [path fill];
  }
}

// Advances listening bar heights and audio history by `delta`.
- (void)stepPhysicsWithDelta:(base::TimeDelta)delta {
  if (self.state != TTCWaveformState::kListening) {
    return;
  }

  _historyTimer += delta;
  if (_historyTimer >= kHistoryStepInterval) {
    _historyTimer -= kHistoryStepInterval;
    for (size_t i = _audioHistory.size() - 1; i > 0; --i) {
      _audioHistory[i] = _audioHistory[i - 1];
    }
    if (_activeVolume.has_value()) {
      _previousReadEmpty = NO;
      _lastVolume = _activeVolume.value();
      _audioHistory[0] = _activeVolume.value();
      _activeVolume = std::nullopt;
    } else if (_previousReadEmpty) {
      _audioHistory[0] = 0.0;
    } else {
      // Hold the previous sample across a single empty interval to smooth
      // callback jitter before decaying to silence on consecutive misses.
      _audioHistory[0] = _lastVolume;
      _previousReadEmpty = YES;
    }
  }

  for (size_t i = 0; i < kBarCount; ++i) {
    const size_t dist =
        (i > kCenterBarIndex) ? (i - kCenterBarIndex) : (kCenterBarIndex - i);
    const CGFloat rawLevel = _audioHistory[dist];
    const CGFloat rawHeight = CalculateBarHeight(rawLevel);
    const CGFloat taper =
        1.0 - (static_cast<CGFloat>(dist) / (kCenterBarIndex + 1.0));
    const CGFloat targetHeight = std::max(kMinBarHeight, rawHeight * taper);

    _currentBarHeights[i] +=
        (targetHeight - _currentBarHeights[i]) * kBubblingSpeed;
    _currentBarHeights[i] =
        std::clamp(_currentBarHeights[i], kMinBarHeight, kMaxBarHeight);
  }
}

@end

@implementation TTCWaveformDisplayLinkProxy {
  __weak TTCWaveformView* _target;
}

- (instancetype)initWithTarget:(TTCWaveformView*)target {
  self = [super init];
  if (self) {
    _target = target;
  }
  return self;
}

- (void)displayLinkDidFire:(CADisplayLink*)link {
  TTCWaveformView* target = _target;
  if (!target) {
    [link invalidate];
    return;
  }
  [target displayLinkDidFire:link];
}

@end
