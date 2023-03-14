// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_UTIL_TRANSPARENT_LINK_BUTTON_H_
#define IOS_CHROME_BROWSER_SHARED_UI_UTIL_TRANSPARENT_LINK_BUTTON_H_

#import <UIKit/UIKit.h>

class GURL;

// Minimum height and width for a link's tappable area.  On touch-down events,
// the portion of the button directly on top of the link text will be
// highlighted with a gray overlay.
extern const CGFloat kLinkTapAreaMinimum;

// Transparent button that is overlaid on link portions of text.
@interface TransparentLinkButton : UIButton

// TransparentLinkButtons must be created via |+buttonsForLinkFrames:URL:`.
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;

// Creates TransparentLinkButtons with `URL` for each NSValue-wrapped CGRect in
// `linkFrames` and returns them in an NSArray.  The links returned by this
// function will be styled such that their touch-down styling will be shared.
// Only the first button in the array will be accessible, and `label` will be
// set as its accessibility label. This is done to ensure that VoiceOver mode
// does not have multiple accessibility elements with the same accessibility
// label and the same action.  `lineHeight` is used to limit the overlap from
// increasing the TransparentLinkButtons to satisfy a11y guidelines for a
// minimum touch area.
+ (NSArray*)buttonsForLinkFrames:(NSArray*)linkFrames
                             URL:(const GURL&)URL
                      lineHeight:(CGFloat)lineHeight
              accessibilityLabel:(NSString*)label
                 accessibilityID:(NSString*)accessibilityID;

// The URL passed upon initialization.
@property(nonatomic, readonly) GURL URL;

// If set to YES, updates the button's background to a semi-opaque color to
// verify the button's location over the text.  The default is NO.
@property(nonatomic, assign, getter=isDebug) BOOL debug;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_UTIL_TRANSPARENT_LINK_BUTTON_H_
