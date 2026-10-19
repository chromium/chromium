// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_EMBEDDED_REAUTH_INTERACTION_MANAGER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_EMBEDDED_REAUTH_INTERACTION_MANAGER_H_

#import <UIKit/UIKit.h>

#import "base/functional/callback_forward.h"
#import "base/types/expected.h"

@protocol SystemIdentity;

// Type of ReAuth token passed to `EmbeddedReAuthInteractionManager`.
enum class EmbeddedReAuthTokenType {
  kReAuthRequestToken,
  kPackagedLookupToken,
};

// Initiates an Embedded ReAuth UI flow to obtain a ReAuth Proof Token (RAPT).
// TODO(crbug.com/434939275): Support canceling and dismissing UI.
class EmbeddedReAuthInteractionManager {
 public:
  // Callback invoked when the Embedded ReAuth flow completes.
  // Contains the ReAuth Proof Token on success, or an `NSError*` on failure.
  using ReAuthCompletionCallback =
      base::OnceCallback<void(base::expected<NSString*, NSError*> result)>;

  EmbeddedReAuthInteractionManager();
  EmbeddedReAuthInteractionManager(const EmbeddedReAuthInteractionManager&) =
      delete;
  EmbeddedReAuthInteractionManager& operator=(
      const EmbeddedReAuthInteractionManager&) = delete;
  virtual ~EmbeddedReAuthInteractionManager();

  // Returns whether Embedded ReAuth is supported by the provider.
  virtual bool IsReAuthSupported() const = 0;

  // Initiates the Embedded ReAuth flow for `identity` using `reauth_token` of
  // `token_type`, presenting UI on top of `presenting_view_controller`, and
  // invokes `callback` with the resulting ReAuth proof token or error.
  virtual void InitiateEmbeddedReAuth(
      id<SystemIdentity> identity,
      NSString* reauth_token,
      EmbeddedReAuthTokenType token_type,
      UIViewController* presenting_view_controller,
      ReAuthCompletionCallback callback) = 0;
};

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_EMBEDDED_REAUTH_INTERACTION_MANAGER_H_
