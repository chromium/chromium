// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_plus_address_mediator.h"

#import "base/i18n/message_formatter.h"
#import "base/metrics/user_metrics.h"
#import "base/ranges/algorithm.h"
#import "base/strings/sys_string_conversions.h"
#import "components/plus_addresses/plus_address_service.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_action_cell.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_constants.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_content_injector.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_plus_address.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_plus_address_cell.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_plus_address_consumer.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/plus_address_list_navigator.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/grit/ios_strings.h"
#import "net/base/registry_controlled_domains/registry_controlled_domain.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/gfx/favicon_size.h"
#import "url/gurl.h"

@implementation ManualFillPlusAddressMediator {
  // The favicon loader used in the cell.
  FaviconLoader* _faviconLoader;

  // Used to fetch plus addresses.
  plus_addresses::PlusAddressService* _plusAddressService;

  // Origin to fetch plus addresses for.
  GURL _URL;

  // The origin to which all operations should be scoped.
  url::Origin _mainFrameOrigin;

  // If YES, create plus address action is shown.
  BOOL _isPlusAddressCreationFallbackEnabled;

  // IF YES, the plus addresses have been fetched for the domain (along with its
  // affiliations).
  BOOL _plusAddressesAreFetched;
}

- (instancetype)initWithFaviconLoader:(FaviconLoader*)faviconLoader
                   plusAddressService:
                       (plus_addresses::PlusAddressService*)plusAddressService
                                  URL:(const GURL&)URL
                       isOffTheRecord:(BOOL)isOffTheRecord {
  self = [super init];
  if (self) {
    _faviconLoader = faviconLoader;
    _plusAddressService = plusAddressService;
    _URL = URL;
    _mainFrameOrigin = url::Origin::Create(URL);
    _isPlusAddressCreationFallbackEnabled =
        _plusAddressService->IsPlusAddressCreationEnabled(_mainFrameOrigin,
                                                          isOffTheRecord);
    _plusAddressesAreFetched = NO;
  }

  return self;
}

- (void)setConsumer:(id<ManualFillPlusAddressConsumer>)consumer {
  if (consumer == _consumer) {
    return;
  }
  _consumer = consumer;
  [self postPlusAddressesToConsumer];
  [self postActionsToConsumer:NO];
}

#pragma mark - TableViewFaviconDataSource

- (void)faviconForPageURL:(CrURL*)URL
               completion:(void (^)(FaviconAttributes*))completion {
  CHECK(completion);
  _faviconLoader->FaviconForPageUrlOrHost(URL.gurl, gfx::kFaviconSize,
                                          completion);
}

#pragma mark - Private

// Initiates the process of fetching and presenting Plus Addresses to the
// consumer.
- (void)postPlusAddressesToConsumer {
  if (!self.consumer) {
    return;
  }

  __weak __typeof(self) weakSelf = self;
  auto callback =
      base::BindOnce(^(std::vector<plus_addresses::PlusProfile> plusProfiles) {
        std::erase_if(plusProfiles,
                      [&](const plus_addresses::PlusProfile& plusProfile) {
                        return !plusProfile.is_confirmed;
                      });

        ManualFillPlusAddressMediator* strongSelf = weakSelf;
        if (!strongSelf) {
          return;
        }

        [strongSelf onPlusAddressesFetched:plusProfiles];
      });

  _plusAddressService->GetAffiliatedPlusProfiles(_mainFrameOrigin,
                                                 std::move(callback));
}

