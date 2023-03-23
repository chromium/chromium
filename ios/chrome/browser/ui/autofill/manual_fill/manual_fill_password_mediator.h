// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_PASSWORD_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_PASSWORD_MEDIATOR_H_

#import <UIKit/UIKit.h>

#include "base/memory/ref_counted.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_favicon_data_source.h"

@protocol ManualFillContentInjector;
@class ManualFillPasswordMediator;
@protocol ManualFillPasswordConsumer;
@protocol PasswordListNavigator;

namespace password_manager {
class PasswordStoreInterface;
}  // namespace password_manager

namespace web {
class WebState;
}  // namespace web

class FaviconLoader;
class GURL;
class SyncSetupService;

namespace manual_fill {

extern NSString* const ManagePasswordsAccessibilityIdentifier;
extern NSString* const ManageSettingsAccessibilityIdentifier;
extern NSString* const OtherPasswordsAccessibilityIdentifier;
extern NSString* const SuggestPasswordAccessibilityIdentifier;

}  // namespace manual_fill

// Delegate for the password mediator.
@protocol ManualFillPasswordMediatorDelegate <NSObject>
// The mediator will attempt to inject content.
- (void)manualFillPasswordMediatorWillInjectContent:
    (ManualFillPasswordMediator*)mediator;
@end

// Object in charge of getting the passwords relevant for the manual fill
// passwords UI.
@interface ManualFillPasswordMediator
    : NSObject <TableViewFaviconDataSource, UISearchResultsUpdating>

// The consumer for passwords updates. Setting it will trigger the consumer
// methods with the current data.
@property(nonatomic, weak) id<ManualFillPasswordConsumer> consumer;
// The delegate in charge of using the content selected by the user.
@property(nonatomic, weak) id<ManualFillContentInjector> contentInjector;
// The object in charge of navigation.
@property(nonatomic, weak) id<PasswordListNavigator> navigator;
// The delegate for this object.
@property(nonatomic, weak) id<ManualFillPasswordMediatorDelegate> delegate;
// If YES  actions will be post to the consumer. Set this value before
// setting the consumer, since just setting this won't trigger the consumer
// callbacks. Defaults to NO.
@property(nonatomic, assign, getter=isActionSectionEnabled)
    BOOL actionSectionEnabled;

// The designated initializer. `profilePasswordStore` must not be nil.
// TODO(crbug.com/1374242): DCHECK accountPasswordStore too and document the
// precondition after launch.
- (instancetype)
    initWithProfilePasswordStore:
        (scoped_refptr<password_manager::PasswordStoreInterface>)
            profilePasswordStore
            accountPasswordStore:
                (scoped_refptr<password_manager::PasswordStoreInterface>)
                    accountPasswordStore
                   faviconLoader:(FaviconLoader*)faviconLoader
                        webState:(web::WebState*)webState
                     syncService:(SyncSetupService*)syncService
                             URL:(const GURL&)URL
          invokedOnPasswordField:(BOOL)invokedOnPasswordField
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Fetches passwords using the URL provided at initialisation as the filter.
// If the URL is empty (invalid) it will fetch all the passwords.
- (void)fetchPasswords;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_PASSWORD_MEDIATOR_H_
