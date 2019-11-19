// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"

#import <Foundation/Foundation.h>

#include "base/mac/scoped_block.h"
#include "base/strings/sys_string_conversions.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_interaction_manager.h"
#include "ios/public/provider/chrome/browser/signin/signin_resources_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using ::testing::_;
using ::testing::Invoke;

namespace {

UIImage* FakeGetCachedAvatarForIdentity(ChromeIdentity*) {
  ios::SigninResourcesProvider* provider =
      ios::GetChromeBrowserProvider()->GetSigninResourcesProvider();
  return provider ? provider->GetDefaultAvatar() : nil;
}

NSString* FakeGetHostedDomainForIdentity(ChromeIdentity* identity) {
  return base::SysUTF8ToNSString(gaia::ExtractDomainName(
      gaia::CanonicalizeEmail(base::SysNSStringToUTF8(identity.userEmail))));
}
}

@interface FakeAccountDetailsViewController : UIViewController {
  __weak ChromeIdentity* _identity;
  UIButton* _removeAccountButton;
}
@end

@implementation FakeAccountDetailsViewController

- (instancetype)initWithIdentity:(ChromeIdentity*)identity {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _identity = identity;
  }
  return self;
}

- (void)dealloc {
  [_removeAccountButton removeTarget:self
                              action:@selector(didTapRemoveAccount:)
                    forControlEvents:UIControlEventTouchUpInside];
}

- (void)viewDidLoad {
  [super viewDidLoad];

  // Obnoxious color, this is a test screen.
  self.view.backgroundColor = [UIColor orangeColor];

  _removeAccountButton = [UIButton buttonWithType:UIButtonTypeCustom];
  [_removeAccountButton setTitle:@"Remove account"
                        forState:UIControlStateNormal];
  [_removeAccountButton addTarget:self
                           action:@selector(didTapRemoveAccount:)
                 forControlEvents:UIControlEventTouchUpInside];
  [self.view addSubview:_removeAccountButton];
}

- (void)viewWillLayoutSubviews {
  [super viewWillLayoutSubviews];

  CGRect bounds = self.view.bounds;
  [_removeAccountButton
      setCenter:CGPointMake(CGRectGetMidX(bounds), CGRectGetMidY(bounds))];
  [_removeAccountButton sizeToFit];
}

- (void)didTapRemoveAccount:(id)sender {
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()
      ->ForgetIdentity(_identity, ^(NSError*) {
        [self dismissViewControllerAnimated:YES completion:nil];
      });
}

@end

