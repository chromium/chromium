// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_UTIL_NAMED_GUIDE_H_
#define IOS_CHROME_BROWSER_SHARED_UI_UTIL_NAMED_GUIDE_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"

// A UILayoutGuide subclass that represents the layout of a well-known piece of
// UI.  See layout_guide_names.h for a list of the UI components for which named
// guides are created.
@interface NamedGuide : UILayoutGuide

// Designated initializer for a guide with `name`.
- (instancetype)initWithName:(GuideName*)name NS_DESIGNATED_INITIALIZER;

// NamedGuides must be created using `-initWithName:`.
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Returns the NamedGuide with the given `name`, if one is attached to `view`
// or one of `view`'s ancestors.  If no guide is found, returns nil.
+ (instancetype)guideWithName:(GuideName*)name view:(UIView*)view;

// Resets `constrainedView` and `constrainedFrame`, deactivating constraints
// that were created to support following the view/frame.  Note that calling
// this function has no effect on constraints that were created outside of this
// class.  `isConstrained` will return NO after calling this function.
- (void)resetConstraints;

// The GuideName passed on initialization.
@property(nonatomic, readonly) GuideName* name;

// Whether this NamedGuide is constrained to either a view or frame (using
// `constrainedView` or `constrainedFrame`, respectively).
@property(nonatomic, readonly, getter=isConstrained) BOOL constrained;

// The view to which this guide should be constrained.  Setting this property
// to a new value will update the guide's constraints to match the new view.
// Setting to nil removes constraints.  Setting this property to a non-nil value
// will reset `constrainedFrame` to CGRectNull.
@property(nonatomic, weak) UIView* constrainedView;

// The frame to which this guide should be constrained, in the guide's owning
// view's coordinate system.  This can be used to specify locations that don't
// correspond with a particular view, or correspond to views in different
// windows.  Setting this property to a new value will update the guide's
// constraints to match the specified frame according to `autoresizingMask`.
// Setting to CGRectNull removes constraints.  Setting this property to a non-
// CGRectNull value will reset `constrainedView` to nil.  If `constrainedView`
// is removed from `owningView`'s hierarchy, this property will be reset to nil.
@property(nonatomic, assign) CGRect constrainedFrame;

// The autoresizing behavior to use when setting up constraints for
// `constrainedFrame`.  This property has no effect if `constrainedFrame` is
// CGRectNull.
@property(nonatomic, assign) UIViewAutoresizing autoresizingMask;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_UTIL_NAMED_GUIDE_H_
