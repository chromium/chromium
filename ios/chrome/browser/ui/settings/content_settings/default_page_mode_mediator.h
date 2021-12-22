// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CONTENT_SETTINGS_DEFAULT_PAGE_MODE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CONTENT_SETTINGS_DEFAULT_PAGE_MODE_MEDIATOR_H_

#import <Foundation/Foundation.h>

@protocol DefaultPageModeConsumer;

// Mediator for the screen allowing the user to choose the default mode
// (Desktop/Mobile) for loading pages.
@interface DefaultPageModeMediator : NSObject

- (instancetype)initWithConsumer:(id<DefaultPageModeConsumer>)consumer
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CONTENT_SETTINGS_DEFAULT_PAGE_MODE_MEDIATOR_H_
