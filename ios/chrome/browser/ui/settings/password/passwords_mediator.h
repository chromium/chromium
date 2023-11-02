// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_MEDIATOR_H_

#import <Foundation/Foundation.h>

#include "base/memory/scoped_refptr.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#include "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/ui/settings/password/password_manager_view_controller_delegate.h"
#import "ios/chrome/browser/ui/settings/utils/password_auto_fill_status_observer.h"
#import "ios/chrome/browser/ui/table_view/table_view_favicon_data_source.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"

class FaviconLoader;
class IOSChromePasswordCheckManager;
@protocol PasswordsConsumer;
class SyncSetupService;

namespace password_manager {
struct CredentialUIEntry;
}

// This mediator fetches and organises the passwords for its consumer.
@interface PasswordsMediator : NSObject <PasswordAutoFillStatusObserver,
                                         PasswordManagerViewControllerDelegate,
                                         SuccessfulReauthTimeAccessor,
                                         TableViewFaviconDataSource>

- (instancetype)
    initWithPasswordCheckManager:
        (scoped_refptr<IOSChromePasswordCheckManager>)passwordCheckManager
                syncSetupService:(SyncSetupService*)syncSetupService
                   faviconLoader:(FaviconLoader*)faviconLoader
                 identityManager:(signin::IdentityManager*)identityManager
                     syncService:(syncer::SyncService*)syncService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Deletes 'form' and its duplicates.
- (void)deleteCredential:(const password_manager::CredentialUIEntry&)credential;

// Disconnect the observers.
- (void)disconnect;

@property(nonatomic, weak) id<PasswordsConsumer> consumer;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_MEDIATOR_H_
