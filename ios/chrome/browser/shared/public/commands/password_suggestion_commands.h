// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_PASSWORD_SUGGESTION_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_PASSWORD_SUGGESTION_COMMANDS_H_

#import <Foundation/Foundation.h>

// Commands related to suggesting strong passwords.
@protocol PasswordSuggestionCommands

// Shows the password suggestion view controller. The secondary action button
// string is set depending on the value of proactive
- (void)showPasswordSuggestion:(NSString*)passwordSuggestion
                     proactive:(BOOL)proactive
                      webState:(web::WebState*)webState
               decisionHandler:(void (^)(BOOL accept))decisionHandler;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_PASSWORD_SUGGESTION_COMMANDS_H_
