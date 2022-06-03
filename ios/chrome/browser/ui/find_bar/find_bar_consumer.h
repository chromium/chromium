// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIND_BAR_FIND_BAR_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_FIND_BAR_FIND_BAR_CONSUMER_H_

@class FindInPageModel;

@protocol FindBarConsumer <NSObject>

- (void)updateResultsCount:(FindInPageModel*)model;

@end

#endif  // IOS_CHROME_BROWSER_UI_FIND_BAR_FIND_BAR_CONSUMER_H_
