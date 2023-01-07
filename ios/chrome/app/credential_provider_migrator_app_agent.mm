// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/credential_provider_migrator_app_agent.h"

#import "components/keyed_service/core/service_access_type.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/credential_provider/credential_provider_migrator.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#import "ios/chrome/browser/ui/main/browser_interface_provider.h"
#import "ios/chrome/browser/ui/main/scene_state.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/credential_provider/constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CredentialProviderAppAgent ()

// `migrator` is in charge of migrating the password when Chrome comes to
// foreground.
@property(nonatomic, strong) CredentialProviderMigrator* migrator;

@end

@implementation CredentialProviderAppAgent

- (void)appDidEnterForeground {
  if (self.migrator) {
    return;
  }
  NSString* key = AppGroupUserDefaultsCredentialProviderNewCredentials();
  SceneState* anyScene = self.appState.foregroundScenes.firstObject;
  DCHECK(anyScene);
  ChromeBrowserState* browserState =
      anyScene.interfaceProvider.mainInterface.browserState;
  DCHECK(browserState);
  scoped_refptr<password_manager::PasswordStoreInterface> store =
      IOSChromePasswordStoreFactory::GetForBrowserState(
          browserState, ServiceAccessType::IMPLICIT_ACCESS);
  NSUserDefaults* userDefaults = app_group::GetGroupUserDefaults();
  self.migrator =
      [[CredentialProviderMigrator alloc] initWithUserDefaults:userDefaults
                                                           key:key
                                                 passwordStore:store];
  __weak __typeof__(self) weakSelf = self;
  [self.migrator startMigrationWithCompletion:^(BOOL success, NSError* error) {
    DCHECK(success);
    weakSelf.migrator = nil;
  }];
}

@end
