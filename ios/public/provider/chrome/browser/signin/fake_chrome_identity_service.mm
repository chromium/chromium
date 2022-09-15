// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"

#import <Foundation/Foundation.h>

#import <objc/runtime.h>

#import "base/command_line.h"
#import "base/mac/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "google_apis/gaia/gaia_auth_util.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_interaction_manager.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service_constants.h"
#import "ios/public/provider/chrome/browser/signin/signin_resources_api.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using ::testing::_;
using ::testing::Invoke;
using base::test::ios::kWaitForUIElementTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace {

NSString* const kIdentityEmailFormat = @"%@@gmail.com";
NSString* const kIdentityGaiaIDFormat = @"%@ID";

NSString* FakeGetHostedDomainForIdentity(id<SystemIdentity> identity) {
  return base::SysUTF8ToNSString(gaia::ExtractDomainName(
      gaia::CanonicalizeEmail(base::SysNSStringToUTF8(identity.userEmail))));
}

// Key for the cached identity avatar. The tests validate that the avatar is
// cached by comparing pointers. As ios::provider::GetSigninDefaultAvatar()
// may return a new image each time it is called, it needs to be cached here
// using objc_getAssociatedObject/objc_setAssociatedObject.
const char kCachedAvatarAssociatedKey[] = "CachedAvatarAssociatedKey";

// Get cached identity avatar. May return nil if no image is cached.
UIImage* GetCachedAvatarForIdentity(id<SystemIdentity> identity) {
  return base::mac::ObjCCastStrict<UIImage>(
      objc_getAssociatedObject(identity, &kCachedAvatarAssociatedKey));
}

// Set cached identity avatar.
void SetCachedAvatarForIdentity(id<SystemIdentity> identity, UIImage* avatar) {
  objc_setAssociatedObject(identity, &kCachedAvatarAssociatedKey, avatar,
                           OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

}  // anonymous namespace

@interface FakeAccountDetailsViewController : UIViewController {
  __weak ChromeIdentity* _identity;
  UIButton* _removeAccountButton;
  UIButton* _closeAccountDetailsButton;
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
  [_closeAccountDetailsButton removeTarget:self
                                    action:@selector(didTapCloseAccount:)
                          forControlEvents:UIControlEventTouchUpInside];
}

- (void)viewDidLoad {
  [super viewDidLoad];

  // Obnoxious color, this is a test screen.
  self.view.backgroundColor = [UIColor orangeColor];

  _removeAccountButton = [UIButton buttonWithType:UIButtonTypeCustom];
  [self addButtonToSubviewWithTitle:@"Remove account"
                             button:_removeAccountButton
                             action:@selector(didTapRemoveAccount:)];

  _closeAccountDetailsButton = [UIButton buttonWithType:UIButtonTypeCustom];
  [self addButtonToSubviewWithTitle:@"Close account"
                             button:_closeAccountDetailsButton
                             action:@selector(didTapCloseAccount:)];
}

- (void)viewWillLayoutSubviews {
  [super viewWillLayoutSubviews];

  CGRect bounds = self.view.bounds;
  [self sizeButtonToFitWithCenter:CGPointMake(CGRectGetMidX(bounds),
                                              CGRectGetMinY(bounds))
                           button:_removeAccountButton];
  [self sizeButtonToFitWithCenter:CGPointMake(CGRectGetMidX(bounds),
                                              CGRectGetMidY(bounds))
                           button:_closeAccountDetailsButton];
}

#pragma mark - Private

- (void)addButtonToSubviewWithTitle:(NSString*)title
                             button:(UIButton*)button
                             action:(SEL)action {
  [button setTitle:title forState:UIControlStateNormal];
  [button addTarget:self
                action:action
      forControlEvents:UIControlEventTouchUpInside];
  [self.view addSubview:button];
}

- (void)sizeButtonToFitWithCenter:(CGPoint)center button:(UIButton*)button {
  [button setCenter:center];
  [button sizeToFit];
}

- (void)didTapRemoveAccount:(id)sender {
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()
      ->ForgetIdentity(_identity, ^(NSError*) {
        [self dismissViewControllerAnimated:YES completion:nil];
      });
}

- (void)didTapCloseAccount:(id)sender {
  [self dismissViewControllerAnimated:YES completion:nil];
}

@end

namespace ios {
FakeChromeIdentityService::FakeChromeIdentityService()
    : identities_([[NSMutableArray alloc] init]),
      capabilitiesByIdentity_([[NSMutableDictionary alloc] init]),
      _fakeMDMError(false),
      _pendingCallback(0) {
  std::string value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kAddFakeIdentitiesArg);
  NSArray* identities = [FakeChromeIdentity identitiesFromBase64String:value];
  if (identities) {
    [identities_ addObjectsFromArray:identities];
  }
}

FakeChromeIdentityService::~FakeChromeIdentityService() {}

// static
FakeChromeIdentityService*
FakeChromeIdentityService::GetInstanceFromChromeProvider() {
  return static_cast<ios::FakeChromeIdentityService*>(
      ios::GetChromeBrowserProvider().GetChromeIdentityService());
}

DismissASMViewControllerBlock
FakeChromeIdentityService::PresentAccountDetailsController(
    id<SystemIdentity> identity,
    UIViewController* viewController,
    BOOL animated) {
  UIViewController* accountDetailsViewController =
      [[FakeAccountDetailsViewController alloc] initWithIdentity:identity];
  [viewController presentViewController:accountDetailsViewController
                               animated:animated
                             completion:nil];
  return ^(BOOL dismissAnimated) {
    [accountDetailsViewController dismissViewControllerAnimated:dismissAnimated
                                                     completion:nil];
  };
}

ChromeIdentityInteractionManager*
FakeChromeIdentityService::CreateChromeIdentityInteractionManager() const {
  return CreateFakeChromeIdentityInteractionManager();
}

FakeChromeIdentityInteractionManager*
FakeChromeIdentityService::CreateFakeChromeIdentityInteractionManager() const {
  return [[FakeChromeIdentityInteractionManager alloc] init];
}

void FakeChromeIdentityService::IterateOverIdentities(
    SystemIdentityIteratorCallback callback) {
  for (id<SystemIdentity> identity in identities_) {
    if (callback.Run(identity) == kIdentityIteratorInterruptIteration)
      return;
  }
}

void FakeChromeIdentityService::ForgetIdentity(
    id<SystemIdentity> identity,
    ForgetIdentityCallback callback) {
  [identities_ removeObject:identity];
  [capabilitiesByIdentity_ removeObjectForKey:identity.gaiaID];
  FireIdentityListChanged(/*notify_user=*/false);
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
    id<SystemIdentity> identity,
    const std::string& client_id,
    const std::set<std::string>& scopes,
    ios::AccessTokenCallback callback) {
  ios::AccessTokenCallback safe_callback = [callback copy];
  NSError* error = nil;
  NSDictionary* user_info = nil;
  if (_fakeMDMError) {
    // `GetAccessToken` is normally an asynchronous operation (that requires
    // some network calls), this is replicated here by dispatching it.
    error =
        [NSError errorWithDomain:@"com.google.HTTPStatus" code:-1 userInfo:nil];
    user_info = @{};
    EXPECT_CALL(*this, HandleMDMNotification(identity, user_info, _))
        .WillRepeatedly(testing::Return(true));
  }
  // `GetAccessToken` is normally an asynchronous operation (that requires some
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
    id<SystemIdentity> identity) {
  return ::GetCachedAvatarForIdentity(identity);
}

void FakeChromeIdentityService::GetAvatarForIdentity(
    id<SystemIdentity> identity) {
  // `GetAvatarForIdentity` is normally an asynchronous operation, this is
  // replicated here by dispatching it.
  ++_pendingCallback;
  dispatch_async(dispatch_get_main_queue(), ^{
    --_pendingCallback;
    UIImage* avatar = ::GetCachedAvatarForIdentity(identity);
    if (!avatar) {
      avatar = ios::provider::GetSigninDefaultAvatar();
      ::SetCachedAvatarForIdentity(identity, avatar);
    }
    FireProfileDidUpdate(identity);
  });
}

void FakeChromeIdentityService::GetHostedDomainForIdentity(
    id<SystemIdentity> identity,
    GetHostedDomainCallback callback) {
  NSString* domain = FakeGetHostedDomainForIdentity(identity);
  // `GetHostedDomainForIdentity` is normally an asynchronous operation , this
  // is replicated here by dispatching it.
  ++_pendingCallback;
  dispatch_async(dispatch_get_main_queue(), ^{
    --_pendingCallback;
    callback(domain, nil);
  });
}

bool FakeChromeIdentityService::IsServiceSupported() {
  return true;
}

NSString* FakeChromeIdentityService::GetCachedHostedDomainForIdentity(
    id<SystemIdentity> identity) {
  NSString* domain =
      ChromeIdentityService::GetCachedHostedDomainForIdentity(identity);
  if (domain) {
    return domain;
  }
  return FakeGetHostedDomainForIdentity(identity);
}

void FakeChromeIdentityService::SimulateForgetIdentityFromOtherApp(
    id<SystemIdentity> identity) {
  [identities_ removeObject:identity];
  [capabilitiesByIdentity_ removeObjectForKey:identity.gaiaID];
  FireChromeIdentityReload();
}

void FakeChromeIdentityService::FireChromeIdentityReload() {
  FireIdentityListChanged(/*notify_user=*/true);
}

void FakeChromeIdentityService::AddManagedIdentities(NSArray* identitiesNames) {
  for (NSString* name in identitiesNames) {
    NSString* email =
        [NSString stringWithFormat:@"%@%@", name, kManagedIdentityEmailSuffix];
    NSString* gaiaID = [NSString stringWithFormat:kIdentityGaiaIDFormat, name];
    [identities_ addObject:[FakeChromeIdentity identityWithEmail:email
                                                          gaiaID:gaiaID
                                                            name:name]];
  }
}

void FakeChromeIdentityService::AddIdentities(NSArray* identitiesNames) {
  for (NSString* name in identitiesNames) {
    NSString* email = [NSString stringWithFormat:kIdentityEmailFormat, name];
    NSString* gaiaID = [NSString stringWithFormat:kIdentityGaiaIDFormat, name];
    [identities_ addObject:[FakeChromeIdentity identityWithEmail:email
                                                          gaiaID:gaiaID
                                                            name:name]];
  }
}

void FakeChromeIdentityService::AddIdentity(id<SystemIdentity> identity) {
  if (![identities_ containsObject:identity]) {
    [identities_ addObject:identity];
  }
  FireIdentityListChanged(/*notify_user=*/false);
}

void FakeChromeIdentityService::SetCapabilities(id<SystemIdentity> identity,
                                                NSDictionary* capabilities) {
  DCHECK([identities_ containsObject:identity]);
  [capabilitiesByIdentity_ setObject:capabilities forKey:identity.gaiaID];
}

void FakeChromeIdentityService::SetFakeMDMError(bool fakeMDMError) {
  _fakeMDMError = fakeMDMError;
}

bool FakeChromeIdentityService::WaitForServiceCallbacksToComplete() {
  ConditionBlock condition = ^() {
    return _pendingCallback == 0;
  };
  return WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, condition);
}

void FakeChromeIdentityService::TriggerIdentityUpdateNotification(
    id<SystemIdentity> identity) {
  FireProfileDidUpdate(identity);
}

void FakeChromeIdentityService::FetchCapabilities(
    id<SystemIdentity> identity,
    NSArray<NSString*>* capabilities,
    ChromeIdentityCapabilitiesFetchCompletionBlock completion) {
  NSMutableDictionary* result = [[NSMutableDictionary alloc] init];
  NSDictionary* capabilitiesForIdentity =
      capabilitiesByIdentity_[identity.gaiaID];
  for (NSString* capability : capabilities) {
    // Set capability result as unknown if not set in capabilitiesByIdentity_.
    NSNumber* capabilityResult =
        [NSNumber numberWithInt:static_cast<int>(
                                    ChromeIdentityCapabilityResult::kUnknown)];
    if ([capabilitiesForIdentity objectForKey:capability]) {
      capabilityResult = capabilitiesForIdentity[capability];
    }
    [result setObject:capabilityResult forKey:capability];
  }
  if (completion) {
    completion(result, nil);
  }
}

}  // namespace ios
