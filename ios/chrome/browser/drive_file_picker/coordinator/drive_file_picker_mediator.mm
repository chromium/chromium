// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive_file_picker/coordinator/drive_file_picker_mediator.h"

#import "base/apple/foundation_util.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/timer/timer.h"
#import "components/image_fetcher/core/image_data_fetcher.h"
#import "ios/chrome/browser/drive/model/drive_file_downloader.h"
#import "ios/chrome/browser/drive/model/drive_list.h"
#import "ios/chrome/browser/drive/model/drive_service.h"
#import "ios/chrome/browser/drive_file_picker/coordinator/drive_file_picker_mediator_delegate.h"
#import "ios/chrome/browser/drive_file_picker/coordinator/drive_file_picker_mediator_helper.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_constants.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_consumer.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_item.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/drive_file_picker_commands.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/ui/menu/browser_action_factory.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_tab_helper.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

namespace {

// Delay after which items are fetched if the request is delayed.
constexpr base::TimeDelta kFetchItemsDelay = base::Seconds(0.5);
// folder_identifier parameter for the My Drive view.
NSString* kMyDriveFolderIdentifier = @"root";

}  // namespace

@implementation DriveFilePickerMediator {
  base::WeakPtr<web::WebState> _webState;
  id<SystemIdentity> _identity;
  // The folder associated to the current `BrowseDriveFilePickerCoordinator`.
  raw_ptr<drive::DriveService> _driveService;
  std::unique_ptr<DriveList> _driveList;
  std::unique_ptr<DriveFileDownloader> _driveDownloader;
  NSString* _title;
  // Type of collection presented in the consumer (outside search).
  DriveFilePickerCollectionType _collectionType;
  // If `_collectionType` is `kFolder`, identifier of that folder.
  NSString* _folderIdentifier;
  std::vector<DriveItem> _fetchedDriveItems;
  raw_ptr<ChromeAccountManagerService> _accountManagerService;
  // The service responsible for fetching a `DriveFilePickerItem`'s image data.
  std::unique_ptr<image_fetcher::ImageDataFetcher> _imageFetcher;
  // File URL to which the selected file is being downloaded.
  NSURL* _selectedFileDestinationURL;
  // The selected drive item identifier.
  NSString* _selectedIdentifier;
  // Identifier of the download for the current selected item.
  NSString* _selectedIdentifierDownloadID;
  // If `_selectedIdentifier` is not nil, then this indicates whether it was
  // selected from search items or not.
  BOOL _selectedIdentifierIsSearchItem;
  // If this is true, all downloadable files can be selected regardless of type.
  BOOL _ignoreAcceptedTypes;
  // Filter used to only show items matching a certain type.
  DriveFilePickerFilter _filter;
  // Types accepted by the WebState.
  NSArray<UTType*>* _acceptedTypes;
  // Sorting criteria.
  DriveItemsSortingType _sortingCriteria;
  // Sorting direction.
  DriveItemsSortingOrder _sortingDirection;
  // Whether the search bar is currently focused.
  BOOL _searchBarFocused;
  // Search text.
  NSString* _searchText;
  // If this is `YES`, then items fetched subsequently will be search items.
  BOOL _shouldShowSearchItems;
  // Timer to delay fetching to avoid fetching too frequently if the query
  // parameters are modified frequently.
  base::OneShotTimer _fetchTimer;
  // The page token to use to continue the current list/search.
  NSString* _pageToken;
}

