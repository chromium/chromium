// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive_file_picker/coordinator/drive_file_picker_mediator.h"

#import "base/apple/foundation_util.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/notreached.h"
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
#import "ios/chrome/common/ui/util/image_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/js_image_transcoder/java_script_image_transcoder.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

namespace {

// Delay after which items are fetched if the request is delayed.
constexpr base::TimeDelta kFetchItemsDelay = base::Seconds(0.5);
// Initial delay after fetching items failed for the first time. The delay
// should double for each new attempt.
constexpr base::TimeDelta kFetchItemsDelayToRetryMin = base::Seconds(0.5);
// Maximum delay to retry after fetching items failed.
constexpr base::TimeDelta kFetchItemsDelayToRetryMax = base::Seconds(10.0);
// folder_identifier parameter for the My Drive view.
NSString* kMyDriveFolderIdentifier = @"root";
// Size to which fetched images should be resized before caching.
constexpr int kFetchedImageResizeDimension = 64;

}  // namespace

@interface DriveFilePickerMediator ()
@end

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
  // The set of images being fetched, soon to be added to `_imageCache`.
  __weak NSMutableSet<NSString*>* _imagesPending;
  // Cache of fetched images for the Drive file picker.
  __weak NSCache<NSString*, UIImage*>* _imageCache;
  // JavaScript image transcoder to locally re-encode icons, thumbnails, etc.
  std::unique_ptr<web::JavaScriptImageTranscoder> _imageTranscoder;
  // File URL to which the selected file is being downloaded.
  NSURL* _selectedFileDestinationURL;
  // The selected drive item. This comes from `_fetchedDriveItems` but is not
  // necessarily contained in `_fetchedDriveItems` at all times.
  std::optional<DriveItem> _selectedFile;
  // Identifier of the download for the current selected item.
  NSString* _selectedFileDownloadID;
  // If `_selectedFile` is not nullopt, then this indicates whether it was
  // selected from search items or not.
  BOOL _selectedFileIsSearchItem;
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
  // parameters are modified frequently or if queries fail several times.
  base::OneShotTimer _fetchTimer;
  // The page token to use to continue the current list/search.
  NSString* _pageToken;
}

- (instancetype)
         initWithWebState:(web::WebState*)webState
                 identity:(id<SystemIdentity>)identity
                    title:(NSString*)title
            imagesPending:(NSMutableSet<NSString*>*)imagesPending
               imageCache:(NSCache<NSString*, UIImage*>*)imageCache
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
    CHECK(driveService);
    CHECK(accountManagerService);
    CHECK(imagesPending);
    CHECK(imageCache);
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
    _driveList = _driveService->CreateList(_identity);
    _driveDownloader = _driveService->CreateFileDownloader(_identity);
    _imageTranscoder = std::make_unique<web::JavaScriptImageTranscoder>();
    _imagesPending = imagesPending;
    _imageCache = imageCache;
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
  _imageTranscoder = nullptr;
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
  [_consumer setCancelButtonVisible:_collectionType ==
                                    DriveFilePickerCollectionType::kRoot];
}

- (void)setSelectedIdentity:(id<SystemIdentity>)selectedIdentity {
  if (_identity == selectedIdentity) {
    return;
  }
  _identity = selectedIdentity;
  [_consumer setSelectedUserIdentityEmail:_identity.userEmail];
  [self setSelectedFile:std::nullopt];
  [self configureConsumerIdentitiesMenu];
  _driveList = _driveService->CreateList(_identity);
  _driveDownloader = _driveService->CreateFileDownloader(_identity);
}

#pragma mark - DriveFilePickerMutator

