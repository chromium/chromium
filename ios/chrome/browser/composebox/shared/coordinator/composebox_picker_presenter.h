// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_SHARED_COORDINATOR_COMPOSEBOX_PICKER_PRESENTER_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_SHARED_COORDINATOR_COMPOSEBOX_PICKER_PRESENTER_H_

#import <UIKit/UIKit.h>

#import <set>

#import "ios/chrome/browser/composebox/shared/coordinator/composebox_picker_image_result.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_id.h"

@class ComposeboxPickerPresenter;

/// Delegate for various picker events.
@protocol ComposeboxPickerPresenterDelegate

// Called when the camera or gallery picker finishes picking an image.
- (void)composeboxPickerPresenter:(ComposeboxPickerPresenter*)presenter
                    didPickImages:
                        (NSArray<ComposeboxPickerImageResult*>*)results;

// Called when the document picker finishes picking an image.
- (void)composeboxPickerPresenter:(ComposeboxPickerPresenter*)presenter
             didPickFilesWithURLs:(NSArray<NSURL*>*)urls;

/// Called when the camera picker presentation dismisses.
- (void)composeboxPickerPresenterDidDissmissCamera:
    (ComposeboxPickerPresenter*)presenter;

// Called when the tab picker finishes picking tabs.
- (void)composeboxPickerPresenter:(ComposeboxPickerPresenter*)presenter
    handleSelectedTabsWithWebStateIDs:
        (std::set<web::WebStateID>)selectedWebStateIDs
                    cachedWebStateIDs:
                        (std::set<web::WebStateID>)cachedWebStateIDs;

@end

/// Presents the various composebox pickers.
@interface ComposeboxPickerPresenter : NSObject

/// Delegate for this class.
@property(nonatomic, weak) id<ComposeboxPickerPresenterDelegate> delegate;

// Creates a new object of this type.
- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser;

// Presents the camera picker.
- (void)presentCameraPicker;

// Presents the gallery picker.
- (void)presentGalleryPickerWithLimit:(NSUInteger)limit;

// Presents the tab picker.
- (void)presentTabPicker;

// Presents the file picker.
- (void)presentFilePicker;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_SHARED_COORDINATOR_COMPOSEBOX_PICKER_PRESENTER_H_