- (instancetype)
         initWithWebState:(web::WebState*)webState
                 identity:(id<SystemIdentity>)identity
                    title:(NSString*)title
           collectionType:(DriveFilePickerCollectionType)collectionType
         folderIdentifier:(NSString*)folderIdentifier
                   filter:(DriveFilePickerFilter)filter
      ignoreAcceptedTypes:(BOOL)ignoreAcceptedTypes
          sortingCriteria:(DriveItemsSortingType)sortingCriteria
         sortingDirection:(DriveItemsSortingOrder)sortingDirection
             driveService:(drive::DriveService*)driveService
    accountManagerService:(ChromeAccountManagerService*)accountManagerService
             imageFetcher:(std::unique_ptr<image_fetcher::ImageDataFetcher>)
                              imageFetcher {
  self = [super init];
  if (self) {
    CHECK(webState);
    CHECK(identity);
    CHECK(accountManagerService);
    _webState = webState->GetWeakPtr();
    _identity = identity;
    _driveService = driveService;
    _accountManagerService = accountManagerService;
    _title = [title copy];
    _collectionType = collectionType;
    _folderIdentifier = [folderIdentifier copy];
    _filter = filter;
    _ignoreAcceptedTypes = ignoreAcceptedTypes;
    _sortingCriteria = sortingCriteria;
    _sortingDirection = sortingDirection;
    _fetchedDriveItems = {};
    _imageFetcher = std::move(imageFetcher);
    // Initialize the list of accepted types.
    ChooseFileTabHelper* tab_helper =
        ChooseFileTabHelper::GetOrCreateForWebState(webState);
    CHECK(tab_helper->IsChoosingFiles());
    _acceptedTypes = UTTypesAcceptedForEvent(tab_helper->GetChooseFileEvent());
  }
  return self;
}

- (void)disconnect {
  if (_collectionType == DriveFilePickerCollectionType::kRoot && _webState &&
      !_webState->IsBeingDestroyed()) {
    ChooseFileTabHelper* tab_helper =
        ChooseFileTabHelper::GetOrCreateForWebState(_webState.get());
    if (tab_helper->IsChoosingFiles()) {
      tab_helper->StopChoosingFiles();
    }
  }
  _webState = nullptr;
  _driveService = nullptr;
  _driveList = nullptr;
  _driveDownloader = nullptr;
  _accountManagerService = nullptr;
  _imageFetcher = nullptr;
}

- (void)setConsumer:(id<DriveFilePickerConsumer>)consumer {
  _consumer = consumer;
  [_consumer setSelectedUserIdentityEmail:_identity.userEmail];
  [self configureConsumerIdentitiesMenu];
  [self updateTitle];
  [_consumer setFilter:_filter];
  [_consumer setAllFilesEnabled:_ignoreAcceptedTypes];
  [_consumer setSortingCriteria:_sortingCriteria direction:_sortingDirection];
  [_consumer setBackground:DriveFilePickerBackground::kLoadingIndicator];
  if (_collectionType == DriveFilePickerCollectionType::kRoot) {
    [_consumer setCancelButtonVisible:YES];
  }
}

- (void)updateSelectedIdentity:(id<SystemIdentity>)selectedIdentity {
  if (_identity == selectedIdentity) {
    return;
  }
  _identity = selectedIdentity;
  [_consumer setSelectedUserIdentityEmail:_identity.userEmail];
  [self clearSelection];
  [self configureConsumerIdentitiesMenu];
}

#pragma mark - DriveFilePickerMutator

