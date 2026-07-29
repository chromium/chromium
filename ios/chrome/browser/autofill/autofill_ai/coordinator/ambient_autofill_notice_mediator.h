// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_COORDINATOR_AMBIENT_AUTOFILL_NOTICE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_COORDINATOR_AMBIENT_AUTOFILL_NOTICE_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "base/memory/weak_ptr.h"
#import "components/autofill/ios/form_util/form_activity_params.h"

@protocol AutofillCommands;

namespace web {
class WebState;
}

@interface AmbientAutofillNoticeMediator : NSObject

- (instancetype)initWithWebState:(base::WeakPtr<web::WebState>)webState
                          params:(const autofill::FormActivityParams&)params
                 autofillHandler:(id<AutofillCommands>)autofillHandler
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Triggers when user taps the primary "OK" action.
- (void)didAcknowledgeNotice;

// Triggers when user taps the secondary "Settings" action.
- (void)didTapSettings;

// Triggers when user swiped down to dismiss the bottom sheet notice manually.
- (void)didDismissNotice;

// Marks the notice as shown in profile preferences.
- (void)markNoticeShown;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_COORDINATOR_AMBIENT_AUTOFILL_NOTICE_MEDIATOR_H_
