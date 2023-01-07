// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_READING_LIST_MODAL_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_READING_LIST_MODAL_CONSUMER_H_

#import <Foundation/Foundation.h>

@protocol InfobarReadingListModalConsumer <NSObject>

// YES if the current page has already been added to Reading List.
- (void)setCurrentPageAdded:(BOOL)pageAdded;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_READING_LIST_MODAL_CONSUMER_H_
