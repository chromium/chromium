// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_LENS_LENS_OVERLAY_RESULT_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_LENS_LENS_OVERLAY_RESULT_H_

#import <UIKit/UIKit.h>

class GURL;

@protocol ChromeLensOverlayResult <NSObject>

// The result URL that is meant to be loaded in the LRP.
@property(nonatomic, assign, readonly) GURL searchResultURL;

// The results HTTP headers that are meant to be loaded in the LRP.
@property(nonatomic, readonly, copy)
    NSDictionary<NSString*, NSString*>* resultsHttpHeaders;

// The selected portion of the original snapshot.
@property(nonatomic, readonly) UIImage* selectionPreviewImage;

// Data containing the suggest signals.
@property(nonatomic, readonly) NSData* suggestSignals;

// Whether the result represents a text selection.
@property(nonatomic, readonly) BOOL isTextSelection;

// Whether the result was generated in the translate filter in Lens.
@property(nonatomic, readonly) BOOL isGeneratedInTranslate;

// The text selection of the result or `nil` if the result is not a text
// selection.
@property(nonatomic, readonly, copy) NSString* queryText;

// The selection rect of the lens region.
@property(nonatomic, readonly) CGRect selectionRect;

/// Called when the result is successfully loaded in the webview.
- (void)resultSuccessfullyLoadedInWebView;

/// Called when the result loading is cancelled in the webview.
- (void)resultLoadingCancelledInWebView;

/// Called when the result fails to load in the webview.
- (void)resultFailedToLoadInWebViewWithError:(NSError*)error;

/// Called when the result webview is shown.
- (void)resultWebviewShown;

/// Called when the result webview is swiped with the given direction.
- (void)resultWebviewSwipedWithDirection:
    (UISwipeGestureRecognizerDirection)direction;

@end

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_LENS_LENS_OVERLAY_RESULT_H_
