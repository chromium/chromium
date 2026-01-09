// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_AUTHENTICATION_COORDINATOR_CARD_UNMASK_AUTHENTICATION_SELECTION_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_AUTOFILL_AUTHENTICATION_COORDINATOR_CARD_UNMASK_AUTHENTICATION_SELECTION_MEDIATOR_DELEGATE_H_

#import <Foundation/Foundation.h>

// The authentication selection mediator delegates dismissal to the coordinator.
@protocol CardUnmaskAuthenticationSelectionMediatorDelegate <NSObject>

// Dismiss the authentication selection view.
- (void)dismissAuthenticationSelection;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_AUTHENTICATION_COORDINATOR_CARD_UNMASK_AUTHENTICATION_SELECTION_MEDIATOR_DELEGATE_H_
