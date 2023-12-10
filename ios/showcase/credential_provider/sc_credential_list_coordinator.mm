// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/credential_provider/sc_credential_list_coordinator.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/common/credential_provider/credential.h"
#import "ios/chrome/credential_provider_extension/ui/credential_details_consumer.h"
#import "ios/chrome/credential_provider_extension/ui/credential_details_view_controller.h"
#import "ios/chrome/credential_provider_extension/ui/credential_list_consumer.h"
#import "ios/chrome/credential_provider_extension/ui/credential_list_view_controller.h"

@interface SCCredential : NSObject <Credential>
@end

@implementation SCCredential
@synthesize favicon = _favicon;
@synthesize password = _password;
@synthesize rank = _rank;
@synthesize recordIdentifier = _recordIdentifier;
@synthesize serviceIdentifier = _serviceIdentifier;
@synthesize serviceName = _serviceName;
@synthesize user = _user;
@synthesize note = _note;

- (instancetype)initWithServiceName:(NSString*)serviceName
                  serviceIdentifier:(NSString*)serviceIdentifier
                               user:(NSString*)user {
  self = [super init];
  if (self) {
    _serviceName = serviceName;
    _serviceIdentifier = serviceIdentifier;
    _user = user;
  }
  return self;
}

@end

namespace {
NSArray<id<Credential>>* suggestedPasswords = @[
  [[SCCredential alloc] initWithServiceName:@"domain.com"
                          serviceIdentifier:@"www.domain.com"
                                       user:@"johnsmith"],
  [[SCCredential alloc] initWithServiceName:@"domain.com"
                          serviceIdentifier:@"www.domain.com"
                                       user:@"janesmythe"],
  [[SCCredential alloc] initWithServiceName:@"domain.com"
                          serviceIdentifier:@"www.domain.com"
                                       user:@"johnsmith"],
  [[SCCredential alloc] initWithServiceName:@"domain.com"
                          serviceIdentifier:@"www.domain.com"
                                       user:@"janesmythe"],
];
NSArray<id<Credential>>* allPasswords = @[
  [[SCCredential alloc] initWithServiceName:@"domain1.com"
                          serviceIdentifier:@"www.domain1.com"
                                       user:@"jsmythe@fazebook.com"],
  [[SCCredential alloc] initWithServiceName:@"domain2.com"
                          serviceIdentifier:@"www.domain2.com"
                                       user:@"jasmith@twitcher.com"],
  [[SCCredential alloc] initWithServiceName:@"domain3.com"
                          serviceIdentifier:@"www.domain3.com"
                                       user:@"HughZername"],
  [[SCCredential alloc] initWithServiceName:@"domain1.com"
                          serviceIdentifier:@"www.domain1.com"
                                       user:@"jsmythe@fazebook.com"],
  [[SCCredential alloc] initWithServiceName:@"domain2.com"
                          serviceIdentifier:@"www.domain2.com"
                                       user:@"jasmith@twitcher.com"],
  [[SCCredential alloc] initWithServiceName:@"domain3.com"
                          serviceIdentifier:@"www.domain3.com"
                                       user:@"HughZername"],
];
}

@interface SCCredentialListCoordinator () <CredentialDetailsConsumerDelegate,
                                           CredentialListHandler>
@property(nonatomic, strong) CredentialListViewController* viewController;
@end

@implementation SCCredentialListCoordinator
@synthesize baseViewController = _baseViewController;
@synthesize viewController = _viewController;

- (void)start {
  self.viewController = [[CredentialListViewController alloc] init];
  self.viewController.delegate = self;
  [self.baseViewController setHidesBarsOnSwipe:NO];
  [self.baseViewController pushViewController:self.viewController animated:YES];

  [self.viewController presentSuggestedPasswords:suggestedPasswords
                                    allPasswords:allPasswords
                                   showSearchBar:YES
                           showNewPasswordOption:NO];
}

#pragma mark - CredentialListHandler

- (void)navigationCancelButtonWasPressed:(UIButton*)button {
}

- (void)updateResultsWithFilter:(NSString*)filter {
  NSMutableArray<id<Credential>>* suggested = [[NSMutableArray alloc] init];
  for (id<Credential> credential in suggestedPasswords) {
    if ([filter length] == 0 ||
        [credential.serviceName localizedStandardContainsString:filter] ||
        [credential.user localizedStandardContainsString:filter]) {
      [suggested addObject:credential];
    }
  }
  NSMutableArray<id<Credential>>* all = [[NSMutableArray alloc] init];
  for (id<Credential> credential in allPasswords) {
    if ([filter length] == 0 ||
        [credential.serviceName localizedStandardContainsString:filter] ||
        [credential.user localizedStandardContainsString:filter]) {
      [all addObject:credential];
    }
  }
  [self.viewController presentSuggestedPasswords:suggested
                                    allPasswords:all
                                   showSearchBar:YES
                           showNewPasswordOption:NO];
}

- (void)userSelectedCredential:(id<Credential>)credential {
}

- (void)showDetailsForCredential:(id<Credential>)credential {
  CredentialDetailsViewController* detailsViewController =
      [[CredentialDetailsViewController alloc] init];
  detailsViewController.delegate = self;
  [detailsViewController presentCredential:credential];
  [self.baseViewController pushViewController:detailsViewController
                                     animated:YES];
}

- (void)newPasswordWasSelected {
}

#pragma mark - CredentialDetailsConsumerDelegate

- (void)unlockPasswordForCredential:(id<Credential>)credential
                  completionHandler:(void (^)(NSString*))completionHandler {
  completionHandler(@"DreamOn");
}

@end
