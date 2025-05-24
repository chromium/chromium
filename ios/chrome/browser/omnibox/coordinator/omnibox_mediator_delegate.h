// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_COORDINATOR_OMNIBOX_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_OMNIBOX_COORDINATOR_OMNIBOX_MEDIATOR_DELEGATE_H_

#import <UIKit/UIKit.h>

@class OmniboxMediator;

/// Delegate for the omnibox mediator.
@protocol OmniboxMediatorDelegate <NSObject>

/// Omnibox textfield did begin editing.
- (void)omniboxMediatorDidBeginEditing:(OmniboxMediator*)mediator;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_COORDINATOR_OMNIBOX_MEDIATOR_DELEGATE_H_
