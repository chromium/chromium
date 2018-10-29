// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_CONTAINER_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_CONTAINER_CONSUMER_H_

#import <UIKit/UIKit.h>

class InfoBarIOS;

// Protocol to communicate with the Infobar container.
@protocol InfobarContainerConsumer

// Add a new infobar to the Infobar container view at position |position|.
- (void)addInfoBar:(InfoBarIOS*)infoBarIOS position:(NSInteger)position;

// Sets the Infobar container user interaction to |enabled|.
- (void)setUserInteractionEnabled:(BOOL)enabled;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_CONTAINER_CONSUMER_H_
