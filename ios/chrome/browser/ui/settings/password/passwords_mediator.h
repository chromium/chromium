// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_MEDIATOR_H_

#import <Foundation/Foundation.h>

#include "base/memory/scoped_refptr.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_favicon_data_source.h"
#import "ios/chrome/browser/ui/settings/password/password_manager_view_controller_delegate.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"

class FaviconLoader;
class IOSChromePasswordCheckManager;
@protocol PasswordsConsumer;
class PrefService;

namespace syncer {
class SyncService;
}

namespace feature_engagement {
class Tracker;
}

// This mediator fetches and organises the passwords for its consumer.
@interface PasswordsMediator : NSObject <PasswordManagerViewControllerDelegate,
                                         SuccessfulReauthTimeAccessor,
                                         TableViewFaviconDataSource>

- (instancetype)initWithPasswordCheckManager:
                    (scoped_refptr<IOSChromePasswordCheckManager>)
                        passwordCheckManager
                               faviconLoader:(FaviconLoader*)faviconLoader
                                 syncService:(syncer::SyncService*)syncService
                                 prefService:(PrefService*)prefService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnect the observers.
- (void)disconnect;

// Ask the Feature Engagement Tracker whether or not the Password Manager widget
// promo can be shown.
- (void)askFETToShowPasswordManagerWidgetPromo;

@property(nonatomic, weak) id<PasswordsConsumer> consumer;

// Feature Engagement Tracker used to handle promo events.
@property(nonatomic, assign) feature_engagement::Tracker* tracker;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_MEDIATOR_H_
