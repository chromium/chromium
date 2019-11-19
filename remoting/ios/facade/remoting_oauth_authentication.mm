// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/facade/remoting_oauth_authentication.h"

#import <Foundation/Foundation.h>
#import <Security/Security.h>

#import "base/bind.h"
#import "base/bind_helpers.h"
#import "ios/third_party/material_components_ios/src/components/Snackbar/src/MaterialSnackbar.h"
#import "remoting/ios/facade/ios_client_runtime_delegate.h"
#import "remoting/ios/facade/remoting_service.h"
#import "remoting/ios/persistence/remoting_keychain.h"
#import "remoting/ios/persistence/remoting_preferences.h"

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "net/url_request/url_request_context_getter.h"
#include "remoting/base/oauth_token_getter.h"
#include "remoting/base/oauth_token_getter_impl.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

static const char kOauthRedirectUrl[] =
    "https://chromoting-oauth.talkgadget."
    "google.com/talkgadget/oauth/chrome-remote-desktop/dev";

// We currently don't support multi-account sign in for OAuth authentication, so
// we store the current refresh token for an unspecified account. If we later
// decide to support multi-account sign in, we may use the user email as the
// account name when storing the refresh token and store the current user email
// in UserDefaults.
static const auto kRefreshTokenAccount =
    remoting::Keychain::kUnspecifiedAccount;

std::unique_ptr<remoting::OAuthTokenGetter>
CreateOAuthTokenGetterWithAuthorizationCode(
    const std::string& auth_code,
    const remoting::OAuthTokenGetter::CredentialsUpdatedCallback&
        on_credentials_update) {
  std::unique_ptr<remoting::OAuthTokenGetter::OAuthIntermediateCredentials>
      oauth_credentials(
          new remoting::OAuthTokenGetter::OAuthIntermediateCredentials(
              auth_code, /*is_service_account=*/false));
  oauth_credentials->oauth_redirect_uri = kOauthRedirectUrl;

  std::unique_ptr<remoting::OAuthTokenGetter> oauth_tokenGetter(
      new remoting::OAuthTokenGetterImpl(
          std::move(oauth_credentials), on_credentials_update,
          RemotingService.instance.runtime->url_loader_factory(),
          /*auto_refresh=*/true));
  return oauth_tokenGetter;
}

std::unique_ptr<remoting::OAuthTokenGetter> CreateOAuthTokenWithRefreshToken(
    const std::string& refresh_token,
    const std::string& email) {
  std::unique_ptr<remoting::OAuthTokenGetter::OAuthAuthorizationCredentials>
      oauth_credentials(
          new remoting::OAuthTokenGetter::OAuthAuthorizationCredentials(
              email, refresh_token, /*is_service_account=*/false));

  std::unique_ptr<remoting::OAuthTokenGetter> oauth_tokenGetter(
      new remoting::OAuthTokenGetterImpl(
          std::move(oauth_credentials),
          base::DoNothing(),
          RemotingService.instance.runtime->url_loader_factory(),
          /*auto_refresh=*/true));
  return oauth_tokenGetter;
}

RemotingAuthenticationStatus oauthStatusToRemotingAuthenticationStatus(
    remoting::OAuthTokenGetter::Status status) {
  switch (status) {
    case remoting::OAuthTokenGetter::Status::AUTH_ERROR:
      return RemotingAuthenticationStatusAuthError;
    case remoting::OAuthTokenGetter::Status::NETWORK_ERROR:
      return RemotingAuthenticationStatusNetworkError;
    case remoting::OAuthTokenGetter::Status::SUCCESS:
      return RemotingAuthenticationStatusSuccess;
  }
}

@interface RemotingOAuthAuthentication () {
  std::unique_ptr<remoting::OAuthTokenGetter> _tokenGetter;
  BOOL _firstLoadUserAttempt;
}
@end

@implementation RemotingOAuthAuthentication

@synthesize user = _user;
@synthesize delegate = _delegate;

- (instancetype)init {
  self = [super init];
  if (self) {
    _user = nil;
    _firstLoadUserAttempt = YES;
  }
  return self;
}

#pragma mark - Property Overrides

- (UserInfo*)user {
  if (_firstLoadUserAttempt && _user == nil) {
    _firstLoadUserAttempt = NO;
    [self setUser:[self loadUserInfo]];
  }
  return _user;
}

