// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_COORDINATOR_AUTOFILL_AI_ENTITY_EDIT_MEDIATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_COORDINATOR_AUTOFILL_AI_ENTITY_EDIT_MEDIATOR_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_mutator.h"
#import "url/gurl.h"

namespace autofill {
class EntityDataManager;
class EntityInstance;
class WalletPassAccessManager;
}  // namespace autofill

namespace consent_auditor {
class ConsentAuditor;
}
namespace signin {
class IdentityManager;
}

@protocol AutofillAIEntityEditConsumer;
@protocol ReauthenticationProtocol;

@class CountryItem;
@class TableViewItem;
@class AutofillAIEntityCountryItem;
@class AutofillAIEntityEditDateItem;

@interface AutofillAIEntityEditMediator : NSObject <AutofillAIEntityEditMutator>

// The consumer of this mediator.
@property(nonatomic, weak) id<AutofillAIEntityEditConsumer> consumer;

// The fetched country list.
@property(nonatomic, strong, readonly) NSArray<CountryItem*>* allCountries;

- (instancetype)
    initWithEntityInstance:(autofill::EntityInstance)entityInstance
         entityDataManager:(autofill::EntityDataManager*)entityDataManager
         walletPassManager:(autofill::WalletPassAccessManager*)walletPassManager
            consentAuditor:(consent_auditor::ConsentAuditor*)consentAuditor
           identityManager:(signin::IdentityManager*)identityManager
              reauthModule:(id<ReauthenticationProtocol>)reauthModule
                 userEmail:(NSString*)userEmail NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Called when a country is selected.
- (void)didSelectCountry:(CountryItem*)countryItem
                 forItem:(AutofillAIEntityCountryItem*)item;

// Returns the URL to manage the Server Wallet item.
- (GURL)walletManagementURL;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_COORDINATOR_AUTOFILL_AI_ENTITY_EDIT_MEDIATOR_H_