- (void)selectDriveItem:(NSString*)driveItemIdentifier {
  std::optional<DriveItem> driveItem =
      FindDriveItemFromIdentifier(_fetchedDriveItems, driveItemIdentifier);
  // If this is a real file, select and download it.
  if (driveItem && !driveItem->is_folder && !driveItem->is_shared_drive) {
    // Unfocusing the search bar so the confirmation button can become visible.
    _searchBarFocused = NO;
    [self.consumer setSearchBarFocused:NO searchText:_searchText];
    [self setSelectedFile:driveItem];
    return;
  }

  // If the user tries to browse into a folder or other type of collection while
  // an item is already selected, clear the selection.
  [self setSelectedFile:std::nullopt];

  if (driveItem && (driveItem->is_folder || driveItem->is_shared_drive)) {
    // If this is a real folder or shared drive, then open it.
    [self.delegate
        browseDriveCollectionWithMediator:self
                                    title:driveItem->name
                            imagesPending:_imagesPending
                               imageCache:_imageCache
                           collectionType:DriveFilePickerCollectionType::kFolder
                         folderIdentifier:driveItem->identifier
                                   filter:_filter
                      ignoreAcceptedTypes:_ignoreAcceptedTypes
                          sortingCriteria:_sortingCriteria
                         sortingDirection:_sortingDirection];
    return;
  }

  // Handle browsing to virtual collections.
  DriveFilePickerItem* myDriveItem = [DriveFilePickerItem myDriveItem];
  DriveFilePickerItem* starredItem = [DriveFilePickerItem starredItem];
  DriveFilePickerItem* recentItem = [DriveFilePickerItem recentItem];
  DriveFilePickerItem* sharedWithMeItem =
      [DriveFilePickerItem sharedWithMeItem];
  DriveFilePickerItem* sharedDrivesItem =
      [DriveFilePickerItem sharedDrivesItem];

  NSString* title;
  DriveFilePickerCollectionType collectionType;
  NSString* folderIdentifier;
  if ([driveItemIdentifier isEqual:myDriveItem.identifier]) {
    title = myDriveItem.title;
    collectionType = DriveFilePickerCollectionType::kFolder;
    folderIdentifier = kMyDriveFolderIdentifier;
  } else if ([driveItemIdentifier isEqual:starredItem.identifier]) {
    title = starredItem.title;
    collectionType = DriveFilePickerCollectionType::kStarred;
    folderIdentifier = nil;
  } else if ([driveItemIdentifier isEqual:recentItem.identifier]) {
    title = recentItem.title;
    collectionType = DriveFilePickerCollectionType::kRecent;
    folderIdentifier = nil;
  } else if ([driveItemIdentifier isEqual:sharedWithMeItem.identifier]) {
    title = sharedWithMeItem.title;
    collectionType = DriveFilePickerCollectionType::kSharedWithMe;
    folderIdentifier = nil;
  } else if ([driveItemIdentifier isEqual:sharedDrivesItem.identifier]) {
    title = sharedDrivesItem.title;
    collectionType = DriveFilePickerCollectionType::kSharedDrives;
    folderIdentifier = nil;
  } else {
    NOTREACHED();
  }

  // If the collection type is `kFolder`, then `folderIdentifier` should be set.
  CHECK(collectionType != DriveFilePickerCollectionType::kFolder ||
        folderIdentifier != nil);
  [self.delegate browseDriveCollectionWithMediator:self
                                             title:title
                                     imagesPending:_imagesPending
                                        imageCache:_imageCache
                                    collectionType:collectionType
                                  folderIdentifier:folderIdentifier
                                            filter:_filter
                               ignoreAcceptedTypes:_ignoreAcceptedTypes
                                   sortingCriteria:_sortingCriteria
                                  sortingDirection:_sortingDirection];
}

- (void)loadFirstPage {
  [self loadItemsAppending:NO delayed:NO animated:YES];
}

- (void)loadNextPage {
  CHECK(_pageToken);
  [self loadItemsAppending:YES delayed:NO animated:YES];
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
  [self loadItemsAppending:NO delayed:NO animated:YES];
}