- (void)selectDriveItem:(NSString*)driveItemIdentifier {
  std::optional<DriveItem> driveItem =
      FindDriveItemFromIdentifier(_fetchedDriveItems, driveItemIdentifier);
  // If this is a real file, select and download it.
  if (driveItem && !driveItem->is_folder) {
    // Unfocusing the search bar so the confirmation button can become visible.
    _searchBarFocused = NO;
    [self.consumer setSearchBarFocused:NO searchText:_searchText];
    if ([_selectedIdentifier isEqual:driveItemIdentifier]) {
      // If the file is already selected, there is nothing else to do.
      return;
    }
    _selectedIdentifier = driveItemIdentifier;
    _selectedIdentifierIsSearchItem = _shouldShowSearchItems;
    [self.consumer setSelectedItemIdentifier:driveItemIdentifier];
    [self downloadDriveItem:*driveItem];
    return;
  }

  // If the user tries to browse into a folder or collection while an item is
  // already selected, clear the selection.
  if (_selectedIdentifier != nil) {
    [self clearSelection];
  }

  if (driveItem && driveItem->is_folder) {
    // If this is a real folder, then browse this folder.
    [self.delegate
        browseDriveCollectionWithMediator:self
                                    title:driveItem->name
                           collectionType:DriveFilePickerCollectionType::kFolder
                         folderIdentifier:driveItem->identifier
                                   filter:_filter
                      ignoreAcceptedTypes:_ignoreAcceptedTypes
                          sortingCriteria:_sortingCriteria
                         sortingDirection:_sortingDirection];
    return;
  } else if ([driveItemIdentifier
                 isEqual:[DriveFilePickerItem myDriveItem].identifier]) {
    DriveFilePickerItem* myDriveItem = [DriveFilePickerItem myDriveItem];
    [self.delegate
        browseDriveCollectionWithMediator:self
                                    title:myDriveItem.title
                           collectionType:DriveFilePickerCollectionType::kFolder
                         folderIdentifier:kMyDriveFolderIdentifier
                                   filter:_filter
                      ignoreAcceptedTypes:_ignoreAcceptedTypes
                          sortingCriteria:_sortingCriteria
                         sortingDirection:_sortingDirection];
  } else if ([driveItemIdentifier
                 isEqual:[DriveFilePickerItem starredItem].identifier]) {
    DriveFilePickerItem* starredItem = [DriveFilePickerItem starredItem];
    [self.delegate
        browseDriveCollectionWithMediator:self
                                    title:starredItem.title
                           collectionType:DriveFilePickerCollectionType::
                                              kStarred
                         folderIdentifier:nil
                                   filter:_filter
                      ignoreAcceptedTypes:_ignoreAcceptedTypes
                          sortingCriteria:_sortingCriteria
                         sortingDirection:_sortingDirection];
  } else if ([driveItemIdentifier
                 isEqual:[DriveFilePickerItem recentItem].identifier]) {
    DriveFilePickerItem* recentItem = [DriveFilePickerItem recentItem];
    [self.delegate
        browseDriveCollectionWithMediator:self
                                    title:recentItem.title
                           collectionType:DriveFilePickerCollectionType::kRecent
                         folderIdentifier:nil
                                   filter:_filter
                      ignoreAcceptedTypes:_ignoreAcceptedTypes
                          sortingCriteria:_sortingCriteria
                         sortingDirection:_sortingDirection];
  } else if ([driveItemIdentifier
                 isEqual:[DriveFilePickerItem sharedWithMeItem].identifier]) {
    DriveFilePickerItem* sharedWithMeItem =
        [DriveFilePickerItem sharedWithMeItem];
    [self.delegate
        browseDriveCollectionWithMediator:self
                                    title:sharedWithMeItem.title
                           collectionType:DriveFilePickerCollectionType::
                                              kSharedWithMe
                         folderIdentifier:nil
                                   filter:_filter
                      ignoreAcceptedTypes:_ignoreAcceptedTypes
                          sortingCriteria:_sortingCriteria
                         sortingDirection:_sortingDirection];
  } else {
    // TODO(crbug.com/344813593): Add support for Shared Drives.
  }
}

- (void)fetchFirstPage {
  if (_collectionType == DriveFilePickerCollectionType::kRoot) {
    [self populateRootItems];
  } else {
    [self fetchItemsAppending:NO delayed:NO animated:YES];
  }
}

- (void)fetchNextPage {
  CHECK(_pageToken);
  [self fetchItemsAppending:YES delayed:NO animated:YES];
}

- (void)setSortingCriteria:(DriveItemsSortingType)criteria
                 direction:(DriveItemsSortingOrder)direction {
  if (_sortingCriteria == criteria && _sortingDirection == direction) {
    // If no sorting parameter changed, do nothing.
    return;
  }
  _sortingCriteria = criteria;
  _sortingDirection = direction;
  [self.consumer setSortingCriteria:criteria direction:direction];
  [self fetchItemsAppending:NO delayed:NO animated:YES];
}

