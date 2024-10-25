// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_COORDINATOR_FAKE_CHROME_LENS_OVERLAY_RESULT_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_COORDINATOR_FAKE_CHROME_LENS_OVERLAY_RESULT_H_

#import <UIKit/UIKit.h>

#import "ios/public/provider/chrome/browser/lens/lens_overlay_result.h"

/// ChromeLensOverlayResult test object.
@interface FakeChromeLensOverlayResult : NSObject <ChromeLensOverlayResult>

/// The result URL that is meant to be loaded in the LRP.
@property(nonatomic, assign) GURL searchResultURL;
/// The selected portion of the original snapshot.
@property(nonatomic, strong) UIImage* selectionPreviewImage;
/// Data containing the suggest signals.
@property(nonatomic, strong) NSData* suggestSignals;
/// Query text.
@property(nonatomic, copy) NSString* queryText;
/// Whether the result represents a text selection.
@property(nonatomic, assign) BOOL isTextSelection;
/// The selection rect of the lens region.
@property(nonatomic, assign) CGRect selectionRect;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_COORDINATOR_FAKE_CHROME_LENS_OVERLAY_RESULT_H_
