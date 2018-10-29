// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_PASSWORD_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_PASSWORD_MEDIATOR_H_

#import <UIKit/UIKit.h>

#include "base/memory/ref_counted.h"

@protocol ManualFillContentDelegate;
@protocol ManualFillPasswordConsumer;
@protocol PasswordListDelegate;

namespace password_manager {
class PasswordStore;
}  // namespace password_manager

class WebStateList;

namespace manual_fill {

extern NSString* const ManagePasswordsAccessibilityIdentifier;
extern NSString* const OtherPasswordsAccessibilityIdentifier;

}  // namespace manual_fill

// Object in charge of getting the passwords relevant for the manual fill
// passwords UI.
@interface ManualFillPasswordMediator : NSObject<UISearchResultsUpdating>

// The consumer for passwords updates. Setting it will trigger the consumer
// methods with the current data.
@property(nonatomic, weak) id<ManualFillPasswordConsumer> consumer;
// The delegate in charge of using the content selected by the user.
@property(nonatomic, weak) id<ManualFillContentDelegate> contentDelegate;
// The delegate in charge of navigation.
@property(nonatomic, weak) id<PasswordListDelegate> navigationDelegate;
// If YES disables filtering the fetched passwords with the active web state
// url. Also activates an "All passwords" action if NO. Set this value before
// setting the consumer, since just setting this won't trigger the consumer
// methods.
@property(nonatomic, assign) BOOL disableFilter;

// The designated initializer. |passwordStore| and |webStateList| must not be
// nil.
- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                       passwordStore:
                           (scoped_refptr<password_manager::PasswordStore>)
                               passwordStore NS_DESIGNATED_INITIALIZER;

// Unavailable. Use |initWithWebStateList:passwordStore:|.
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_PASSWORD_MEDIATOR_H_
