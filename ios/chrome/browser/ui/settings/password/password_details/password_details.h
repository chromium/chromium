// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "third_party/abseil-cpp/absl/types/optional.h"
#import "url/gurl.h"

namespace password_manager {
struct CredentialUIEntry;
}  // namespace password_manager

// Represents the credential type (blocked, federated or regular) of the
// credential in this Password Details.
typedef NS_ENUM(NSInteger, CredentialType) {
  CredentialTypeRegular = kItemTypeEnumZero,
  CredentialTypeBlocked,
  CredentialTypeFederation,
};

// Enum which represents the entry point from which the password details are
// accessed.
enum class DetailsContext {
  kGeneral,  // When accessed from any context other than Password Checkup.
  kCompromisedIssues,  // When accessed from the compromised issues page.
  kDismissedWarnings,  // When accessed from the dismissed warnings page.
  kReusedIssues,       // When accessed from the reused issues page.
  kWeakIssues,         // When accessed from the weak issues page.
};

// Object which is used by `PasswordDetailsViewController` to show
// information about password.
@interface PasswordDetails : NSObject

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

// The federation providing this credential, if any.
@property(nonatomic, copy, readonly) NSString* federation;

// Associated password.
@property(nonatomic, copy) NSString* password;

// Associated note.
@property(nonatomic, copy) NSString* note;

// Whether password is compromised or not.
@property(nonatomic, assign, getter=isCompromised) BOOL compromised;

// Whether password is muted or not.
@property(nonatomic, assign, getter=isMuted) BOOL muted;

// URL which allows to change the password of compromised credential.
@property(nonatomic, readonly) absl::optional<GURL> changePasswordURL;

// `shouldOfferToMoveToAccount` tells whether or not to show a move option.
@property(nonatomic, assign) BOOL shouldOfferToMoveToAccount;

// The DetailsContext for the password details.
@property(nonatomic, assign) DetailsContext context;

- (instancetype)initWithCredential:
    (const password_manager::CredentialUIEntry&)credential
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_H_
