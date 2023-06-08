// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_MEDIATOR_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_MEDIATOR_CONSUMER_H_

@class WhatsNewItem;

// Handles updates from the mediator to the UI.
@protocol WhatsNewMediatorConsumer <NSObject>

// Sets What's New feature items and chrome tip item.
- (void)setWhatsNewProperties:(WhatsNewItem*)chromeTip
                 featureItems:(NSArray<WhatsNewItem*>*)featureItems;

@end

#endif  // IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_MEDIATOR_CONSUMER_H_
