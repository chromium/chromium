// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_BUGGY_AUTHENTICATION_VIEW_OWNER_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_BUGGY_AUTHENTICATION_VIEW_OWNER_H_

#include <Foundation/Foundation.h>

// Coordinators implementing this protocol may have the authentication view
// directly displayed on top of `self.baseViewController`. This view is known to
// disappear silently; See crbug.com/395959814. Any coordinator that start a
// coordinator implementing `BuggyAuthenticationViewOwner` should use
// `viewWillPersist` to check whether they should expect the view to have
// disappeared.
@protocol BuggyAuthenticationViewOwner

// Whether the view currently being displayed on top of
// `self.baseViewcontroller` is safe from crbug.com/395959814. If the value is
// `false` the coordinator may be started with no user-visible UI.
@property(nonatomic, readonly) BOOL viewWillPersist;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_BUGGY_AUTHENTICATION_VIEW_OWNER_H_
