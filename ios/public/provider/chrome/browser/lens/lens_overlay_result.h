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

// The selected portion of the original snapshot.
@property(nonatomic, readonly) UIImage* selectionPreviewImage;

// Data containing the suggest signals.
@property(nonatomic, readonly) NSData* suggestSignals;

// Whether the result represents a text selection.
@property(nonatomic, readonly) BOOL isTextSelection;

// The text selection of the result or `nil` if the result is not a text
// selection.
@property(nonatomic, readonly, copy) NSString* queryText;

@end

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_LENS_LENS_OVERLAY_RESULT_H_