- (void)setUser:(UserInfo*)user {
  _user = user;
  [self storeUserInfo:_user];
  [_delegate userDidUpdate:_user];
}

#pragma mark - Class Implementation

- (void)authenticateWithAuthorizationCode:(NSString*)authorizationCode {
  __weak RemotingOAuthAuthentication* weakSelf = self;
  _tokenGetter = CreateOAuthTokenGetterWithAuthorizationCode(
      std::string(base::SysNSStringToUTF8(authorizationCode)),
      base::BindRepeating(
          ^(const std::string& user_email, const std::string& refresh_token) {
            VLOG(1) << "New Creds: " << user_email << " " << refresh_token;
            UserInfo* user = [[UserInfo alloc] init];
            user.userEmail = base::SysUTF8ToNSString(user_email);
            user.refreshToken = base::SysUTF8ToNSString(refresh_token);
            [weakSelf setUser:user];
          }));
  // Stimulate the oAuth Token Getter to fetch and access token, this forces it
  // to convert the authorization code into a refresh token, and saving the
  // refresh token will happen automaticly in the above block.
  [self callbackWithAccessToken:^(RemotingAuthenticationStatus status,
                                  NSString* user_email,
                                  NSString* access_token) {
    if (status == RemotingAuthenticationStatusSuccess) {
      VLOG(1) << "Success fetching access token from authorization code.";
    } else {
      LOG(ERROR) << "Failed to fetch access token from authorization code. ("
                 << status << ")";
      [MDCSnackbarManager
          showMessage:
              [MDCSnackbarMessage
                  messageWithText:@"Authentication Failed. Please try again."]];
    }
  }];
}

#pragma mark - Private

// Provide the |refreshToken| and |email| to authenticate a user as a returning
// user of the application.
- (void)authenticateWithRefreshToken:(NSString*)refreshToken
                               email:(NSString*)email {
  _tokenGetter = CreateOAuthTokenWithRefreshToken(
      std::string(base::SysNSStringToUTF8(refreshToken)),
      base::SysNSStringToUTF8(email));
}

- (void)callbackWithAccessToken:(AccessTokenCallback)onAccessToken {
  // Be careful here since a failure to reset onAccessToken will end up with
  // retain cycle and memory leakage.
  if (_tokenGetter) {
    _tokenGetter->CallWithToken(base::BindOnce(
        ^(remoting::OAuthTokenGetter::Status status,
          const std::string& user_email, const std::string& access_token) {
          onAccessToken(oauthStatusToRemotingAuthenticationStatus(status),
                        base::SysUTF8ToNSString(user_email),
                        base::SysUTF8ToNSString(access_token));
        }));
  }
}

- (void)logout {
  [self storeUserInfo:nil];
  [self setUser:nil];
}

- (void)invalidateCache {
  _tokenGetter->InvalidateCache();
}

#pragma mark - Persistence

- (void)storeUserInfo:(UserInfo*)user {
  if (user) {
    [RemotingPreferences instance].activeUserKey = user.userEmail;
    std::string refreshToken = base::SysNSStringToUTF8(user.refreshToken);
    remoting::RemotingKeychain::GetInstance()->SetData(
        remoting::Keychain::Key::REFRESH_TOKEN, kRefreshTokenAccount,
        refreshToken);
  } else {
    [RemotingPreferences instance].activeUserKey = nil;
    remoting::RemotingKeychain::GetInstance()->RemoveData(
        remoting::Keychain::Key::REFRESH_TOKEN, kRefreshTokenAccount);
  }
}

- (UserInfo*)loadUserInfo {
  UserInfo* user = [[UserInfo alloc] init];
  user.userEmail = [RemotingPreferences instance].activeUserKey;
  std::string refreshTokenString =
      remoting::RemotingKeychain::GetInstance()->GetData(
          remoting::Keychain::Key::REFRESH_TOKEN, kRefreshTokenAccount);
  user.refreshToken = base::SysUTF8ToNSString(refreshTokenString);

  if (!user || ![user isAuthenticated]) {
    user = nil;
  } else {
    [self authenticateWithRefreshToken:user.refreshToken email:user.userEmail];
  }
  return user;
}

@end
