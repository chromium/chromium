// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_SHARED_COORDINATOR_COMPOSEBOX_PICKER_PRESENTER_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_SHARED_COORDINATOR_COMPOSEBOX_PICKER_PRESENTER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/composebox/shared/coordinator/composebox_picker_image_result.h"

@class ComposeboxPickerPresenter;

/// Delegate for various picker events.
@protocol ComposeboxPickerPresenterDelegate

// Called when the camera or gallery picker finishes picking an image.
- (void)composeboxPickerPresenter:(ComposeboxPickerPresenter*)presenter
                    didPickImages:
                        (NSArray<ComposeboxPickerImageResult*>*)results;

/// Called when the camera picker presentation dismisses.
- (void)composeboxPickerPresenterDidDissmissCamera:
    (ComposeboxPickerPresenter*)presenter;

@end

/// Presents the various composebox pickers.
@interface ComposeboxPickerPresenter : NSObject

/// Delegate for this class.
@property(nonatomic, weak) id<ComposeboxPickerPresenterDelegate> delegate;

// Creates a new object of this type.
- (instancetype)initWithBaseViewController:
    (UIViewController*)baseViewController;

// Presents the camera picker.
- (void)presentCameraPicker;

// Presents the gallery picker.
- (void)presentGalleryPickerWithLimit:(NSUInteger)limit;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_SHARED_COORDINATOR_COMPOSEBOX_PICKER_PRESENTER_H_
