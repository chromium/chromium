// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FOLLOW_FOLLOW_IPH_PRESENTER_H_
#define IOS_CHROME_BROWSER_FOLLOW_FOLLOW_IPH_PRESENTER_H_

// Protocol to present in-product help (IPH) related to the follow feature.
@protocol FollowIPHPresenter

// Tells receiver to present the in-product help (IPH) to follow the currently
// browsing site.
- (void)presentFollowWhileBrowsingIPH;

@end

#endif  // IOS_CHROME_BROWSER_FOLLOW_FOLLOW_IPH_PRESENTER_H_
