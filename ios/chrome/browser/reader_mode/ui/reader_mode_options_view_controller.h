// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_UI_READER_MODE_OPTIONS_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_READER_MODE_UI_READER_MODE_OPTIONS_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@class ReaderModeOptionsControlsView;
@protocol ReaderModeOptionsCommands;
@protocol ReaderModeOptionsMutator;

// View controller for the reader mode options.
@interface ReaderModeOptionsViewController : UIViewController

// The view that contains the controls for the Reader Mode options.
@property(nonatomic, readonly) ReaderModeOptionsControlsView* controlsView;

@property(nonatomic, weak) id<ReaderModeOptionsMutator> mutator;

@property(nonatomic, weak) id<ReaderModeOptionsCommands>
    readerModeOptionsHandler;

// Updates the visibility of the "Hide Reader mode" button.
// This button is visible by default.
- (void)updateHideReaderModeButtonVisibility:(BOOL)visible;

// Returns the appropriate detent value for a sheet presentation in `context`.
- (CGFloat)resolveDetentValueForSheetPresentation:
    (id<UISheetPresentationControllerDetentResolutionContext>)context;

@end

#endif  // IOS_CHROME_BROWSER_READER_MODE_UI_READER_MODE_OPTIONS_VIEW_CONTROLLER_H_