namespace ios {
NSString* const kIdentityEmailFormat = @"%@@gmail.com";
NSString* const kIdentityGaiaIDFormat = @"%@ID";

FakeChromeIdentityService::FakeChromeIdentityService()
    : identities_([[NSMutableArray alloc] init]),
      _fakeMDMError(false),
      _pendingCallback(0) {}

FakeChromeIdentityService::~FakeChromeIdentityService() {}

// static
FakeChromeIdentityService*
FakeChromeIdentityService::GetInstanceFromChromeProvider() {
  return static_cast<ios::FakeChromeIdentityService*>(
      ios::GetChromeBrowserProvider()->GetChromeIdentityService());
}

DismissASMViewControllerBlock
FakeChromeIdentityService::PresentAccountDetailsController(
    ChromeIdentity* identity,
    UIViewController* viewController,
    BOOL animated) {
  UIViewController* accountDetailsViewController =
      [[FakeAccountDetailsViewController alloc] initWithIdentity:identity];
  [viewController presentViewController:accountDetailsViewController
                               animated:animated
                             completion:nil];
  return ^(BOOL animated) {
    [accountDetailsViewController dismissViewControllerAnimated:animated
                                                     completion:nil];
  };
}

ChromeIdentityInteractionManager*
FakeChromeIdentityService::CreateChromeIdentityInteractionManager(
    ios::ChromeBrowserState* browser_state,
    id<ChromeIdentityInteractionManagerDelegate> delegate) const {
  ChromeIdentityInteractionManager* manager =
      [[FakeChromeIdentityInteractionManager alloc] init];
  manager.delegate = delegate;
  return manager;
}

bool FakeChromeIdentityService::IsValidIdentity(
    ChromeIdentity* identity) const {
  return [identities_ indexOfObject:identity] != NSNotFound;
}

ChromeIdentity* FakeChromeIdentityService::GetIdentityWithGaiaID(
    const std::string& gaia_id) const {
  NSString* gaiaID = base::SysUTF8ToNSString(gaia_id);
  NSUInteger index =
      [identities_ indexOfObjectPassingTest:^BOOL(ChromeIdentity* obj,
                                                  NSUInteger, BOOL* stop) {
        return [[obj gaiaID] isEqualToString:gaiaID];
      }];
  if (index == NSNotFound) {
    return nil;
  }
  return [identities_ objectAtIndex:index];
}

bool FakeChromeIdentityService::HasIdentities() const {
  return [identities_ count] > 0;
}

NSArray* FakeChromeIdentityService::GetAllIdentities() const {
  return identities_;
}

NSArray* FakeChromeIdentityService::GetAllIdentitiesSortedForDisplay() const {
  return identities_;
}

void FakeChromeIdentityService::ForgetIdentity(
    ChromeIdentity* identity,
    ForgetIdentityCallback callback) {
  [identities_ removeObject:identity];
  FireIdentityListChanged();
  if (callback) {
    // Forgetting an identity is normally an asynchronous operation (that
    // require some network calls), this is replicated here by dispatching
    // it.
    ++_pendingCallback;
    dispatch_async(dispatch_get_main_queue(), ^{
      --_pendingCallback;
      callback(nil);
    });
  }
}

void FakeChromeIdentityService::GetAccessToken(
    ChromeIdentity* identity,
    const std::string& client_id,
    const std::set<std::string>& scopes,
    ios::AccessTokenCallback callback) {
  ios::AccessTokenCallback safe_callback = [callback copy];
  NSError* error = nil;
  NSDictionary* user_info = nil;
  if (_fakeMDMError) {
    // |GetAccessToken| is normally an asynchronous operation (that requires
    // some network calls), this is replicated here by dispatching it.
    error =
        [NSError errorWithDomain:@"com.google.HTTPStatus" code:-1 userInfo:nil];
    user_info = @{};
    EXPECT_CALL(*this, HandleMDMNotification(identity, user_info, _))
        .WillRepeatedly(testing::Return(true));
  }
  // |GetAccessToken| is normally an asynchronous operation (that requires some
  // network calls), this is replicated here by dispatching it.
  ++_pendingCallback;
  dispatch_async(dispatch_get_main_queue(), ^{
    --_pendingCallback;
    if (user_info)
      FireAccessTokenRefreshFailed(identity, user_info);
    // Token and expiration date. It should be larger than typical test
    // execution because tests usually setup mock to expect one token request
    // and then rely on access token being served from cache.
    NSTimeInterval expiration = 60.0;
    NSDate* expiresDate = [NSDate dateWithTimeIntervalSinceNow:expiration];
    NSString* token = [expiresDate description];

    safe_callback(token, expiresDate, error);
  });
}

UIImage* FakeChromeIdentityService::GetCachedAvatarForIdentity(
    ChromeIdentity* identity) {
  return FakeGetCachedAvatarForIdentity(identity);
}

void FakeChromeIdentityService::GetAvatarForIdentity(
    ChromeIdentity* identity,
    GetAvatarCallback callback) {
  if (!callback) {
    return;
  }
  // |GetAvatarForIdentity| is normally an asynchronous operation, this is
  // replicated here by dispatching it.
  ++_pendingCallback;
  dispatch_async(dispatch_get_main_queue(), ^{
    --_pendingCallback;
    callback(FakeGetCachedAvatarForIdentity(identity));
  });
}

void FakeChromeIdentityService::GetHostedDomainForIdentity(
    ChromeIdentity* identity,
    GetHostedDomainCallback callback) {
  NSString* domain = FakeGetHostedDomainForIdentity(identity);
  // |GetHostedDomainForIdentity| is normally an asynchronous operation , this
  // is replicated here by dispatching it.
  ++_pendingCallback;
  dispatch_async(dispatch_get_main_queue(), ^{
    --_pendingCallback;
    callback(domain, nil);
  });
}

NSString* FakeChromeIdentityService::GetCachedHostedDomainForIdentity(
    ChromeIdentity* identity) {
  NSString* domain =
      ChromeIdentityService::GetCachedHostedDomainForIdentity(identity);
  if (domain) {
    return domain;
  }
  return FakeGetHostedDomainForIdentity(identity);
}

void FakeChromeIdentityService::SetUpForIntegrationTests() {}

void FakeChromeIdentityService::AddIdentities(NSArray* identitiesNames) {
  for (NSString* name in identitiesNames) {
    NSString* email = [NSString stringWithFormat:kIdentityEmailFormat, name];
    NSString* gaiaID = [NSString stringWithFormat:kIdentityGaiaIDFormat, name];
    [identities_ addObject:[FakeChromeIdentity identityWithEmail:email
                                                          gaiaID:gaiaID
                                                            name:name]];
  }
}

void FakeChromeIdentityService::AddIdentity(ChromeIdentity* identity) {
  if (![identities_ containsObject:identity]) {
    [identities_ addObject:identity];
  }
  FireIdentityListChanged();
}

void FakeChromeIdentityService::RemoveIdentity(ChromeIdentity* identity) {
  if ([identities_ indexOfObject:identity] != NSNotFound) {
    [identities_ removeObject:identity];
    FireIdentityListChanged();
  }
}

void FakeChromeIdentityService::SetFakeMDMError(bool fakeMDMError) {
  _fakeMDMError = fakeMDMError;
}

bool FakeChromeIdentityService::HasPendingCallback() {
  return _pendingCallback > 0;
}

}  // namespace ios
