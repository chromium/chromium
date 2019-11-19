// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_NOTIFICATION_PRESENTER_H_
#define IOS_CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_NOTIFICATION_PRESENTER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/translate/translate_notification_handler.h"

@protocol SnackbarCommands;

// Presents translate notifications snackbars.
@interface TranslateNotificationPresenter
    : NSObject <TranslateNotificationHandler>

- (instancetype)initWithDispatcher:(id<SnackbarCommands>)dispatcher
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_NOTIFICATION_PRESENTER_H_
