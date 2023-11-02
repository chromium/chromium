// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_POPUP_MENU_POPUP_MENU_CAROUSEL_METRICS_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_POPUP_MENU_POPUP_MENU_CAROUSEL_METRICS_DELEGATE_H_

#import <Foundation/Foundation.h>

// Protocol for informing a client of overflow menu, carousel-related metrics,
// like the number of currently visible destinations.
@protocol PopupMenuCarouselMetricsDelegate

// Called when the popup menu detects the number of visible destinations in the
// carousel changes.
- (void)visibleDestinationCountDidChange:(NSInteger)numVisibleDestinations;

@end

#endif  // IOS_CHROME_BROWSER_UI_POPUP_MENU_POPUP_MENU_CAROUSEL_METRICS_DELEGATE_H_