// Presents the fetched `plusProfiles` to the consumer.
- (void)onPlusAddressesFetched:
    (const std::vector<plus_addresses::PlusProfile>&)plusProfiles {
  _plusAddressesAreFetched = YES;
  int plusAddressesCount = (int)plusProfiles.size();
  NSMutableArray* items =
      [[NSMutableArray alloc] initWithCapacity:plusAddressesCount];

  for (int i = 0; i < plusAddressesCount; i++) {
    NSString* cellIndexAccessibilityLabel = base::SysUTF16ToNSString(
        base::i18n::MessageFormatter::FormatWithNamedArgs(
            l10n_util::GetStringUTF16(
                IDS_PLUS_ADDRESS_MANUAL_FALLBACK_CELL_INDEX_IOS),
            "count", plusAddressesCount, "position", i + 1));

    ManualFillPlusAddress* manualFillPlusAddress =
        [self createManualFillPlusAddress:base::SysUTF8ToNSString(
                                              *plusProfiles[i].plus_address)];
    ManualFillPlusAddressItem* item = [[ManualFillPlusAddressItem alloc]
                initWithPlusAddress:manualFillPlusAddress
                    contentInjector:_contentInjector
                        menuActions:@[]
        cellIndexAccessibilityLabel:cellIndexAccessibilityLabel];
    [items addObject:item];
  }

  [self.consumer presentPlusAddresses:items];
  [self postActionsToConsumer:(items.count > 0)];
}

// Creates `ManualFillPlusAddress` from `plusAddress`.
- (ManualFillPlusAddress*)createManualFillPlusAddress:(NSString*)plusAddress {
  std::string host = _URL.host();
  std::string site_name =
      net::registry_controlled_domains::GetDomainAndRegistry(
          host, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  NSString* siteName = base::SysUTF8ToNSString(site_name);
  NSString* plusAddressHost = base::SysUTF8ToNSString(host);
  if ([plusAddressHost hasPrefix:@"www."] && plusAddressHost.length > 4) {
    plusAddressHost = [plusAddressHost substringFromIndex:4];
  }
  return [[ManualFillPlusAddress alloc]
      initWithPlusAddress:plusAddress
                 siteName:siteName.length ? siteName : plusAddressHost
                     host:plusAddressHost
                      URL:_URL];
}

// Sends actions to the consumer.
- (void)postActionsToConsumer:(BOOL)hasPlusAddresses {
  if (!self.consumer) {
    return;
  }

  NSMutableArray<ManualFillActionItem*>* actions =
      [[NSMutableArray alloc] init];

  if (_plusAddressesAreFetched && hasPlusAddresses) {
    NSString* managePlusAddressesTitle = l10n_util::GetNSString(
        IDS_PLUS_ADDRESS_MANUAL_FALLBACK_MANAGE_ACTION_TEXT_IOS);
    ManualFillActionItem* managePlusAddressItem = [[ManualFillActionItem alloc]
        initWithTitle:managePlusAddressesTitle
               action:^{
                 base::RecordAction(base::UserMetricsAction(
                     "ManualFallback_PlusAddress_OpenManagePlusAddress"));
               }];
    managePlusAddressItem.accessibilityIdentifier =
        manual_fill::kManagePlusAddressAccessibilityIdentifier;
    [actions addObject:managePlusAddressItem];
  }

  __weak __typeof(self) weakSelf = self;
  // Offer plus address creation if it's supported for the current user session
  // and if the user doesn't have any plus addresses created for the current
  // domain.
  if (_isPlusAddressCreationFallbackEnabled && _plusAddressesAreFetched &&
      !hasPlusAddresses) {
    NSString* createPlusAddressesTitle = l10n_util::GetNSString(
        IDS_PLUS_ADDRESS_MANUAL_FALLBACK_CREATE_ACTION_TEXT_IOS);
    ManualFillActionItem* createPlusAddressItem = [[ManualFillActionItem alloc]
        initWithTitle:createPlusAddressesTitle
               action:^{
                 base::RecordAction(base::UserMetricsAction(
                     "ManualFallback_PlusAddress_OpenCreatePlusAddress"));
                 [weakSelf.navigator openCreatePlusAddressSheet];
               }];
    createPlusAddressItem.accessibilityIdentifier =
        manual_fill::kCreatePlusAddressAccessibilityIdentifier;
    [actions addObject:createPlusAddressItem];
  }

  if (actions.count > 0) {
    [self.consumer presentPlusAddressActions:actions];
  }
}

@end