- (void)fetchIconForDriveItem:(NSString*)itemIdentifier {
  std::optional<DriveItem> driveItem =
      FindDriveItemFromIdentifier(_fetchedDriveItems, itemIdentifier);
  CHECK(driveItem);
  NSString* imageLink = GetImageLinkForDriveItem(*driveItem);
  CHECK(imageLink);
  __weak __typeof(self) weakSelf = self;
  // If there is a cached image, use it.
  UIImage* cachedImage = [_imageCache objectForKey:imageLink];
  if (cachedImage) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(
                       [](DriveFilePickerMediator* mediator,
                          UIImage* cachedImage, NSString* imageLink) {
                         [mediator setIcon:cachedImage
                             forItemsWithImageLink:imageLink];
                       },
                       weakSelf, cachedImage, imageLink));
    // Since `setIcon:` has to be called asynchronously, if there were other
    // cells with the same image appearing in this cycle for which
    // `shouldFetchIcon` is also YES, then this task would be posted once for
    // each cell, which is duplicated work. To avoid this, set `shouldFetchIcon`
    // to NO for all these items synchronously.
    [self setShouldFetchIcon:NO forItemsWithImageLink:imageLink];
    return;
  }
  // If the image is being fetched, it should be in the cache next time.
  if ([_imagesPending containsObject:imageLink]) {
    return;
  }
  // Otherwise fetch the image.
  [_imagesPending addObject:imageLink];
  GURL imageURL = GURL(base::SysNSStringToUTF16(imageLink));
  _imageFetcher->FetchImageData(
      imageURL,
      base::BindOnce(
          [](DriveFilePickerMediator* mediator, NSString* imageLink,
             NSString* itemIdentifier, const std::string& imageData,
             const image_fetcher::RequestMetadata& metadata) {
            NSData* imageNSData = [NSData dataWithBytes:imageData.data()
                                                 length:imageData.length()];
            [mediator processUnsafeImageData:imageNSData
                             fetchedFromLink:imageLink];
          },
          weakSelf, imageLink, itemIdentifier),
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
  // If the currently selected item is not part of enabled items, unselect it.
  if (_selectedFile &&
      ![enabledItemsIdentifiers containsObject:_selectedFile->identifier]) {
    [self setSelectedFile:std::nullopt];
  }
}

- (void)setFilter:(DriveFilePickerFilter)filter {
  if (_filter == filter) {
    return;
  }
  _filter = filter;
  [self.consumer setFilter:filter];
  [self loadItemsAppending:NO delayed:NO animated:YES];
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
  [self loadItemsAppending:NO delayed:YES animated:YES];
}

