// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_ISSUES_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_ISSUES_MEDIATOR_H_

#import <Foundation/Foundation.h>

#include "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/ui/table_view/table_view_favicon_data_source.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"

namespace password_manager {
struct CredentialUIEntry;
}  // namespace password_manager

class FaviconLoader;
class IOSChromePasswordCheckManager;
@protocol PasswordIssuesConsumer;

// This mediator fetches and organises the credentials for its consumer.
@interface PasswordIssuesMediator
    : NSObject <SuccessfulReauthTimeAccessor, TableViewFaviconDataSource>

- (instancetype)initWithPasswordCheckManager:
                    (IOSChromePasswordCheckManager*)manager
                               faviconLoader:(FaviconLoader*)faviconLoader
                                 syncService:(syncer::SyncService*)syncService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@property(nonatomic, weak) id<PasswordIssuesConsumer> consumer;

// Deletes password from the password store.
- (void)deleteCredential:(const password_manager::CredentialUIEntry&)credential;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_ISSUES_MEDIATOR_H_