- (void)fetchIconForDriveItem:(NSString*)itemIdentifier {
  std::optional<DriveItem> driveItem =
      FindDriveItemFromIdentifier(_fetchedDriveItems, itemIdentifier);
  CHECK(driveItem);

  // By default drive api provides a 16 resolution icons, replacing 16 by 64 in
  // the icon URLs provide better sized icons e.g.
  // the URL https://drive-thirdparty.googleusercontent.com/16/type/video/mp4
  // becomes https://drive-thirdparty.googleusercontent.com/64/type/video/mp4
  NSString* resizedIconLink =
      [driveItem->icon_link stringByReplacingOccurrencesOfString:@"16"
                                                      withString:@"64"];
  GURL iconURL = GURL(base::SysNSStringToUTF16(resizedIconLink));
  __weak __typeof(self) weakSelf = self;
  _imageFetcher->FetchImageData(
      iconURL,
      base::BindOnce(^(const std::string& imageData,
                       const image_fetcher::RequestMetadata& metadata) {
        [weakSelf updateDriveItem:itemIdentifier withImageData:imageData];
      }),
      NO_TRAFFIC_ANNOTATION_YET);
}

- (void)submitFileSelection {
  if (!_webState || _webState->IsBeingDestroyed()) {
    [self.driveFilePickerHandler hideDriveFilePicker];
    return;
  }
  ChooseFileTabHelper* tab_helper =
      ChooseFileTabHelper::GetOrCreateForWebState(_webState.get());
  if (!tab_helper->IsChoosingFiles()) {
    [self.driveFilePickerHandler hideDriveFilePicker];
    return;
  }
  CHECK(_selectedFileDestinationURL);
  tab_helper->StopChoosingFiles(@[ _selectedFileDestinationURL ], nil, nil);
  [self.delegate mediatorDidStopFileSelection:self];
}

- (void)setAcceptedTypesIgnored:(BOOL)ignoreAcceptedTypes {
  if (ignoreAcceptedTypes == _ignoreAcceptedTypes) {
    return;
  }
  _ignoreAcceptedTypes = ignoreAcceptedTypes;
  NSMutableSet<NSString*>* enabledItemsIdentifiers = [NSMutableSet set];
  for (const DriveItem& item : _fetchedDriveItems) {
    if (DriveFilePickerItemShouldBeEnabled(item, _acceptedTypes,
                                           _ignoreAcceptedTypes)) {
      [enabledItemsIdentifiers addObject:item.identifier];
    }
  }
  [self.consumer setEnabledItems:enabledItemsIdentifiers];
  [self.consumer setAllFilesEnabled:_ignoreAcceptedTypes];
}

- (void)setFilter:(DriveFilePickerFilter)filter {
  if (_filter == filter) {
    return;
  }
  _filter = filter;
  [self.consumer setFilter:filter];
  [self fetchItemsAppending:NO delayed:NO animated:YES];
}

- (void)setSearchBarFocused:(BOOL)focused {
  if (_searchBarFocused == focused) {
    return;
  }
  _searchBarFocused = focused;
  // If `_searchBarFocused` is set from the mutator interface, either because
  // the user focused the search bar by tapping on it or defocused it by tapping
  // "Cancel", then the search items should respectively be shown or hidden.
  [self setShouldShowSearchItems:focused];
}

- (void)setSearchText:(NSString*)searchText {
  if ([searchText isEqualToString:_searchText]) {
    return;
  }
  NSString* previousSearchText = _searchText;
  _searchText = searchText;
  if (!_searchBarFocused) {
    // If the search bar is not focused while the search text changes, it means
    // that the search bar was just unfocused. The search text should now be
    // empty, the consequences of unfocusing the search bar are handled already.
    return;
  }
  if (_searchText.length == 0 || previousSearchText.length == 0) {
    // When switching from zero-state to non-zero-state search or the other way
    // around, items are trashed and the loading indicator is presented.
    [self clearItems];
    [self.consumer setBackground:DriveFilePickerBackground::kLoadingIndicator];
  }
  // Fetching new items is delayed when `_searchText` is modified, to ensure
  // modifying it very frequently does not equally too frequent API calls. This
  // works because only one pending fetch request is ever allowed at a time.
  [self fetchItemsAppending:NO delayed:YES animated:YES];
}

- (void)browseBack {
  [self.delegate browseToParentWithMediator:self];
}

