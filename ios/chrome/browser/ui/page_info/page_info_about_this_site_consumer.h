// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_ABOUT_THIS_SITE_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_ABOUT_THIS_SITE_CONSUMER_H_

#import <Foundation/Foundation.h>

@class PageInfoAboutThisSiteInfo;

// Consumer protocol for the view controller that displays AboutThisSite
// information, i.e Page Info.
@protocol PageInfoAboutThisSiteConsumer <NSObject>

// The AboutThisSite information to be displayed by the view controller. This
// method is called once when the consumer is connected to the mediator.
- (void)setAboutThisSiteSection:(PageInfoAboutThisSiteInfo*)info;

@end

#endif  // IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_ABOUT_THIS_SITE_CONSUMER_H_
