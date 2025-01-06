// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_CHROME_LENS_OVERLAY_RESULT_IMPL_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_CHROME_LENS_OVERLAY_RESULT_IMPL_H_

#import "ios/public/provider/chrome/browser/lens/lens_overlay_result.h"

/// A simple implementation for ChromeLensOverlayResult that can be instantiated
/// with static data.
/// @discussion normally, ChromeLensOverlayResults are vended by the Lens SDK.
/// However, sometimes it is convenient to create instances of a result without
/// Lens SDK. When using ChromeLensOverlayResultImpl, careful to not pass them
/// to Lens SDK by accident, as it expects instances of their internal
/// implementation of this protocol.
@interface ChromeLensOverlayResultImpl : NSObject <ChromeLensOverlayResult>

- (instancetype)initWithResultURL:(GURL)searchResultURL
                     previewImage:(UIImage*)previewImage
                   suggestSignals:(NSData*)suggestSignals
                  isTextSelection:(BOOL)isTextSelection
                        queryText:(NSString*)queryText
                    selectionRect:(CGRect)selectionRect
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_CHROME_LENS_OVERLAY_RESULT_IMPL_H_