- (void)hideSearchItemsOrCancelFileSelection {
  if (_shouldShowSearchItems) {
    [self setShouldShowSearchItems:NO];
  } else {
    [self.delegate mediatorDidStopFileSelection:self];
  }
}

#pragma mark - Private

// Updates the title in the consumer.
- (void)updateTitle {
  if (_shouldShowSearchItems) {
    // No title in search mode.
    [self.consumer setTitle:nil];
  } else if (_collectionType == DriveFilePickerCollectionType::kRoot) {
    // When presenting the root collection, out of search mode, show root title.
    [self.consumer setRootTitle];
  } else {
    // Otherwise, out of search mode, show the provided collection title.
    [self.consumer setTitle:_title];
  }
}

// Populates the consumer with root items e.g. "My Drive", "Shared Drives", etc.
- (void)populateRootItems {
  // There is no next page at the root.
  _pageToken = nil;
  NSArray<DriveFilePickerItem*>* primaryItems = @[
    [DriveFilePickerItem myDriveItem], [DriveFilePickerItem sharedDrivesItem],
    [DriveFilePickerItem computersItem], [DriveFilePickerItem starredItem]
  ];
  NSArray<DriveFilePickerItem*>* secondaryItems = @[
    [DriveFilePickerItem recentItem], [DriveFilePickerItem sharedWithMeItem]
  ];
  [self.consumer populatePrimaryItems:primaryItems
                       secondaryItems:secondaryItems
                               append:NO
                     showSearchHeader:NO
                    nextPageAvailable:NO
                             animated:YES];
  [self.consumer setBackground:DriveFilePickerBackground::kNoBackground];
}

// Clears items in the mediator and consumer.
- (void)clearItems {
  _fetchedDriveItems = {};
  [self.consumer populatePrimaryItems:nil
                       secondaryItems:nil
                               append:NO
                     showSearchHeader:NO
                    nextPageAvailable:NO
                             animated:NO];
}

- (void)setShouldShowSearchItems:(BOOL)shouldShowSearchItems {
  if (shouldShowSearchItems == _shouldShowSearchItems) {
    return;
  }
  // When this line is reached, the mediator is switching between two modes:
  // showing search items and showing non-search items.
  _shouldShowSearchItems = shouldShowSearchItems;
  if (_selectedIdentifier != nil && _selectedIdentifierIsSearchItem &&
      !_shouldShowSearchItems) {
    // If the selected item was a search item and search items are hidden, clear
    // the selection.
    [self clearSelection];
  }
  if (!_shouldShowSearchItems) {
    // If search items are hidden, then ensure the search bar is defocused and
    // the search text is cleared.
    _searchBarFocused = NO;
    [self.consumer setSearchBarFocused:NO searchText:nil];
    _searchText = nil;
  }
  // When switching between search items and non-search items, the list of items
  // is cleared and the loading indicator is presented.
  [self clearItems];
  [self.consumer setBackground:DriveFilePickerBackground::kLoadingIndicator];
  [self updateTitle];
  if (_collectionType == DriveFilePickerCollectionType::kRoot &&
      !shouldShowSearchItems) {
    [self populateRootItems];
  } else {
    [self fetchItemsAppending:NO delayed:NO animated:YES];
  }
}

// Clears the selected identifier and updates the consumer accordingly.
- (void)clearSelection {
  [self.consumer setDownloadStatus:DriveFileDownloadStatus::kNotStarted];
  [self.consumer setSelectedItemIdentifier:nil];
  if (_driveDownloader && _selectedIdentifierDownloadID) {
    _driveDownloader->CancelDownload(_selectedIdentifierDownloadID);
  }
  _selectedIdentifierDownloadID = nil;
  _selectedIdentifier = nil;
}

