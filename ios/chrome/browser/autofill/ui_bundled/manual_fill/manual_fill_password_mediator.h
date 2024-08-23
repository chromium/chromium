// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_PASSWORD_MEDIATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_PASSWORD_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "base/memory/ref_counted.h"
#import "components/password_manager/core/browser/password_store/password_store_interface.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_favicon_data_source.h"

@protocol ManualFillContentInjector;
@class ManualFillPasswordMediator;
@protocol ManualFillPasswordConsumer;
@protocol PasswordListNavigator;

namespace password_manager {
struct CredentialUIEntry;
class SavedPasswordsPresenter;
}  // namespace password_manager

namespace syncer {
class SyncService;
}  // namespace syncer

namespace web {
class WebState;
}  // namespace web

class FaviconLoader;
class GURL;

// Delegate for the password mediator.
@protocol ManualFillPasswordMediatorDelegate <NSObject>

// The mediator will attempt to inject content.
- (void)manualFillPasswordMediatorWillInjectContent:
    (ManualFillPasswordMediator*)mediator;

// Requests the delegate to open the details of a credential in edit mode.
- (void)manualFillPasswordMediator:(ManualFillPasswordMediator*)mediator
    didTriggerOpenPasswordDetailsInEditMode:
        (password_manager::CredentialUIEntry)credential;

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

// The designated initializer. `profilePasswordStore` and `accountPasswordStore`
// are used to create a PasswordCounterObserver, which is used by this mediator
// to ultimately determine whether or not a certain manual filling action should
// be made available. If this mediator is created to show the full list of saved
// passwords, and not to show the manual filling options for the current site,
// then no manual filling actions will be shown. In this case,
// `profilePasswordStore` and `accountPasswordStore` can be set to `nil`. Note:
// A valid `profilePasswordStore` is required to create a
// PasswordCounterObserver.
- (instancetype)initWithFaviconLoader:(FaviconLoader*)faviconLoader
                             webState:(web::WebState*)webState
                          syncService:(syncer::SyncService*)syncService
                                  URL:(const GURL&)URL
             invokedOnObfuscatedField:(BOOL)invokedOnObfuscatedField
                 profilePasswordStore:
                     (scoped_refptr<password_manager::PasswordStoreInterface>)
                         profilePasswordStore
                 accountPasswordStore:
                     (scoped_refptr<password_manager::PasswordStoreInterface>)
                         accountPasswordStore
               showAutofillFormButton:(BOOL)showAutofillFormButton
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Sets the saved passwords presenter.
- (void)setSavedPasswordsPresenter:
    (password_manager::SavedPasswordsPresenter*)savedPasswordsPresenter;

// Fetches passwords related to the current site.
- (void)fetchPasswordsForOrigin;

// Fetches all saved passwords.
- (void)fetchAllPasswords;

// Detaches observers.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_PASSWORD_MEDIATOR_H_
