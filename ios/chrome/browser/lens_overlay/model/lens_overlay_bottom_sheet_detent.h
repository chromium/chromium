// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_BOTTOM_SHEET_DETENT_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_BOTTOM_SHEET_DETENT_H_

#import <UIKit/UIKit.h>

// Custom detent implementation for the Lens Overlay bottom sheet.
@interface LensOverlayBottomSheetDetent : NSObject

// The identifier of the detent.
@property(nonatomic, readonly, copy) NSString* identifier;

// The resolved value for the detent.
@property(nonatomic, readonly) CGFloat value;

// Creates a new detent with a given identifier and a block providing a float
// value for the detent.
- (instancetype)initWithIdentifier:(NSString*)identifier
                     valueResolver:(CGFloat (^)())value;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_BOTTOM_SHEET_DETENT_H_