- (void)downloadDriveItem:(const DriveItem&)driveItem {
  [self.consumer setDownloadStatus:DriveFileDownloadStatus::kInProgress];
  _driveDownloader = _driveService->CreateFileDownloader(_identity);
  NSURL* fileURL = DriveFilePickerGenerateDownloadFileURL(driveItem.name);
  CHECK(fileURL);
  __weak __typeof(self) weakSelf = self;
  _selectedIdentifierDownloadID = _driveDownloader->DownloadFile(
      driveItem, fileURL,
      base::BindRepeating(^(DriveFileDownloadID driveFileDownloadID,
                            const DriveFileDownloadProgress& progress){
      }),
      base::BindOnce(^(DriveFileDownloadID driveFileDownloadID, BOOL sucess,
                       NSError* error) {
        [weakSelf handleDownloadResponse:driveFileDownloadID
                                   error:error
                                 fileURL:fileURL];
      }));
}

- (void)handleDownloadResponse:(DriveFileDownloadID)driveFileDownloadID
                         error:(NSError*)error
                       fileURL:(NSURL*)fileURL {
  if (error) {
    // TODO(crbug.com/344815565): Display error message.
  }
  [self.consumer setDownloadStatus:DriveFileDownloadStatus::kSuccess];
  _selectedFileDestinationURL = fileURL;
}

// Fetches items according to the original query and the current state of the
// mediator i.e. selected filters, sorting parameters, search text, focus state
// of the search bar. If there is already a fetch query pending, a new one
// replaces it. If `append` is YES, then fetched items are inserted at the end
// of `_fetchedDriveItems`, otherwise they replace existing items. If `delayed`
// is YES then the query will be delayed and potentially canceled if a new query
// is triggered before the end of the delay. If `animated` is YES then the items
// in the response to that query will be animated into the consumer.
- (void)fetchItemsAppending:(BOOL)append
                    delayed:(BOOL)delayed
                   animated:(BOOL)animated {
  // If there is already a timer programmed to fetch items, cancel it.
  _fetchTimer.Stop();
  // If the fetching needs to be delayed, post it for later and return early.
  if (delayed) {
    __weak __typeof(self) weakSelf = self;
    _fetchTimer.Start(FROM_HERE, kFetchItemsDelay, base::BindOnce(^{
                        [weakSelf fetchItemsAppending:append
                                              delayed:NO
                                             animated:animated];
                      }));
    return;
  }

  if (!append) {
    // If this is a new query, then `_pageToken` can be reset.
    _pageToken = nil;
  }

  _driveList = _driveService->CreateList(_identity);

  DriveListQuery query = CreateDriveListQuery(
      _collectionType, _folderIdentifier, _filter, _sortingCriteria,
      _sortingDirection, _shouldShowSearchItems, _searchText, _pageToken);

  __weak __typeof(self) weakSelf = self;
  _driveList->ListFiles(query, base::BindOnce(^(const DriveListResult& result) {
                          [weakSelf handleListItemsResponse:result
                                                   animated:animated];
                        }));
}

