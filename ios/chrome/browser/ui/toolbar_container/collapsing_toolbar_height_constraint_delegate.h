// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_CONTAINER_COLLAPSING_TOOLBAR_HEIGHT_CONSTRAINT_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_CONTAINER_COLLAPSING_TOOLBAR_HEIGHT_CONSTRAINT_DELEGATE_H_

#import <Foundation/Foundation.h>

@class CollapsingToolbarHeightConstraint;
namespace toolbar_container {
class HeightRange;
}  // namespace toolbar_container

// The delegate for the collapsing height constraint.
@protocol CollapsingToolbarHeightConstraintDelegate<NSObject>

// Called when |constraint|'s height range is changed from |oldHeightRange|.
- (void)collapsingHeightConstraint:
            (nonnull CollapsingToolbarHeightConstraint*)constraint
          didUpdateFromHeightRange:
              (const toolbar_container::HeightRange&)oldHeightRange;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_CONTAINER_COLLAPSING_TOOLBAR_HEIGHT_CONSTRAINT_DELEGATE_H_
