// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_SECONDARY_TOOLBAR_MEDIATOR_H_
#define IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_SECONDARY_TOOLBAR_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/toolbar/ui_bundled/secondary_toolbar_keyboard_state_provider.h"

namespace collaboration::messaging {
class MessagingBackendService;
}  // namespace collaboration::messaging
@protocol SecondaryToolbarConsumer;
class WebStateList;

/// Mediator providing web state information to SecondaryToolbarViewController.
@interface SecondaryToolbarMediator
    : NSObject <SecondaryToolbarKeyboardStateProvider>

/// Creates an instance of the mediator.
- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                    messagingService:
                        (collaboration::messaging::MessagingBackendService*)
                            messagingService;

// The consumer for this mediator.
@property(nonatomic, weak) id<SecondaryToolbarConsumer> consumer;

/// Disconnects web state list reference.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_SECONDARY_TOOLBAR_MEDIATOR_H_
