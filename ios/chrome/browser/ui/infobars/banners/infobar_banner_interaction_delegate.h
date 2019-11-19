// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_BANNERS_INFOBAR_BANNER_INTERACTION_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_BANNERS_INFOBAR_BANNER_INTERACTION_DELEGATE_H_

// Delegate for InfobarBannerInteractable events.
@protocol InfobarBannerInteractionDelegate
// Called by the InfobarBanner whenever an interaction has started.
- (void)infobarBannerStartedInteraction;
@end

// An InfobarBanner must conform to this protocol if its presentation will be
// interactable and interruptible.
@protocol InfobarBannerInteractable
// Delegate to communicate events.
@property(nonatomic, weak) id<InfobarBannerInteractionDelegate>
    interactionDelegate;
@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_BANNERS_INFOBAR_BANNER_INTERACTION_DELEGATE_H_
