// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_plus_address_mediator.h"

#import "base/i18n/message_formatter.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/user_metrics.h"
#import "base/ranges/algorithm.h"
#import "base/strings/sys_string_conversions.h"
#import "components/plus_addresses/grit/plus_addresses_strings.h"
#import "components/plus_addresses/plus_address_service.h"
#import "components/plus_addresses/plus_address_ui_utils.h"
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
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/menu/browser_action_factory.h"
#import "ios/chrome/grit/ios_strings.h"
#import "net/base/registry_controlled_domains/registry_controlled_domain.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/gfx/favicon_size.h"
#import "url/gurl.h"

@interface ManualFillPlusAddressMediator () <ManualFillContentInjector>
@end

@implementation ManualFillPlusAddressMediator {
  // The favicon loader used in the cell.
  raw_ptr<FaviconLoader> _faviconLoader;

  // Used to fetch plus addresses.
  raw_ptr<plus_addresses::PlusAddressService> _plusAddressService;

  // The origin to which all operations should be scoped.
  url::Origin _mainFrameOrigin;

  // If `YES`, create plus address action is shown.
  BOOL _isPlusAddressCreationFallbackEnabled;

  // A cache of all the plus addresses that are shown in the select plus
  // address. Used for filtering out addresses based on the search string.
  NSArray<ManualFillPlusAddress*>* _allPlusAddresses;
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
    _mainFrameOrigin = url::Origin::Create(URL);
    _isPlusAddressCreationFallbackEnabled =
        _plusAddressService->IsPlusAddressCreationEnabled(_mainFrameOrigin,
                                                          isOffTheRecord);
  }

  return self;
}

- (void)setConsumer:(id<ManualFillPlusAddressConsumer>)consumer {
  if (consumer == _consumer) {
    return;
  }

  _consumer = consumer;

  if (_allPlusAddresses) {
    [_consumer presentPlusAddresses:
                   [self createManualFillPlusAddressItems:_allPlusAddresses]];
  } else {
    [self postPlusAddressesToConsumer];
  }
}

- (void)fetchAllPlusAddresses {
  base::span<const plus_addresses::PlusProfile> plusProfiles =
      _plusAddressService->GetPlusProfiles();
  _allPlusAddresses =
      [self createManualFillPlusAddressesFromPlusProfiles:plusProfiles];
}

#pragma mark - TableViewFaviconDataSource

- (void)faviconForPageURL:(CrURL*)URL
               completion:(void (^)(FaviconAttributes*))completion {
  CHECK(completion);
  _faviconLoader->FaviconForPageUrlOrHost(URL.gurl, gfx::kFaviconSize,
                                          completion);
}

#pragma mark - UISearchResultsUpdating

- (void)updateSearchResultsForSearchController:
    (UISearchController*)searchController {
  CHECK(_allPlusAddresses);
  NSString* searchText = searchController.searchBar.text;
  if (!searchText.length) {
    [self.consumer presentPlusAddresses:[self createManualFillPlusAddressItems:
                                                  _allPlusAddresses]];
    return;
  }

  NSPredicate* predicate =
      [NSPredicate predicateWithFormat:
                       @"host CONTAINS[cd] %@ OR plusAddress CONTAINS[cd] %@",
                       searchText, searchText];
  NSArray* filteredPlusAddresses =
      [_allPlusAddresses filteredArrayUsingPredicate:predicate];
  [self.consumer presentPlusAddresses:[self createManualFillPlusAddressItems:
                                                filteredPlusAddresses]];
}

#pragma mark - ManualFillContentInjector

- (BOOL)canUserInjectInPasswordField:(BOOL)passwordField
                       requiresHTTPS:(BOOL)requiresHTTPS {
  NOTREACHED_NORETURN();
}

- (void)userDidPickContent:(NSString*)content
             passwordField:(BOOL)passwordField
             requiresHTTPS:(BOOL)requiresHTTPS {
  // If the "Select Plus Address" view is presented, close the sheet and then
  // initiate the filling.
  if (self.delegate) {
    [self.delegate manualFillPlusAddressMediatorWillInjectContent];
  }
  [self.contentInjector userDidPickContent:content
                             passwordField:passwordField
                             requiresHTTPS:requiresHTTPS];
}

- (void)autofillFormWithCredential:(ManualFillCredential*)credential
                      shouldReauth:(BOOL)shouldReauth {
  NOTREACHED_NORETURN();
}

- (void)autofillFormWithSuggestion:(FormSuggestion*)formSuggestion
                           atIndex:(NSInteger)index {
  NOTREACHED_NORETURN();
}

- (BOOL)isActiveFormAPasswordForm {
  NOTREACHED_NORETURN();
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
  NSArray<ManualFillPlusAddress*>* plusAddresses =
      [self createManualFillPlusAddressesFromPlusProfiles:plusProfiles];
  [self.consumer
      presentPlusAddresses:[self
                               createManualFillPlusAddressItems:plusAddresses]];
  [self postActionsToConsumer:(plusAddresses.count > 0)];
}

