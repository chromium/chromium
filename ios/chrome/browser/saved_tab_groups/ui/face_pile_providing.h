// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_UI_FACE_PILE_PROVIDING_H_
#define IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_UI_FACE_PILE_PROVIDING_H_

#import <UIKit/UIKit.h>

// Objects implementing this protocol will provide a FacePile view (always the
// same). FacePiles provided by this object will be up-to-date as long as this
// object is alive.
@protocol FacePileProviding

// Returns the FacePile computed width.
- (CGFloat)facePileWidth;

// Returns the FacePile associated with this object.
- (UIView*)facePileView;

// Returns YES if the providers are considered equal, NO otherwise.
- (BOOL)isEqualFacePileProviding:(id<FacePileProviding>)otherProvider;

@end

#endif  // IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_UI_FACE_PILE_PROVIDING_H_
