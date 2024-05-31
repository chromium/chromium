// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_address_mediator.h"

#import "base/i18n/message_formatter.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/data_model/autofill_profile.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/address_consumer.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/address_list_delegate.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_action_cell.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_address+AutofillProfile.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_address.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_address_cell.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_content_injector.h"
#import "ios/chrome/browser/ui/menu/browser_action_factory.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

using autofill::AutofillProfile;

namespace manual_fill {
NSString* const ManageAddressAccessibilityIdentifier =
    @"kManualFillManageAddressAccessibilityIdentifier";
}  // namespace manual_fill

@interface ManualFillAddressMediator ()

// All available addresses.
@property(nonatomic, assign) std::vector<const AutofillProfile*> addresses;

@end

@implementation ManualFillAddressMediator

- (instancetype)initWithProfiles:(std::vector<const AutofillProfile*>)profiles {
  self = [super init];
  if (self) {
    _addresses = std::move(profiles);
  }
  return self;
}

- (void)setConsumer:(id<ManualFillAddressConsumer>)consumer {
  if (consumer == _consumer) {
    return;
  }
  _consumer = consumer;
  [self postAddressesToConsumer];
  [self postActionsToConsumer];
}

- (void)reloadWithProfiles:(std::vector<const AutofillProfile*>)profiles {
  self.addresses = profiles;
  if (self.consumer) {
    [self postAddressesToConsumer];
    [self postActionsToConsumer];
  }
}

#pragma mark - Private

// Posts the addresses to the consumer.
- (void)postAddressesToConsumer {
  if (!self.consumer) {
    return;
  }

  int addressCount = self.addresses.size();
  NSMutableArray* items =
      [[NSMutableArray alloc] initWithCapacity:addressCount];
  for (int i = 0; i < addressCount; i++) {
    ManualFillAddress* manualFillAddress =
        [[ManualFillAddress alloc] initWithProfile:*self.addresses[i]];

    NSArray<UIAction*>* menuActions =
        IsKeyboardAccessoryUpgradeEnabled()
            ? @[ [self createMenuEditActionForAddress:self.addresses[i]] ]
            : @[];

    NSString* cellIndexAccessibilityLabel = base::SysUTF16ToNSString(
        base::i18n::MessageFormatter::FormatWithNamedArgs(
            l10n_util::GetStringUTF16(
                IDS_IOS_MANUAL_FALLBACK_ADDRESS_CELL_INDEX),
            "count", addressCount, "position", i + 1));

    auto item = [[ManualFillAddressItem alloc]
                    initWithAddress:manualFillAddress
                    contentInjector:self.contentInjector
                        menuActions:menuActions
        cellIndexAccessibilityLabel:cellIndexAccessibilityLabel];
    [items addObject:item];
  }

  [self.consumer presentAddresses:items];
}

- (void)postActionsToConsumer {
  if (!self.consumer) {
    return;
  }

  NSString* manageAddressesTitle =
      l10n_util::GetNSString(IDS_IOS_MANUAL_FALLBACK_MANAGE_ADDRESSES);
  __weak __typeof(self) weakSelf = self;
  ManualFillActionItem* manageAddressesItem = [[ManualFillActionItem alloc]
      initWithTitle:manageAddressesTitle
             action:^{
               base::RecordAction(base::UserMetricsAction(
                   "ManualFallback_Profiles_OpenManageProfiles"));
               [weakSelf.navigationDelegate openAddressSettings];
             }];
  manageAddressesItem.accessibilityIdentifier =
      manual_fill::ManageAddressAccessibilityIdentifier;
  [self.consumer presentActions:@[ manageAddressesItem ]];
}

// Creates a UIAction to edit an address from a UIMenu.
- (UIAction*)createMenuEditActionForAddress:(const AutofillProfile*)address {
  ActionFactory* actionFactory = [[ActionFactory alloc]
      initWithScenario:
          kMenuScenarioHistogramAutofillManualFallbackAddressEntry];

  UIAction* editAction = [actionFactory actionToEditWithBlock:^{
      // TODO(crbug.com/326413640): Handle tap.
  }];

  return editAction;
}

@end