// Creates and returns an array of `ManualFillPlusAddressItem` from
// `ManualFillPlusAddress`.
- (NSArray<ManualFillPlusAddressItem*>*)createManualFillPlusAddressItems:
    (NSArray<ManualFillPlusAddress*>*)plusAddresses {
  int plusAddressesCount = [plusAddresses count];
  NSMutableArray* items =
      [[NSMutableArray alloc] initWithCapacity:plusAddressesCount];

  NSArray<UIAction*>* menuActions = IsKeyboardAccessoryUpgradeEnabled()
                                        ? @[ [self createManageMenuAction] ]
                                        : @[];

  for (int i = 0; i < plusAddressesCount; i++) {
    NSString* cellIndexAccessibilityLabel = base::SysUTF16ToNSString(
        base::i18n::MessageFormatter::FormatWithNamedArgs(
            l10n_util::GetStringUTF16(
                IDS_PLUS_ADDRESS_MANUAL_FALLBACK_CELL_INDEX_IOS),
            "count", plusAddressesCount, "position", i + 1));
    ManualFillPlusAddressItem* item = [[ManualFillPlusAddressItem alloc]
                initWithPlusAddress:plusAddresses[i]
                    contentInjector:self
                        menuActions:menuActions
        cellIndexAccessibilityLabel:cellIndexAccessibilityLabel];
    [items addObject:item];
  }

  return items;
}

// Creates and returns an array of `ManualFillPlusAddress` from `plusProfiles`.
- (NSArray<ManualFillPlusAddress*>*)
    createManualFillPlusAddressesFromPlusProfiles:
        (base::span<const plus_addresses::PlusProfile>)plusProfiles {
  int plusAddressesCount = (int)plusProfiles.size();
  NSMutableArray* items =
      [[NSMutableArray alloc] initWithCapacity:plusAddressesCount];

  for (int i = 0; i < plusAddressesCount; i++) {
    ManualFillPlusAddress* manualFillPlusAddress =
        [self createManualFillPlusAddressFromPlusProfile:plusProfiles[i]];
    [items addObject:manualFillPlusAddress];
  }

  return items;
}

// Creates `ManualFillPlusAddress` from `plusAddress`.
- (ManualFillPlusAddress*)createManualFillPlusAddressFromPlusProfile:
    (const plus_addresses::PlusProfile&)plusProfile {
  GURL URL(plusProfile.facet.canonical_spec());

  std::string host = URL.host();
  std::string siteName = net::registry_controlled_domains::GetDomainAndRegistry(
      host, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

  NSString* plusAddressSiteName =
      base::SysUTF8ToNSString(siteName.size() > 0 ? siteName : host);
  NSString* plusAddressHost = l10n_util::GetNSStringF(
      IDS_PLUS_ADDRESS_MANUAL_FALLBACK_SUGGESTION_SUBLABEL_PREFIX_TEXT_IOS,
      GetOriginForDisplay(plusProfile));
  return [[ManualFillPlusAddress alloc]
      initWithPlusAddress:base::SysUTF8ToNSString(*plusProfile.plus_address)
                 siteName:plusAddressSiteName
                     host:plusAddressHost
                      URL:URL];
}

// Sends actions to the consumer.
- (void)postActionsToConsumer:(BOOL)hasPlusAddresses {
  if (!self.consumer) {
    return;
  }

  NSMutableArray<ManualFillActionItem*>* actions =
      [[NSMutableArray alloc] init];

  __weak __typeof(self) weakSelf = self;

  // Offer manage action if there are any plus addresses for the domain.
  if (hasPlusAddresses) {
    NSString* managePlusAddressesTitle = l10n_util::GetNSString(
        IDS_PLUS_ADDRESS_MANUAL_FALLBACK_MANAGE_ACTION_TEXT_IOS);
    ManualFillActionItem* managePlusAddressItem = [[ManualFillActionItem alloc]
        initWithTitle:managePlusAddressesTitle
               action:^{
                 base::RecordAction(base::UserMetricsAction(
                     "ManualFallback_PlusAddress_OpenManagePlusAddress"));
                 [weakSelf.navigator openManagePlusAddress];
               }];
    managePlusAddressItem.accessibilityIdentifier =
        manual_fill::kManagePlusAddressAccessibilityIdentifier;
    [actions addObject:managePlusAddressItem];
  }

  // Offer plus address creation if it's supported for the current user session
  // and if the user doesn't have any plus addresses created for the current
  // domain.
  if (_isPlusAddressCreationFallbackEnabled && !hasPlusAddresses) {
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

  // Offer the user to select the plus address manually if plus address filling
  // is supported for the last committed origin and the user has at least 1 plus
  // address.
  if (_plusAddressService->IsPlusAddressFillingEnabled(_mainFrameOrigin) &&
      !_plusAddressService->GetPlusProfiles().empty()) {
    NSString* selectPlusAddressesTitle = l10n_util::GetNSString(
        IDS_PLUS_ADDRESS_MANUAL_FALLBACK_SELECT_ACTION_TEXT_IOS);
    ManualFillActionItem* selectPlusAddressItem = [[ManualFillActionItem alloc]
        initWithTitle:selectPlusAddressesTitle
               action:^{
                 base::RecordAction(base::UserMetricsAction(
                     "ManualFallback_PlusAddress_OpenSelectPlusAddress"));
                 [weakSelf.navigator openAllPlusAddressList];
               }];
    selectPlusAddressItem.accessibilityIdentifier =
        manual_fill::kSelectPlusAddressAccessibilityIdentifier;
    [actions addObject:selectPlusAddressItem];
  }

  if (actions.count > 0) {
    [self.consumer presentPlusAddressActions:actions];
  }
}

// Creates a "Manage" UIAction to be used with a UIMenu. Tapping on it, would
// open the manage view for the plus address.
- (UIAction*)createManageMenuAction {
  ActionFactory* actionFactory = [[ActionFactory alloc]
      initWithScenario:
          kMenuScenarioHistogramAutofillManualFallbackPlusAddressEntry];

  __weak __typeof(self) weakSelf = self;
  UIAction* manageAction = [actionFactory actionToManageLinkInNewTabWithBlock:^{
    [weakSelf.navigator openManagePlusAddress];
  }];

  return manageAction;
}

@end
