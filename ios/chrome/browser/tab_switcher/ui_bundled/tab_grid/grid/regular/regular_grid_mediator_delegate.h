// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_GRID_REGULAR_REGULAR_GRID_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_GRID_REGULAR_REGULAR_GRID_MEDIATOR_DELEGATE_H_

@protocol FacePileProviding;

// Delegate protocol for the regular grid mediator.
@protocol RegularGridMediatorDelegate

// Returns a FacePile provider for `groupID` with `groupColor`.
- (id<FacePileProviding>)facePileProviderForGroupID:(const std::string&)groupID
                                         groupColor:(UIColor*)groupColor;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_GRID_REGULAR_REGULAR_GRID_MEDIATOR_DELEGATE_H_
