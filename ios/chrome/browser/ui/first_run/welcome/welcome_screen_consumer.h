// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_WELCOME_WELCOME_SCREEN_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_WELCOME_WELCOME_SCREEN_CONSUMER_H_

#import <Foundation/Foundation.h>

@protocol WelcomeScreenConsumer <NSObject>

@property(nonatomic, assign) BOOL isManaged;

@end

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_WELCOME_WELCOME_SCREEN_CONSUMER_H_
