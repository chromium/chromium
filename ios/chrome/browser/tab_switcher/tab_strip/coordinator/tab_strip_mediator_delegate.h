// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_TAB_STRIP_COORDINATOR_TAB_STRIP_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_TAB_STRIP_COORDINATOR_TAB_STRIP_MEDIATOR_DELEGATE_H_

// Delegate protocol for the tab strip mediator.
@protocol TabStripMediatorDelegate

// Returns a FacePile provider for `groupID` with `groupColor`.
- (id<FacePileProviding>)facePileProviderForGroupID:(const std::string&)groupID
                                         groupColor:(UIColor*)groupColor;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_TAB_STRIP_COORDINATOR_TAB_STRIP_MEDIATOR_DELEGATE_H_
