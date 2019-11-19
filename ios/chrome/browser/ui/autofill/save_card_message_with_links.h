// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_SAVE_CARD_MESSAGE_WITH_LINKS_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_SAVE_CARD_MESSAGE_WITH_LINKS_H_

#import <UIKit/UIKit.h>

#include <vector>

class GURL;

// Represents a message with optional links. Each linkRange in |linkRanges|
// represents the range (in |messageText|) for the corresponding (same index)
// linkURL in |linkURLS|.
@interface SaveCardMessageWithLinks : NSObject

@property(nonatomic, copy) NSString* messageText;

@property(nonatomic, strong) NSArray* linkRanges;

@property(nonatomic, assign) std::vector<GURL> linkURLs;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_SAVE_CARD_MESSAGE_WITH_LINKS_H_
