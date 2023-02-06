// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_CHECKUP_PASSWORD_CHECKUP_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_CHECKUP_PASSWORD_CHECKUP_MEDIATOR_H_

#import <Foundation/Foundation.h>

@protocol PasswordCheckupConsumer;

// This mediator fetches and organises the insecure credentials for its
// consumer.
@interface PasswordCheckupMediator : NSObject

// Consumer of this mediator.
@property(nonatomic, weak) id<PasswordCheckupConsumer> consumer;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_CHECKUP_PASSWORD_CHECKUP_MEDIATOR_H_
