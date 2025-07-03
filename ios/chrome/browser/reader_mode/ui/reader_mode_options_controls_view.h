// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_UI_READER_MODE_OPTIONS_CONTROLS_VIEW_H_
#define IOS_CHROME_BROWSER_READER_MODE_UI_READER_MODE_OPTIONS_CONTROLS_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/reader_mode/ui/reader_mode_options_consumer.h"

@protocol ReaderModeOptionsMutator;

// A view that contains the controls for the Reader Mode options.
@interface ReaderModeOptionsControlsView : UIView <ReaderModeOptionsConsumer>

@property(nonatomic, weak) id<ReaderModeOptionsMutator> mutator;

@end

#endif  // IOS_CHROME_BROWSER_READER_MODE_UI_READER_MODE_OPTIONS_CONTROLS_VIEW_H_