- (void)hideSearchItemsOrBrowseBack {
  if (_shouldShowSearchItems) {
    [self setShouldShowSearchItems:NO];
  } else {
    [self.delegate browseToParentWithMediator:self];
  }
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
- (void)populateRootItemsAnimated:(BOOL)animated {
  // There is no next page at the root.
  _pageToken = nil;
  NSArray<DriveFilePickerItem*>* primaryItems = @[
    [DriveFilePickerItem myDriveItem], [DriveFilePickerItem sharedDrivesItem],
    [DriveFilePickerItem starredItem]
  ];
  NSArray<DriveFilePickerItem*>* secondaryItems = @[
    [DriveFilePickerItem recentItem], [DriveFilePickerItem sharedWithMeItem]
  ];
  [self.consumer populatePrimaryItems:primaryItems
                       secondaryItems:secondaryItems
                               append:NO
                     showSearchHeader:NO
                    nextPageAvailable:NO
                             animated:animated];
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
  if (_selectedFile && _selectedFileIsSearchItem && !_shouldShowSearchItems) {
    // If the selected item was a search item and search items are hidden, clear
    // the selection.
    [self setSelectedFile:std::nullopt];
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
  [self loadItemsAppending:NO delayed:NO animated:YES];
}

// Sets the selected item (can be none), cancels any previously started download
// and potentially starts a new download. Updates the consumer accordingly.
- (void)setSelectedFile:(std::optional<DriveItem>)item {
  NSString* itemIdentifier = item ? item->identifier : nil;
  NSString* selectedItemIdentifier =
      _selectedFile ? _selectedFile->identifier : nil;
  if (selectedItemIdentifier == itemIdentifier ||
      [selectedItemIdentifier isEqualToString:itemIdentifier]) {
    // If the item is already selected, do nothing.
    return;
  }

  // Clean-up any already existing download.
  if (_selectedFile && _selectedFileDownloadID) {
    _driveDownloader->CancelDownload(_selectedFileDownloadID);
    _selectedFileDownloadID = nil;
  }

  // Update selected item and status in the consumer.
  _selectedFile = item;
  _selectedFileIsSearchItem = _selectedFile ? _shouldShowSearchItems : NO;
  [self.consumer setSelectedItemIdentifier:itemIdentifier];
  [self.consumer setDownloadStatus:item ? DriveFileDownloadStatus::kInProgress
                                        : DriveFileDownloadStatus::kNotStarted];

  // If there is a new selected item, start to download it.
  if (!item) {
    return;
  }
  NSURL* fileURL = DriveFilePickerGenerateDownloadFileURL(item->name);
  CHECK(fileURL);
  __weak __typeof(self) weakSelf = self;
  _selectedFileDownloadID = _driveDownloader->DownloadFile(
      *item, fileURL,
      base::BindRepeating(^(DriveFileDownloadID driveFileDownloadID,
                            const DriveFileDownloadProgress& progress){
      }),
      base::BindOnce(
          [](DriveFilePickerMediator* mediator, NSURL* downloadFileURL,
             DriveFileDownloadID driveFileDownloadID, BOOL success,
             NSError* error) {
            [mediator handleDownloadResponse:driveFileDownloadID
                                       error:error
                                     fileURL:downloadFileURL];
          },
          weakSelf, fileURL));
}

- (void)handleDownloadResponse:(DriveFileDownloadID)driveFileDownloadID
                         error:(NSError*)error
                       fileURL:(NSURL*)fileURL {
  CHECK(_selectedFile);
  // Reset the download ID to indicate that there is no ongoing download
  // anymore.
  _selectedFileDownloadID = nil;
  if (error) {
    // If there is a download error, prepare a callback to optionally try again.
    __weak __typeof(self) weakSelf = self;
    auto retrySelectCallback = base::BindOnce(
        [](DriveFilePickerMediator* mediator, std::optional<DriveItem> file) {
          [mediator setSelectedFile:std::move(file)];
        },
        weakSelf, _selectedFile);
    // Then clear the selection.
    [self setSelectedFile:std::nullopt];
    // Then present an alert to ask the user whether to try again.
    [self.consumer
        showDownloadFailureAlertWithRetryBlock:base::CallbackToBlock(std::move(
                                                   retrySelectCallback))];
    return;
  }
  [self.consumer setDownloadStatus:DriveFileDownloadStatus::kSuccess];
  _selectedFileDestinationURL = fileURL;
}

// If root items should be loaded, then there are no items to fetch so this
// asynchronously populates the consumer. Otherwise, this fetches items.
- (void)loadItemsAppending:(BOOL)append
                   delayed:(BOOL)delayed
                  animated:(BOOL)animated {
  // If there is a timer programmed to fetch items, cancel it.
  _fetchTimer.Stop();
  // Cancel any pending fetch query.
  if (_driveList->IsExecutingQuery()) {
    _driveList->CancelCurrentQuery();
  }

  if (_collectionType != DriveFilePickerCollectionType::kRoot ||
      _shouldShowSearchItems) {
    const base::TimeDelta delay =
        delayed ? kFetchItemsDelay : base::TimeDelta();
    [self fetchItemsAppending:append
                        delay:delay
                 delayToRetry:kFetchItemsDelayToRetryMin
                     animated:animated];
    return;
  }

  // If root items are to be presented, then `append` should be NO.
  CHECK(!append);
  // Populating root items asynchronously as `loadItems...:` is expected to work
  // asynchronously in general.
  __weak __typeof(self) weakSelf = self;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          ^(__strong __typeof(weakSelf) strongSelf, BOOL populateAnimated) {
            [strongSelf populateRootItemsAnimated:populateAnimated];
          },
          weakSelf, animated));
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
                      delay:(base::TimeDelta)delay
               delayToRetry:(base::TimeDelta)delayToRetry
                   animated:(BOOL)animated {
  // If the fetching needs to be delayed, post it for later and return early.
  if (delay != base::TimeDelta()) {
    __weak __typeof(self) weakSelf = self;
    _fetchTimer.Start(FROM_HERE, delay, base::BindOnce(^{
                        [weakSelf fetchItemsAppending:append
                                                delay:base::TimeDelta()
                                         delayToRetry:delayToRetry
                                             animated:animated];
                      }));
    return;
  }

  if (!append) {
    // If this is a new query, then `_pageToken` can be reset.
    _pageToken = nil;
  }

  DriveListQuery query = CreateDriveListQuery(
      _collectionType, _folderIdentifier, _filter, _sortingCriteria,
      _sortingDirection, _shouldShowSearchItems, _searchText, _pageToken);

  __weak __typeof(self) weakSelf = self;
  auto completion = base::BindOnce(
      [](DriveFilePickerMediator* mediator, const base::TimeDelta& delayToRetry,
         BOOL animated, const DriveListResult& result) {
        [mediator handleListItemsResponse:result
                             delayToRetry:delayToRetry
                                 animated:animated];
      },
      weakSelf, delayToRetry, animated);
  if (_shouldShowSearchItems ||
      _collectionType != DriveFilePickerCollectionType::kSharedDrives) {
    _driveList->ListFiles(query, std::move(completion));
  } else {
    _driveList->ListSharedDrives(query, std::move(completion));
  }
}

// Called as a completion of `_driveList->ListItems(...)`. Either replaces
// exisiting items with new ones (`append` is NO) or appends new items to
// existing ones (`append` is YES). If `animated` is YES then new items are
// animated into the consumer.
- (void)handleListItemsResponse:(const DriveListResult&)result
                   delayToRetry:(base::TimeDelta)delayToRetry
                       animated:(BOOL)animated {
  const BOOL append = _pageToken != nil;
  if (result.error) {
    // If there is an error, try again with twice the delay at the next attempt.
    [self fetchItemsAppending:append
                        delay:delayToRetry
                 delayToRetry:std::clamp(2 * delayToRetry,
                                         kFetchItemsDelayToRetryMin,
                                         kFetchItemsDelayToRetryMax)
                     animated:animated];
    return;
  }

  // Remember old item identifiers so they can be reconfigured if they also show
  // up in `result.items`.
  NSMutableSet<NSString*>* previousIdentifiers = [NSMutableSet set];
  for (const DriveItem& item : _fetchedDriveItems) {
    [previousIdentifiers addObject:item.identifier];
  }

  if (append) {
    // If `append`, then this is the next page to insert at the end.
    _fetchedDriveItems.reserve(_fetchedDriveItems.size() + result.items.size());
    // Only insert items which were not already fetched.
    for (const DriveItem& item : result.items) {
      if (![previousIdentifiers containsObject:item.identifier]) {
        _fetchedDriveItems.push_back(item);
      }
    }
  } else {
    // Otherwise this is a first page so existing items are replaced.
    _fetchedDriveItems = result.items;
  }
  _pageToken = result.next_page_token;

  NSMutableArray<DriveFilePickerItem*>* res = [[NSMutableArray alloc] init];
  NSMutableArray<NSString*>* itemsToReconfigure = [NSMutableArray array];
  for (const DriveItem& item : result.items) {
    if ([previousIdentifiers containsObject:item.identifier]) {
      if (append) {
        // If appending, skipping items which are already in previous pages.
        continue;
      } else {
        // If replacing items, then the cell associated with an item which was
        // already there simply needs to be reconfigured.
        [itemsToReconfigure addObject:item.identifier];
      }
    }
    NSString* imageLink = GetImageLinkForDriveItem(item);
    UIImage* fetchedIcon = [_imageCache objectForKey:imageLink];
    DriveFilePickerItem* filePickerItem = DriveItemToDriveFilePickerItem(
        item, _collectionType, _sortingCriteria, _shouldShowSearchItems,
        _searchText, fetchedIcon, imageLink);
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

- (void)configureConsumerIdentitiesMenu {
  ActionFactory* actionFactory = [[ActionFactory alloc]
      initWithScenario:kMenuScenarioHistogramSelectDriveIdentityEntry];

  __weak __typeof(self) weakSelf = self;
  auto actionResult = ^(id<SystemIdentity> identity) {
    [weakSelf.driveFilePickerHandler
        setDriveFilePickerSelectedIdentity:identity];
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

// Transcodes `unsafeImageData` into `safeImageData` and forwards
// `safeImageData` to `processSafeImageData:`.
- (void)processUnsafeImageData:(NSData*)unsafeImageData
               fetchedFromLink:(NSString*)imageLink {
  __weak __typeof(self) weakSelf = self;
  _imageTranscoder->TranscodeImage(
      unsafeImageData, @"image/png", @(kFetchedImageResizeDimension),
      @(kFetchedImageResizeDimension), nil,
      base::BindOnce(
          [](DriveFilePickerMediator* mediator,
             NSMutableSet<NSString*>* imagesPending, NSString* imageLink,
             NSData* safeImageData, NSError* error) {
            if (!safeImageData) {
              // If there is no data, then remove `imageLink` from
              // `imagesPending` and try again next time the item appears on the
              // screen.
              [imagesPending removeObject:imageLink];
              return;
            }
            [mediator processSafeImageData:safeImageData
                           fetchedFromLink:imageLink];
          },
          weakSelf, _imagesPending, imageLink));
}

// Decodes and caches `imageData` using `imageLink` as key and updates the
// consumer items associated with image link `imageLink`.
- (void)processSafeImageData:(NSData*)imageData
             fetchedFromLink:(NSString*)imageLink {
  UIImage* image = [UIImage imageWithData:imageData];
  [_imageCache setObject:image forKey:imageLink];
  [_imagesPending removeObject:imageLink];
  [self setIcon:image forItemsWithImageLink:imageLink];
}

// Set `shouldFetchIcon` for all consumer items associated with image link
// `imageLink`.
- (void)setShouldFetchIcon:(BOOL)shouldFetchIcon
     forItemsWithImageLink:(NSString*)imageLink {
  // Update items with the same image link in the consumer.
  NSMutableSet<NSString*>* itemsToUpdate = [NSMutableSet set];
  for (const DriveItem& item : _fetchedDriveItems) {
    if ([GetImageLinkForDriveItem(item) isEqualToString:imageLink]) {
      [itemsToUpdate addObject:item.identifier];
    }
  }
  [self.consumer setShouldFetchIcon:shouldFetchIcon forItems:itemsToUpdate];
}

// Sets `image` as icon for all consumer items associated with image link
// `imageLink`.
- (void)setIcon:(UIImage*)image forItemsWithImageLink:(NSString*)imageLink {
  // Update items with the same image link in the consumer.
  NSMutableSet<NSString*>* itemsToUpdate = [NSMutableSet set];
  for (const DriveItem& item : _fetchedDriveItems) {
    if ([GetImageLinkForDriveItem(item) isEqualToString:imageLink]) {
      [itemsToUpdate addObject:item.identifier];
    }
  }
  [self.consumer setFetchedIcon:image forItems:itemsToUpdate];
}

@end