// Called as a completion of `_driveList->ListItems(...)`. Either replaces
// exisiting items with new ones (`append` is NO) or appends new items to
// existing ones (`append` is YES). If `animated` is YES then new items are
// animated into the consumer.
- (void)handleListItemsResponse:(const DriveListResult&)result
                       animated:(BOOL)animated {
  // Remember old item identifiers so they can be reconfigured if they also show
  // up in `result.items`.
  NSMutableSet<NSString*>* previousIdentifiers = [NSMutableSet set];
  for (const DriveItem& item : _fetchedDriveItems) {
    [previousIdentifiers addObject:item.identifier];
  }

  BOOL append = _pageToken != nil;
  if (append) {
    // If `append`, then this is the next page to insert at the end.
    _fetchedDriveItems.insert(_fetchedDriveItems.end(), result.items.begin(),
                              result.items.end());
  } else {
    // Otherwise this is a first page so existing items are replaced.
    _fetchedDriveItems = result.items;
  }
  _pageToken = result.next_page_token;

  NSMutableArray<DriveFilePickerItem*>* res = [[NSMutableArray alloc] init];
  NSMutableArray<NSString*>* itemsToReconfigure = [NSMutableArray array];
  for (const DriveItem& item : result.items) {
    if ([previousIdentifiers containsObject:item.identifier]) {
      [itemsToReconfigure addObject:item.identifier];
    }
    DriveFilePickerItem* filePickerItem =
        DriveItemToDriveFilePickerItem(item, _collectionType, _sortingCriteria,
                                       _shouldShowSearchItems, _searchText);
    filePickerItem.enabled = DriveFilePickerItemShouldBeEnabled(
        item, _acceptedTypes, _ignoreAcceptedTypes);
    // If the search text is not empty, emphasize the first match of the search
    // text inside the name of the item.
    if (_searchText.length != 0) {
      filePickerItem.titleRangeToEmphasize =
          [filePickerItem.title rangeOfString:_searchText
                                      options:NSCaseInsensitiveSearch];
    }
    [res addObject:filePickerItem];
  }
  // Showing the "Recent" search header in zero-state search.
  BOOL showSearchHeader = _shouldShowSearchItems && _searchText.length == 0;
  // If the next page token is nil, then the consumer does not need to try and
  // fetch new items when the end of the list is reached.
  BOOL nextPageAvailable = _pageToken != nil;
  [self.consumer populatePrimaryItems:res
                       secondaryItems:nil
                               append:append
                     showSearchHeader:showSearchHeader
                    nextPageAvailable:nextPageAvailable
                             animated:animated];
  // If some items were already in the previous list, reconfigure these items.
  [self.consumer reconfigureItemsWithIdentifiers:itemsToReconfigure];
  // Update background of the file picker view.
  if (res.count != 0) {
    // If there are items to present, then clear the background.
    [self.consumer setBackground:DriveFilePickerBackground::kNoBackground];
  } else if (_shouldShowSearchItems ||
             _filter != DriveFilePickerFilter::kShowAllFiles) {
    // If there are no items during search or while applying a filter, then show
    // "No matching results" as explanation.
    [self.consumer setBackground:DriveFilePickerBackground::kNoMatchingResults];
  } else {
    // Otherwise, show "Empty folder" as explanation.
    [self.consumer setBackground:DriveFilePickerBackground::kEmptyFolder];
  }
}

- (void)identityUpdatedWithSelectedIdentity:
    (id<SystemIdentity>)selectedIdentity {
  if ([_identity isEqual:selectedIdentity]) {
    return;
  }
  [self.driveFilePickerHandler
      setDriveFilePickerSelectedIdentity:selectedIdentity];
}

- (void)configureConsumerIdentitiesMenu {
  ActionFactory* actionFactory = [[ActionFactory alloc]
      initWithScenario:kMenuScenarioHistogramSelectDriveIdentityEntry];

  __weak __typeof(self) weakSelf = self;
  auto actionResult = ^(id<SystemIdentity> identity) {
    [weakSelf identityUpdatedWithSelectedIdentity:identity];
  };
  // TODO(crbug.com/344812396): Add the identites block.
  UIMenuElement* identitiesMenu = [actionFactory
      menuToSelectDriveIdentityWithIdentities:_accountManagerService
                                                  ->GetAllIdentities()
                              currentIdentity:_identity
                                        block:actionResult];
  // TODO(crbug.com/344812396): Add the new account block.
  UIAction* addAccountAction =
      [actionFactory actionToAddAccountForDriveWithBlock:nil];
  [self.consumer setEmailsMenu:[UIMenu menuWithChildren:@[
                   addAccountAction, identitiesMenu
                 ]]];
}

- (void)updateDriveItem:(NSString*)itemIdentifier
          withImageData:(const std::string&)imageData {
  std::optional<DriveItem> driveItem =
      FindDriveItemFromIdentifier(_fetchedDriveItems, itemIdentifier);
  if (!driveItem) {
    return;
  }
  UIImage* driveIcon =
      [UIImage imageWithData:[NSData dataWithBytes:imageData.data()
                                            length:imageData.size()]
                       scale:[UIScreen mainScreen].scale];
  // An early return if no drive icon is available, avoiding an infinite look in
  // the consumer.
  if (!driveIcon) {
    return;
  }

  [self.consumer setIcon:driveIcon forItem:itemIdentifier];
}

@end
