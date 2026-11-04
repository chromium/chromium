// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_UI_TTC_WAVEFORM_VIEW_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_UI_TTC_WAVEFORM_VIEW_H_

#import <UIKit/UIKit.h>

// Accessibility identifier for `TTCWaveformView`.
extern NSString* const kTTCWaveformViewAccessibilityIdentifier;

// Visual states for `TTCWaveformView`.
enum class TTCWaveformState {
  // Static 9-dot baseline for idle, disconnected, or error states.
  kDots,
  // Continuous traveling cosine wave packet across resting dots.
  kProcessing,
  // Live vertical audio bars driven by microphone volume.
  kListening,
};

// Custom `UIView` that renders animated audio waveform feedback matching
// Desktop TTC waveform physics.
@interface TTCWaveformView : UIView

// Current visual state of the waveform.
@property(nonatomic, assign) TTCWaveformState state;

// Current normalized audio energy level in the range [0.0, 1.0].
@property(nonatomic, assign) float audioLevel;

// Initializes the view with designated frame.
- (instancetype)initWithFrame:(CGRect)frame NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_UI_TTC_WAVEFORM_VIEW_H_
