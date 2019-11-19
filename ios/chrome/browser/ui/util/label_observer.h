// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_UTIL_LABEL_OBSERVER_H_
#define IOS_CHROME_BROWSER_UI_UTIL_LABEL_OBSERVER_H_

#import <UIKit/UIKit.h>

// Class that observes changes to UILabel properties via KVO.  This allows
// various classes that manage a UILabel's style via NSAttributedStrings to
// reapply their styling in response to property changes that would invalidate
// the attributed text.  This class also synchronizes notifications of style
// invalidation so that property changes that occur as the result of a
// LabelObserverAction do not trigger other actions.
@interface LabelObserver : NSObject

// Returns the LabelObserver for |label|, laziliy instantiating one if
// necessary. LabelObservers are associated with label but must be kept alive by
// the caller. |-startObserving| must be called before the |label| is observed.
+ (instancetype)observerForLabel:(UILabel*)label;

// LabelObservers should be created via |+observerForLabel:|.
- (instancetype)init NS_UNAVAILABLE;

// Starts observing the label. For each call to this function, |-stopObserving|
// should be called before |label| is deallocated.
- (void)startObserving;

// Stops observing the label. The label stops being observed once the number of
// call to this function match the number of call to |-startObserving|. When
// observation of a label ends, none of its attributes are changed, and none
// of the registered actions are called.
- (void)stopObserving;

// Block type that takes a label.  Blocks registered for a label will be called
// when property values are updated.
typedef void (^LabelObserverAction)(UILabel* label);

// Registers |action| to be called when stylistic properties on the observed
// label are changed.  Style changes include changes to the label's font,
// textColor, textAlignment, lineBreakMode, shadowColor, or shadowOffset.
- (void)addStyleChangedAction:(LabelObserverAction)action;

// Registers |action| to be called when the observed label's layout has changed.
// Layout changes include changes to the label's bounds, frame, or superview, as
// well as changes to its center, which doesn't affect the label's layout
// internally but does affect its position in its superview.
- (void)addLayoutChangedAction:(LabelObserverAction)action;

// Registers |action| to be called when the observed label's text has changed.
// Text changes include changes to the label's text or attributedText.
- (void)addTextChangedAction:(LabelObserverAction)action;

@end

#endif  // IOS_CHROME_BROWSER_UI_UTIL_LABEL_OBSERVER_H_
