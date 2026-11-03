// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTOR_INPUT_PLATE_VIEW_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTOR_INPUT_PLATE_VIEW_H_

#import <UIKit/UIKit.h>

@class ActorInputPlateView;

// Delegate notified of user actions on an `ActorInputPlateView`.
@protocol ActorInputPlateViewDelegate <NSObject>

// Called when the user submits non-whitespace `text` (trimmed of leading and
// trailing whitespace). The plate's text is cleared before this is called.
- (void)actorInputPlateView:(ActorInputPlateView*)inputPlateView
                didSendText:(NSString*)text;

@end

// A pill-shaped, single-line text input with a trailing send button. Overflow
// scrolls horizontally and never overlaps the button.
@interface ActorInputPlateView : UIView

// The delegate notified when text is sent.
@property(nonatomic, weak) id<ActorInputPlateViewDelegate> delegate;

// The text currently typed by the user.
@property(nonatomic, copy, readonly) NSString* text;

// Designated initializer.
- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTOR_INPUT_PLATE_VIEW_H_
