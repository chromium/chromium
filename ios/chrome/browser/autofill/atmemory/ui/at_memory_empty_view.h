// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_EMPTY_VIEW_H_
#define IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_EMPTY_VIEW_H_

#import <UIKit/UIKit.h>

// Custom zero-state empty view.
// Anchors the image vertically so changing message lengths do not shift the
// image.
@interface AtMemoryEmptyView : UIView

// Accessibility label describing the empty view for VoiceOver.
@property(nonatomic, copy) NSString* viewAccessibilityLabel;

// Initializes the empty view with `frame`, `image`, and `message`.
- (instancetype)initWithFrame:(CGRect)frame
                        image:(UIImage*)image
                      message:(NSString*)message NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

// Updates the displayed `message` without moving the image or rebuilding views.
- (void)updateMessage:(NSString*)message;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_EMPTY_VIEW_H_
