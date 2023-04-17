// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/set_up_list.h"

#import "components/password_manager/core/browser/password_manager_util.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/default_browser/utils.h"
#import "ios/chrome/browser/ntp/set_up_list_item.h"
#import "ios/chrome/browser/ntp/set_up_list_item_type.h"
#import "ios/chrome/browser/signin/authentication_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Returns the SignInSync item if the conditions are right, or nil.
SetUpListItem* signInSyncItem(AuthenticationService* auth_service) {
  if (auth_service->HasPrimaryIdentity(signin::ConsentLevel::kSignin)) {
    return nil;
  }
  return [[SetUpListItem alloc] initWithType:SetUpListItemType::kSignInSync
                                    complete:NO];
}

// Returns the DefaultBrowser item if the conditions are right, or nil.
SetUpListItem* defaultBrowserItem() {
  if (IsChromeLikelyDefaultBrowser()) {
    return nil;
  }
  return [[SetUpListItem alloc] initWithType:SetUpListItemType::kDefaultBrowser
                                    complete:NO];
}

// Returns the Autofill item if the conditions are right, or nil.
SetUpListItem* autofillItem(PrefService* prefs) {
  if (password_manager_util::IsCredentialProviderEnabledOnStartup(prefs)) {
    return nil;
  }
  return [[SetUpListItem alloc] initWithType:SetUpListItemType::kAutofill
                                    complete:NO];
}

// Adds the item to the array if it is not `nil`.
void AddItemIfNotNil(NSMutableArray* array, id item) {
  if (item) {
    [array addObject:item];
  }
}
}  // namespace

@implementation SetUpList

+ (instancetype)buildFromPrefs:(PrefService*)prefs
         authenticationService:(AuthenticationService*)authService {
  NSMutableArray<SetUpListItem*>* items =
      [[NSMutableArray<SetUpListItem*> alloc] init];
  AddItemIfNotNil(items, signInSyncItem(authService));
  AddItemIfNotNil(items, defaultBrowserItem());
  AddItemIfNotNil(items, autofillItem(prefs));
  // TODO(crbug.com/1428070): Add a Follow item to the Set Up List.
  return [[self alloc] initWithItems:items];
}

- (instancetype)initWithItems:(NSArray<SetUpListItem*>*)items {
  self = [super init];
  if (self) {
    _items = items;
  }
  return self;
}

@end
