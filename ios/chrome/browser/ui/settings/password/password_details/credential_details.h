// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_CREDENTIAL_DETAILS_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_CREDENTIAL_DETAILS_H_

#import <Foundation/Foundation.h>

#import <optional>

#import "base/time/time.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "url/gurl.h"

namespace password_manager {
struct CredentialUIEntry;
}  // namespace password_manager

// Represents the credential type (blocked, federated or regular) of the
// credential in this Credential Details.
typedef NS_ENUM(NSInteger, CredentialType) {
  CredentialTypeRegularPassword = kItemTypeEnumZero,
  CredentialTypeBlocked,
  CredentialTypeFederation,
  CredentialTypePasskey,
};

// Enum which represents the entry point from which the credential details are
// accessed.
enum class DetailsContext {
  kPasswordSettings,   // When accessed from any context other than Password
                       // Checkup inside the settings context.
  kOutsideSettings,    // When accessed outside the settings context.
  kCompromisedIssues,  // When accessed from the compromised issues page.
  kDismissedWarnings,  // When accessed from the dismissed warnings page.
  kReusedIssues,       // When accessed from the reused issues page.
  kWeakIssues,         // When accessed from the weak issues page.
};

// Object which is used by `PasswordDetailsViewController` to show
// information about password and/or passkey.
@interface CredentialDetails : NSObject

// Represents the type of the credential (blocked, federated or regular).
@property(nonatomic, assign) CredentialType credentialType;

// Associated sign-on realm used as identifier for this object.
@property(nonatomic, copy, readonly) NSString* signonRealm;

// Short version of websites.
@property(nonatomic, copy, readonly) NSArray<NSString*>* origins;

// Associated websites. It is determined by either the sign-on realm or the
// display name of the Android app.
@property(nonatomic, copy, readonly) NSArray<NSString*>* websites;

// Associated username.
@property(nonatomic, copy) NSString* username;

// The user's display name, if this is a passkey. Always empty otherwise.
@property(nonatomic, copy) NSString* userDisplayName;

// The federation providing this credential, if any.
@property(nonatomic, copy, readonly) NSString* federation;

// The creation time, if this is a passkey, nullopt otherwise.
@property(nonatomic, readonly) std::optional<base::Time> creationTime;

// Associated password.
@property(nonatomic, copy) NSString* password;

// Associated note.
@property(nonatomic, copy) NSString* note;

// Whether password is compromised or not.
@property(nonatomic, assign, getter=isCompromised) BOOL compromised;

// Whether password is muted or not.
@property(nonatomic, assign, getter=isMuted) BOOL muted;

// URL which allows to change the password of compromised credential.
@property(nonatomic, readonly) std::optional<GURL> changePasswordURL;

// `shouldOfferToMoveToAccount` tells whether or not to show a move option.
@property(nonatomic, assign) BOOL shouldOfferToMoveToAccount;

// The DetailsContext for the credential details.
@property(nonatomic, assign) DetailsContext context;

- (instancetype)initWithCredential:
    (const password_manager::CredentialUIEntry&)credential
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_CREDENTIAL_DETAILS_H_
