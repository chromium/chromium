// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_PASSWORDS_CWV_PASSWORD_CONTROLLER_H_
#define IOS_WEB_VIEW_INTERNAL_PASSWORDS_CWV_PASSWORD_CONTROLLER_H_

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, CWVPasswordUserDecision);
@class CWVAutofillSuggestion;
@class CWVPasswordController;
@class CWVPassword;

namespace web {
class WebState;
}

// Internal protocol to receive callbacks related to password autofilling.
@protocol CWVPasswordControllerDelegate

// Called when user needs to decide on whether or not to save the |password|.
// This can happen when user is successfully logging into a web site with a new
// username.
// Pass user decision to |decisionHandler|. This block should be called only
// once if user made the decision, or not get called if user ignores the prompt.
- (void)passwordController:(CWVPasswordController*)passwordController
    decideSavePolicyForPassword:(CWVPassword*)password
                decisionHandler:
                    (void (^)(CWVPasswordUserDecision decision))decisionHandler;

// Called when user needs to decide on whether or not to update the |password|.
// This can happen when user is successfully logging into a web site with a new
// password and an existing username.
// Pass user decision to |decisionHandler|. This block should be called only
// once if user made the decision, or not get called if user ignores the prompt.
- (void)passwordController:(CWVPasswordController*)passwordController
    decideUpdatePolicyForPassword:(CWVPassword*)password
                  decisionHandler:(void (^)(CWVPasswordUserDecision decision))
                                      decisionHandler;

@end

// Implements features that allow saving entered passwords as well as
// autofilling password forms.
@interface CWVPasswordController : NSObject

// Creates a new password controller with the given |webState|.
// |delegate| is used to receive password autofill suggestion callbacks.
- (instancetype)initWithWebState:(web::WebState*)webState
                     andDelegate:
                         (nullable id<CWVPasswordControllerDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// See same method in |CWVAutofillController|. This one only fetches password
// autofill suggestions.
- (void)fetchSuggestionsForFormWithName:(NSString*)formName
                        fieldIdentifier:(NSString*)fieldIdentifier
                              fieldType:(NSString*)fieldType
                                frameID:(NSString*)frameID
                      completionHandler:
                          (void (^)(NSArray<CWVAutofillSuggestion*>*))
                              completionHandler;

// See same method in |CWVAutofillController|. This one autofills password form
// on the page.
- (void)fillSuggestion:(CWVAutofillSuggestion*)suggestion
     completionHandler:(void (^)(void))completionHandler;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_INTERNAL_PASSWORDS_CWV_PASSWORD_CONTROLLER_H_
