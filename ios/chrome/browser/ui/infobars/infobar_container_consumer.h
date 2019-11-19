// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_CONTAINER_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_CONTAINER_CONSUMER_H_

#import <UIKit/UIKit.h>

@protocol InfobarUIDelegate;

// Protocol to communicate with the Infobar container.
@protocol InfobarContainerConsumer

// Adds |infoBarDelegate|'s Infobar to the InfobarContainer.
- (void)addInfoBarWithDelegate:(id<InfobarUIDelegate>)infoBarDelegate;

// Informs InfobarContainerConsumer that the backing infobarManager will change.
// This most likely means that the WebState is changing and a new set of
// Infobars will/may be presented.
- (void)infobarManagerWillChange;

// Sets the Infobar container user interaction to |enabled|.
- (void)setUserInteractionEnabled:(BOOL)enabled;

// Tells the consumer to update its layout to reflect changes in the contained
// infobars.
- (void)updateLayoutAnimated:(BOOL)animated;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_CONTAINER_CONSUMER_H_
