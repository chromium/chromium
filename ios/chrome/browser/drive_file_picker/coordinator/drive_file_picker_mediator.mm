// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive_file_picker/coordinator/drive_file_picker_mediator.h"

#import <queue>
#import <unordered_set>

#import "base/apple/foundation_util.h"
#import "base/cancelable_callback.h"
#import "base/files/file_path.h"
#import "base/functional/callback_helpers.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "base/timer/timer.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_utils.h"
#import "ios/chrome/browser/drive/model/drive_file_downloader.h"
#import "ios/chrome/browser/drive/model/drive_list.h"
#import "ios/chrome/browser/drive/model/drive_service.h"
#import "ios/chrome/browser/drive_file_picker/coordinator/drive_file_picker_collection.h"
#import "ios/chrome/browser/drive_file_picker/coordinator/drive_file_picker_image_fetcher.h"
#import "ios/chrome/browser/drive_file_picker/coordinator/drive_file_picker_mediator_delegate.h"
#import "ios/chrome/browser/drive_file_picker/coordinator/drive_file_picker_mediator_helper.h"
#import "ios/chrome/browser/drive_file_picker/coordinator/drive_file_picker_metrics_helper.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_constants.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_consumer.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_item.h"
#import "ios/chrome/browser/menu/ui_bundled/browser_action_factory.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/drive_file_picker_commands.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
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
// Delay before current items are cleared if fetching new items takes too long.
constexpr base::TimeDelta kClearItemsDelay = base::Seconds(2.0);

}  // namespace

@implementation DriveFilePickerMediator {
  base::WeakPtr<web::WebState> _webState;
  std::unique_ptr<DriveList> _driveList;
  std::unique_ptr<DriveFileDownloader> _driveDownloader;
  std::unique_ptr<DriveFilePickerCollection> _collection;
  std::vector<DriveItem> _fetchedDriveItems;
  // The selected files. These come from `_fetchedDriveItems` but are not
  // necessarily contained in `_fetchedDriveItems` at all times.
  std::unordered_set<DriveItem> _selectedFiles;
  // Download ID file being downloaded.
  NSString* _downloadingFileDownloadID;
  // Identifier of the file being downloaded.
  NSString* _downloadingFileIdentifier;
  // Queue of files to download. A file is added to the queue when it is added
  // to the current selection, and removed when a local copy of the file is
  // ready to be passed to the WebState.
  std::queue<DriveItem> _downloadingQueue;
  // Callback used to determine if a local copy of a file is already on disk
  // before performing any attempt to download it.
  base::CancelableOnceCallback<void(bool)> _fileVersionReadyCallback;
  // If `_selectedFiles` is not empty, then this indicates whether the files are
  // search items or not.
  BOOL _selectedFilesAreSearchItems;
  // Options used to display the current collection.
  DriveFilePickerOptions _options;
  // Types accepted by the WebState.
  NSArray<UTType*>* _acceptedTypes;
  // Whether the WebState accepts multiple files.
  BOOL _allowsMultipleSelection;
  // Whether the search bar is currently focused.
  BOOL _searchBarFocused;
  // Search text.
  NSString* _searchText;
  // If this is `YES`, then items fetched subsequently will be search items.
  BOOL _shouldShowSearchItems;
  // Timer to delay fetching to avoid fetching too frequently if the query
  // parameters are modified frequently or if queries fail several times.
  base::OneShotTimer _timerBeforeFetch;
  // Timer to clear items after fetching did not complete for too long.
  base::OneShotTimer _timerAfterFetchBeforeClearItems;
  // The page token to use to continue the current list/search.
  NSString* _nextPageToken;
  // A helper to report metrics.
  __weak DriveFilePickerMetricsHelper* _metricsHelper;
}

- (instancetype)initWithWebState:(web::WebState*)webState
                      collection:
                          (std::unique_ptr<DriveFilePickerCollection>)collection
                         options:(DriveFilePickerOptions)options {
  self = [super init];
  if (self) {
    CHECK(webState);
    CHECK(collection);
    _webState = webState->GetWeakPtr();
    _collection = std::move(collection);
    _options = options;
    _fetchedDriveItems = {};
    // Initialize the list of accepted types.
    ChooseFileTabHelper* tab_helper =
        ChooseFileTabHelper::FromWebState(webState);
    CHECK(tab_helper->IsChoosingFiles());
    const ChooseFileEvent& event = tab_helper->GetChooseFileEvent();
    _acceptedTypes = UTTypesAcceptedForEvent(event);
    _allowsMultipleSelection = event.allow_multiple_files;
  }
  return self;
}

#pragma mark - Public properties

- (void)setMetricsHelper:(DriveFilePickerMetricsHelper*)metricsHelper {
  if (_metricsHelper == metricsHelper) {
    return;
  }
  _metricsHelper = metricsHelper;
  if (_metricsHelper) {
    _metricsHelper.searchingState = DriveFilePickerSearchState::kNotSearching;
    if (_collection->IsRoot()) {
      ChooseFileTabHelper* tab_helper =
          ChooseFileTabHelper::FromWebState(_webState.get());
      CHECK(tab_helper);
      [_metricsHelper
          reportActivationMetricsForEvent:tab_helper->GetChooseFileEvent()];
    }
  }
}

- (void)setDriveService:(raw_ptr<drive::DriveService>)driveService {
  if (_driveService == driveService) {
    return;
  }
  _driveService = driveService;
  if (_driveService) {
    _driveList = _driveService->CreateList(_collection->GetIdentity());
    _driveDownloader =
        _driveService->CreateFileDownloader(_collection->GetIdentity());
  }
}

#pragma mark - Public methods

- (void)disconnect {
  if (_collection->IsRoot() && _webState && !_webState->IsBeingDestroyed()) {
    ChooseFileTabHelper* tab_helper =
        ChooseFileTabHelper::FromWebState(_webState.get());

    CHECK(tab_helper);
    if (tab_helper->IsChoosingFiles()) {
      tab_helper->StopChoosingFiles();
    }
  }
  // Clear selection on shutdown (stops download, allows dismissal, etc...)
  [self setSelectedFiles:{}];
  _timerBeforeFetch.Stop();
  _timerAfterFetchBeforeClearItems.Stop();
  _webState = nullptr;
  _driveService = nullptr;
  _driveList = nullptr;
  _driveDownloader = nullptr;
  _accountManagerService = nullptr;
  _identityManager = nullptr;
  _imageFetcher = nullptr;
}

- (void)setCollection:(std::unique_ptr<DriveFilePickerCollection>)collection {
  _collection = std::move(collection);

  [self setShouldShowSearchItems:NO];
  [self setSelectedFiles:{}];
  _searchBarFocused = NO;
  _searchText = nil;
  [_consumer setSelectedUserIdentityEmail:_collection->GetIdentity().userEmail];
  [self clearItemsAndShowLoadingIndicator];
  [self configureConsumerIdentitiesMenu];
  [self updateTitle];
  _driveList = _driveService->CreateList(_collection->GetIdentity());
  _driveDownloader =
      _driveService->CreateFileDownloader(_collection->GetIdentity());
  [_consumer setFilter:_options.filter];
  [_consumer setAllFilesEnabled:_options.ignore_accepted_types];
  [_consumer setSortingCriterion:_options.sorting_criterion
                       direction:_options.sorting_direction];
  [_consumer setCancelButtonVisible:_collection->IsRoot()];
  [_consumer setSearchBarFocused:NO searchText:nil];
  [self loadFirstPage];
}

#pragma mark - Public properties

- (void)setConsumer:(id<DriveFilePickerConsumer>)consumer {
  _consumer = consumer;
  [_consumer setSelectedUserIdentityEmail:_collection->GetIdentity().userEmail];
  [self configureConsumerIdentitiesMenu];
  [self updateTitle];
  [_consumer setFilter:_options.filter];
  [_consumer setAllFilesEnabled:_options.ignore_accepted_types];
  [_consumer setSortingCriterion:_options.sorting_criterion
                       direction:_options.sorting_direction];
  [_consumer setBackground:DriveFilePickerBackground::kLoadingIndicator];
  [_consumer setCancelButtonVisible:_collection->IsRoot()];
  [_consumer setFilterMenuEnabled:[self filterMenuShouldBeEnabled]];
  [_consumer setSortingMenuEnabled:[self sortingMenuShouldBeEnabled]];
  [_consumer setAllowsMultipleSelection:_allowsMultipleSelection];
}

- (void)setActive:(BOOL)active {
  if (_active == active) {
    return;
  }
  _active = active;
  if (_active) {
    [self.delegate mediator:self didActivateSearch:_shouldShowSearchItems];
    if (_shouldShowSearchItems) {
      if (_searchText.length) {
        _metricsHelper.searchingState = DriveFilePickerSearchState::kSearchText;
      } else {
        _metricsHelper.searchingState =
            DriveFilePickerSearchState::kSearchRecent;
      }
    } else {
      _metricsHelper.searchingState = DriveFilePickerSearchState::kNotSearching;
    }
  }
  [self applyPendingOptions];
}

- (void)setPendingOptions:(std::optional<DriveFilePickerOptions>)options {
  _pendingOptions = options;
  if (_active) {
    [self applyPendingOptions];
  }
}

#pragma mark - DriveFilePickerMutator

- (void)selectOrDeselectDriveItem:(NSString*)driveItemIdentifier {
  // `driveItem` is null if the `DriveFilePickerItem` that was selected does not
  // correspond to a `DriveItem` that was fetched i.e. it corresponds to a
  // virtual collection.
  std::optional<DriveItem> driveItem =
      FindDriveItemFromIdentifier(_fetchedDriveItems, driveItemIdentifier);

  // Types of items are handled in the following order:
  // I. Real items (items for which `driveItem` is not null)
  //    1. Files and shortcuts to files are handled first,
  //    2. Real collections i.e. shared drives, folders or shortcuts to folders.
  // II. Virtual items
  //    1. Virtual collections i.e. "My Drive", "Starred items", etc.

  // I.1. If this item cannot be browsed then it is a file or shortcut to a
  // file, so select it and download it.
  if (driveItem && !driveItem->CanBeBrowsed()) {
    [self selectOrDeselectFile:*driveItem];
    return;
  }

  // If the user tries to browse into a folder or other type of collection while
  // an item is already selected, clear the selection.
  [self setSelectedFiles:{}];

  // I.2. Handle real collections i.e. shared drives, folders or shortcuts to
  // folders.
  if (driveItem && driveItem->CanBeBrowsed()) {
    [self browseDriveCollectionForItem:*driveItem];
    return;
  }

  // II.1. Handle browsing to virtual collections.
  std::unique_ptr<DriveFilePickerCollection> collection =
      _collection->GetFirstLevelCollection(driveItemIdentifier);
  if (!collection) {
    // If no first level collection corresponds to this item, do nothing.
    return;
  }
  std::optional<DriveFilePickerFirstLevel> firstLevel =
      collection->GetFirstLevel();
  if (firstLevel) {
    _metricsHelper.firstLevelItem = *firstLevel;
  }
  [self.delegate browseDriveCollectionWithMediator:self
                                        collection:std::move(collection)
                                           options:_options];
}

- (void)loadFirstPage {
  [self loadItemsAppending:NO delayed:NO animated:NO];
}

- (void)loadNextPage {
  CHECK(_nextPageToken);
  [self loadItemsAppending:YES delayed:NO animated:YES];
}

- (void)setSortingCriterion:(DriveFilePickerSortingCriterion)criterion
                  direction:(DriveFilePickerSortingDirection)direction {
  if (_options.sorting_criterion == criterion &&
      _options.sorting_direction == direction) {
    // If no sorting parameter changed, do nothing.
    return;
  }
  _options.sorting_criterion = criterion;
  _options.sorting_direction = direction;
  [_metricsHelper reportSortingCriterionChange:criterion
                                 withDirection:direction];
  [self.delegate browseDriveCollectionWithMediator:self
                                  didUpdateOptions:_options];
  [self.consumer setSortingCriterion:criterion direction:direction];
  [self loadItemsAppending:NO delayed:NO animated:YES];
}

- (void)fetchIconForDriveItem:(NSString*)itemIdentifier {
  std::optional<DriveItem> driveItem =
      FindDriveItemFromIdentifier(_fetchedDriveItems, itemIdentifier);
  if (!driveItem || _imageFetcher->IsFetchInProgress(*driveItem)) {
    // If no item is associated with `itemIdentifier` or if the image is already
    // being fetched, do nothing.
    return;
  }
  // Otherwise fetch the image.
  __weak __typeof(self) weakSelf = self;
  _imageFetcher->FetchImage(*driveItem,
                            base::BindOnce(^(DriveItem item, UIImage* image) {
                              [weakSelf setFetchedIcon:image
                                  forItemsWithImageLink:item.GetImageLink()
                                              imageType:item.GetImageType()];
                            }));
}

- (void)submitFileSelection {
  if (!_webState || _webState->IsBeingDestroyed()) {
    [self.driveFilePickerHandler hideDriveFilePicker];
    return;
  }
  ChooseFileTabHelper* tab_helper =
      ChooseFileTabHelper::FromWebState(_webState.get());
  CHECK(tab_helper);
  if (!tab_helper->IsChoosingFiles()) {
    [self.driveFilePickerHandler hideDriveFilePicker];
    return;
  }
  NSMutableArray<NSURL*>* fileURLs = [NSMutableArray array];
  for (const DriveItem& selectedFile : _selectedFiles) {
    std::optional<base::FilePath> selectedFilePath =
        DriveFilePickerGenerateDownloadFilePath(
            _webState->GetUniqueIdentifier(), selectedFile.identifier,
            selectedFile.name);
    [fileURLs addObject:base::apple::FilePathToNSURL(*selectedFilePath)];
  }
  CHECK(fileURLs.count > 0);
  _metricsHelper.submittedFiles = fileURLs;
  NSString* displayString = GetDisplayStringForFileUrls(fileURLs);
  tab_helper->StopChoosingFiles(fileURLs, displayString, /*icon_image=*/nil);
  [self.delegate mediatorDidStopFileSelection:self];
}

- (void)setAcceptedTypesIgnored:(BOOL)ignoreAcceptedTypes {
  if (ignoreAcceptedTypes == _options.ignore_accepted_types) {
    return;
  }
  _options.ignore_accepted_types = ignoreAcceptedTypes;
  [self updateAcceptableItems];
  [self.delegate browseDriveCollectionWithMediator:self
                                  didUpdateOptions:_options];
}

- (void)setFilter:(DriveFilePickerFilter)filter {
  if (_options.filter == filter) {
    return;
  }
  _options.filter = filter;
  [_metricsHelper reportFilterChange:filter];
  [self.delegate browseDriveCollectionWithMediator:self
                                  didUpdateOptions:_options];
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
    [self clearItemsAndShowLoadingIndicator];
  }
  if (_searchText.length == 0) {
    _metricsHelper.searchingState = DriveFilePickerSearchState::kSearchRecent;
  } else {
    _metricsHelper.searchingState = DriveFilePickerSearchState::kSearchText;
  }
  [_consumer setFilterMenuEnabled:[self filterMenuShouldBeEnabled]];
  [_consumer setSortingMenuEnabled:[self sortingMenuShouldBeEnabled]];
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
    _metricsHelper.userDismissed = YES;
    [self.delegate mediatorDidStopFileSelection:self];
  }
}

#pragma mark - Private methods

// Browses to the collection corresponding to `item`.
- (void)browseDriveCollectionForItem:(const DriveItem&)item {
  if (_collection->IsRoot() && _shouldShowSearchItems) {
    _metricsHelper.firstLevelItem = DriveFilePickerFirstLevel::kSearch;
  }
  NSString* folderIdentifier = nil;
  if (item.is_shortcut) {
    folderIdentifier = item.shortcut_target_identifier;
  } else {
    folderIdentifier = item.identifier;
  }
  if (!folderIdentifier) {
    // If no appropriate folder identifier could be retrieved from `driveItem`
    // then do nothing.
    return;
  }
  // If this is a real folder or shared drive, then open it.
  std::unique_ptr<DriveFilePickerCollection> collection =
      _collection->GetFolder(item.name, folderIdentifier);
  [self.delegate browseDriveCollectionWithMediator:self
                                        collection:std::move(collection)
                                           options:_options];
}

// Updates the title in the consumer.
- (void)updateTitle {
  if (_shouldShowSearchItems) {
    // No title in search mode.
    [self.consumer setTitle:nil];
  } else if (_collection->IsRoot()) {
    // When presenting the root collection, out of search mode, show root title.
    [self.consumer setRootTitle];
  } else {
    // Otherwise, out of search mode, show the provided collection title.
    [self.consumer setTitle:_collection->GetTitle()];
  }
}

// Update what items can be selected by the user.
- (void)updateAcceptableItems {
  NSMutableSet<NSString*>* enabledItemsIdentifiers = [NSMutableSet set];
  for (const DriveItem& item : _fetchedDriveItems) {
    if (DriveFilePickerItemShouldBeEnabled(item, _acceptedTypes,
                                           _options.ignore_accepted_types)) {
      [enabledItemsIdentifiers addObject:item.identifier];
    }
  }
  [self.consumer setEnabledItems:enabledItemsIdentifiers];
  [self.consumer setAllFilesEnabled:_options.ignore_accepted_types];
  // Update selected files to exclude items which should not be enabled.
  std::unordered_set<DriveItem> enabledSelectedFiles;
  for (const DriveItem& selectedFile : _selectedFiles) {
    if (DriveFilePickerItemShouldBeEnabled(selectedFile, _acceptedTypes,
                                           _options.ignore_accepted_types)) {
      enabledSelectedFiles.insert(selectedFile);
    }
  }
  [self setSelectedFiles:enabledSelectedFiles];
}

// Populates the consumer with root items e.g. "My Drive", "Shared Drives", etc.
- (void)populateRootItemsAnimated:(BOOL)animated {
  // There is no next page at the root.
  [self setNextPageToken:nil];
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
- (void)clearItemsAndShowLoadingIndicator {
  _fetchedDriveItems = {};
  [self.consumer populatePrimaryItems:nil
                       secondaryItems:nil
                               append:NO
                     showSearchHeader:NO
                    nextPageAvailable:NO
                             animated:NO];
  [self.consumer setBackground:DriveFilePickerBackground::kLoadingIndicator];
}

// Sets whether the file picker should show "search" items or instead items
// corresponding to the current collection. This method manages the transition
// between "search" mode and "non-search", so it reloads items, updates the
// current selection and the consumer accordingly.
- (void)setShouldShowSearchItems:(BOOL)shouldShowSearchItems {
  if (shouldShowSearchItems == _shouldShowSearchItems) {
    return;
  }
  // When this line is reached, the mediator is switching between two modes:
  // showing search items and showing non-search items.
  _shouldShowSearchItems = shouldShowSearchItems;
  [self.delegate mediator:self didActivateSearch:shouldShowSearchItems];
  if (!_selectedFiles.empty() && _selectedFilesAreSearchItems &&
      !_shouldShowSearchItems) {
    // If the selected items were search items and search items are hidden,
    // clear the selection.
    [self setSelectedFiles:{}];
  }
  if (!_shouldShowSearchItems) {
    // If search items are hidden, then ensure the search bar is defocused and
    // the search text is cleared.
    _searchBarFocused = NO;
    _metricsHelper.searchSubFolderCounter -= 1;
    [self.consumer setSearchBarFocused:NO searchText:nil];
    _searchText = nil;
    _metricsHelper.searchingState = DriveFilePickerSearchState::kNotSearching;
  } else {
    _metricsHelper.triggeredSearch = YES;
    _metricsHelper.searchSubFolderCounter += 1;
    _metricsHelper.searchingState = DriveFilePickerSearchState::kSearchRecent;
  }

  if (_shouldShowSearchItems == _selectedFilesAreSearchItems) {
    NSMutableSet<NSString*>* selectedFilesIdentifiers = [NSMutableSet set];
    for (const DriveItem& selectedFile : _selectedFiles) {
      [selectedFilesIdentifiers addObject:selectedFile.identifier];
    }
    [self.consumer setSelectedItemIdentifiers:selectedFilesIdentifiers];
  } else {
    [self.consumer setSelectedItemIdentifiers:nil];
  }

  // When switching between search items and non-search items, the list of items
  // is cleared and the loading indicator is presented.
  [self clearItemsAndShowLoadingIndicator];
  [_consumer setFilterMenuEnabled:[self filterMenuShouldBeEnabled]];
  [_consumer setSortingMenuEnabled:[self sortingMenuShouldBeEnabled]];
  [self updateTitle];
  [self loadItemsAppending:NO delayed:NO animated:YES];
}

// If multifile selection is possible, then this toggles the selection of
// `file`. Otherwise, the current selection is cleared and replaced with `file`.
- (void)selectOrDeselectFile:(const DriveItem&)file {
  // Unfocusing the search bar so the confirmation button can become visible.
  _searchBarFocused = NO;
  [self.consumer setSearchBarFocused:NO searchText:_searchText];

  if (_shouldShowSearchItems && !_selectedFilesAreSearchItems) {
    // If the file picker is now in search mode but the selected files come from
    // outside search mode, reset the selection before editing it further. This
    // is unnecessary in the other direction since the selection is already
    // cleared when exiting search mode if files were selected from search mode.
    [self setSelectedFiles:{}];
  }

  const std::unordered_set<DriveItem>& oldSelectedFiles = _selectedFiles;
  bool fileWasAlreadySelected = oldSelectedFiles.contains(file);

  // If multiple file selection is allowed, the file selected is toggled.
  if (_allowsMultipleSelection) {
    if (fileWasAlreadySelected) {
      // If the file was selected, deselect it.
      [self deselectFile:file];
    } else {
      // If the file was not selected, add it to the selection.
      std::unordered_set<DriveItem> newSelectedFiles = oldSelectedFiles;
      newSelectedFiles.insert(file);
      [self setSelectedFiles:newSelectedFiles];
    }
    return;
  }

  // Otherwise if only one file can be selected...
  if (fileWasAlreadySelected) {
    // ... if it is already selected, return early.
    return;
  }

  // If the file was not selected, select it.
  [self setSelectedFiles:{file}];
}

// Removes `file` from the set of selected files.
- (void)deselectFile:(const DriveItem&)file {
  std::unordered_set<DriveItem> newSelectedFiles = _selectedFiles;
  newSelectedFiles.erase(file);
  [self setSelectedFiles:newSelectedFiles];
}

// Sets the selected files (can be empty), cancels started downloads if the
// files are not longer selected and potentially starts a new download. Updates
// the consumer accordingly.
- (void)setSelectedFiles:(std::unordered_set<DriveItem>)newSelectedFiles {
  const std::unordered_set<DriveItem>& oldSelectedFiles = _selectedFiles;

  if (oldSelectedFiles == newSelectedFiles &&
      _shouldShowSearchItems == _selectedFilesAreSearchItems) {
    // If the selection is the same, do nothing.
    return;
  }

  if (!_fileVersionReadyCallback.IsCancelled()) {
    // If there is a running task to retrieve a local copy of
    // `_downloadingQueue.front()` then cancel that task for now.
    _fileVersionReadyCallback.Cancel();
  }

  // If there is a file being downloaded which is no longer selected, cancel the
  // download.
  if (_downloadingFileDownloadID) {
    CHECK(!_downloadingQueue.empty());
    CHECK([_downloadingFileIdentifier
        isEqualToString:_downloadingQueue.front().identifier]);
    if (oldSelectedFiles.contains(_downloadingQueue.front()) &&
        !newSelectedFiles.contains(_downloadingQueue.front())) {
      _driveDownloader->CancelDownload(_downloadingFileDownloadID);
      _downloadingFileDownloadID = nil;
      _downloadingFileIdentifier = nil;
      _downloadingQueue.pop();
    }
  }

  // Add all newly selected files to the downloading queue.
  for (const DriveItem& newSelectedFile : newSelectedFiles) {
    if (!oldSelectedFiles.contains(newSelectedFile)) {
      _downloadingQueue.push(newSelectedFile);
    }
  }

  _selectedFiles = std::move(newSelectedFiles);
  _selectedFilesAreSearchItems =
      _selectedFiles.empty() ? NO : _shouldShowSearchItems;
  NSMutableSet<NSString*>* selectedFilesIdentifiers = [NSMutableSet set];
  for (const DriveItem& selectedFile : _selectedFiles) {
    [selectedFilesIdentifiers addObject:selectedFile.identifier];
  }
  [self.consumer setSelectedItemIdentifiers:selectedFilesIdentifiers];
  // Allow/forbid file picker dismissal.
  [self.delegate mediator:self didAllowDismiss:_selectedFiles.empty()];
  _metricsHelper.selectedFile = !_selectedFiles.empty();
  [self processDownloadingQueue];
}

// Dequeues files from `_downloadingQueue` by discarding them if they are not
// part of the selection, or otherwise downloading them. Updates the consumer
// with the current download status accordingly.
- (void)processDownloadingQueue {
  if (!_webState || _webState->IsBeingDestroyed()) {
    // If the WebState was or is being destroyed, do nothing.
    return;
  }

  if (_downloadingFileDownloadID || !_fileVersionReadyCallback.IsCancelled()) {
    // If there is an ongoing download, or if there is a running task to
    // retrieve a local copy of `_downloadingQueue.front()`, wait for its
    // completion and do nothing for now.
    return;
  }

  // Dequeue all unselected items.
  while (!_downloadingQueue.empty() &&
         !_selectedFiles.contains(_downloadingQueue.front())) {
    _downloadingQueue.pop();
  }

  // If the queue is empty, then update the consumer and stop.
  if (_downloadingQueue.empty()) {
    DriveFileDownloadStatus newDownloadStatus =
        _selectedFiles.empty() ? DriveFileDownloadStatus::kNotStarted
                               : DriveFileDownloadStatus::kSuccess;
    [self.consumer setDownloadStatus:newDownloadStatus];
    return;
  }

  // Get the file at the front of the queue.
  const DriveItem& fileToDownload = _downloadingQueue.front();
  std::optional<base::FilePath> filePath =
      DriveFilePickerGenerateDownloadFilePath(_webState->GetUniqueIdentifier(),
                                              fileToDownload.identifier,
                                              fileToDownload.name);
  NSURL* fileURL = base::apple::FilePathToNSURL(*filePath);
  CHECK(fileURL);

  // Check if a local copy of this version of the file is ready.
  __weak __typeof(self) weakSelf = self;
  _fileVersionReadyCallback.Reset(base::BindOnce(
      [](DriveFilePickerMediator* mediator, NSURL* fileURL,
         NSString* fileIdentifier, bool fileURLReady) {
        [mediator handleFileURL:fileURL
              readyForSelection:fileURLReady
                 fileIdentifier:fileIdentifier];
      },
      weakSelf, fileURL, fileToDownload.identifier));
  ChooseFileTabHelper* tabHelper =
      ChooseFileTabHelper::FromWebState(_webState.get());
  CHECK(tabHelper);
  tabHelper->CheckFileUrlReadyForSelection(
      fileURL, fileToDownload.modified_time,
      _fileVersionReadyCallback.callback());
}

// If `readyForSelection` is `YES`, this means there is a file at `fileURL`
// ready to be submitted to the page so this pops the download queue and
// continues to process the downloading queue.
// Otherwise, downloads the file at the front of the queue.
- (void)handleFileURL:(NSURL*)fileURL
    readyForSelection:(BOOL)readyForSelection
       fileIdentifier:(NSString*)fileIdentifier {
  if (!_webState || _webState->IsBeingDestroyed()) {
    // If the WebState was or is being destroyed, do nothing.
    return;
  }
  CHECK(!_downloadingQueue.empty());
  const DriveItem& fileToDequeue = _downloadingQueue.front();
  CHECK([fileIdentifier isEqualToString:fileToDequeue.identifier]);

  if (readyForSelection) {
    // If there is a copy of the file ready, dequeue the file.
    _downloadingQueue.pop();
    [self processDownloadingQueue];
    return;
  }

  // Otherwise, download the file at the front of the queue.
  [self.consumer setDownloadStatus:DriveFileDownloadStatus::kInProgress];
  __weak __typeof(self) weakSelf = self;
  _downloadingFileIdentifier = [fileIdentifier copy];

  // The file to download might be different from the file in the queue i.e. if
  // the file in the queue is a shortcut to a real file, then the real file
  // should be downloaded instead. Only the identifier should matter to use the
  // downloader so a new DriveItem is created with only `identifier` set to the
  // correct value.
  DriveItem fileToDownload;
  fileToDownload.identifier = fileToDequeue.is_shortcut
                                  ? fileToDequeue.shortcut_target_identifier
                                  : fileToDequeue.identifier;
  _downloadingFileDownloadID = _driveDownloader->DownloadFile(
      fileToDownload, fileURL,
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
  // Inform the WebState that the destination URL isn't ready for selection yet.
  ChooseFileTabHelper* tabHelper =
      ChooseFileTabHelper::FromWebState(_webState.get());
  CHECK(tabHelper);
  tabHelper->RemoveFileUrlReadyForSelection(fileURL);
}

// If there is an `error` then shows an alert to let the user decide whether to
// retry the download of the file at the front of the download queue or instead
// skip it. Otherwise, mark the file as ready to be submitted to the page, pop
// it from the queue and continue to process the download queue.
- (void)handleDownloadResponse:(DriveFileDownloadID)driveFileDownloadID
                         error:(NSError*)error
                       fileURL:(NSURL*)fileURL {
  if (!_webState || _webState->IsBeingDestroyed()) {
    return;
  }
  CHECK(!_downloadingQueue.empty());
  CHECK([driveFileDownloadID isEqualToString:_downloadingFileDownloadID]);
  CHECK([_downloadingFileIdentifier
      isEqualToString:_downloadingQueue.front().identifier]);
  // Reset the download ID to indicate that there is no ongoing download
  // anymore.
  _downloadingFileIdentifier = nil;
  _downloadingFileDownloadID = nil;
  if (error) {
    _metricsHelper.hasError = YES;
    // If there is a download error, prepare a callback to optionally try again.
    __weak __typeof(self) weakSelf = self;
    auto retryCallback = base::BindOnce(
        [](DriveFilePickerMediator* mediator, const DriveItem& file) {
          [mediator processDownloadingQueue];
        },
        weakSelf, _downloadingQueue.front());
    auto cancelCallback = base::BindOnce(
        [](DriveFilePickerMediator* mediator, const DriveItem& file) {
          [mediator deselectFile:file];
          [mediator processDownloadingQueue];
        },
        weakSelf, _downloadingQueue.front());
    // Then present an alert to ask the user whether to try again.
    [self.consumer
        showDownloadFailureAlertForFileName:_downloadingQueue.front().name
                                 retryBlock:base::CallbackToBlock(
                                                std::move(retryCallback))
                                cancelBlock:base::CallbackToBlock(
                                                std::move(cancelCallback))];
    return;
  }
  // If the download was successful, add it to the set of files ready for
  // selection, pop the file from the queue and continue processing the download
  // queue.
  ChooseFileTabHelper* tabHelper =
      ChooseFileTabHelper::FromWebState(_webState.get());
  CHECK(tabHelper);
  tabHelper->AddFileUrlReadyForSelection(
      fileURL, _downloadingQueue.front().modified_time);
  _downloadingQueue.pop();
  [self processDownloadingQueue];
}

// Updates the presented list of items by replacing them or appending new ones,
// as a function of the currently presented collection and current projection.
- (void)loadItemsAppending:(BOOL)append
                   delayed:(BOOL)delayed
                  animated:(BOOL)animated {
  // If root items should be loaded, then there are no items to fetch so this
  // asynchronously populates the consumer. Otherwise, this fetches items.

  if (!_driveList) {
    // When disconnected, do nothing.
    return;
  }

  // If there is a timer programmed to fetch items, cancel it.
  _timerBeforeFetch.Stop();
  // Cancel any pending fetch query.
  if (_driveList->IsExecutingQuery()) {
    _timerAfterFetchBeforeClearItems.Stop();
    _driveList->CancelCurrentQuery();
  }

  if (!_collection->IsRoot() || _shouldShowSearchItems) {
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
  __weak __typeof(self) weakSelf = self;

  // If the fetching needs to be delayed, post it for later and return early.
  if (delay != base::TimeDelta()) {
    _timerBeforeFetch.Start(FROM_HERE, delay, base::BindOnce(^{
                              [weakSelf fetchItemsAppending:append
                                                      delay:base::TimeDelta()
                                               delayToRetry:delayToRetry
                                                   animated:animated];
                            }));
    return;
  }

  if (!append) {
    // If this is a new query, then `_nextPageToken` can be reset, current items
    // can be disabled and should be cleared if the query takes too long.
    [self.consumer setEnabledItems:nil];
    [self setNextPageToken:nil];
    _timerAfterFetchBeforeClearItems.Start(
        FROM_HERE, kClearItemsDelay,
        base::BindOnce(
            [](DriveFilePickerMediator* mediator) {
              [mediator clearItemsAndShowLoadingIndicator];
            },
            weakSelf));
  }

  DriveListQuery query = CreateDriveListQuery(
      _collection->GetType(), _collection->GetFolderIdentifier(), _options,
      _shouldShowSearchItems, _searchText, _nextPageToken);

  auto completion = base::BindOnce(
      [](DriveFilePickerMediator* mediator, const base::TimeDelta& delayToRetry,
         BOOL animated, const DriveListResult& result) {
        [mediator handleListItemsResponse:result
                             delayToRetry:delayToRetry
                                 animated:animated];
      },
      weakSelf, delayToRetry, animated);
  if (_shouldShowSearchItems || !_collection->IsSharedDrives()) {
    _driveList->ListFiles(query, std::move(completion));
  } else {
    _driveList->ListSharedDrives(query, std::move(completion));
  }
}

// Processes the `result` of querying a list of items and either replaces
// exisiting items with the new ones (`append` is NO) or appends the new items
// to the existing ones (`append` is YES). If `animated` is YES then new items
// are animated into the consumer. Called as a completion of
// `_driveList->ListItems(...)`.
- (void)handleListItemsResponse:(const DriveListResult&)result
                   delayToRetry:(base::TimeDelta)delayToRetry
                       animated:(BOOL)animated {
  // Cancel clearing items if fetching completed.
  _timerAfterFetchBeforeClearItems.Stop();

  const BOOL append = _nextPageToken != nil;
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

  NSMutableArray<DriveFilePickerItem*>* res = [NSMutableArray array];
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
    DriveFilePickerItem* filePickerItem = DriveItemToDriveFilePickerItem(
        item, _collection->GetType(), _options.sorting_criterion,
        _shouldShowSearchItems, _searchText);
    filePickerItem.enabled = DriveFilePickerItemShouldBeEnabled(
        item, _acceptedTypes, _options.ignore_accepted_types);
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
  [self.consumer populatePrimaryItems:res
                       secondaryItems:nil
                               append:append
                     showSearchHeader:showSearchHeader
                    nextPageAvailable:NO
                             animated:animated];
  [self setNextPageToken:result.next_page_token];
  // If some items were already in the previous list, reconfigure these items.
  [self.consumer reconfigureItemsWithIdentifiers:itemsToReconfigure];
  // Update background of the file picker view.
  if (res.count != 0) {
    // If there are items to present, then clear the background.
    [self.consumer setBackground:DriveFilePickerBackground::kNoBackground];
  } else if (_shouldShowSearchItems ||
             _options.filter != DriveFilePickerFilter::kShowAllFiles) {
    // If there are no items during search or while applying a filter, then show
    // "No matching results" as explanation.
    [self.consumer setBackground:DriveFilePickerBackground::kNoMatchingResults];
  } else {
    // Otherwise, show "Empty folder" as explanation.
    [self.consumer setBackground:DriveFilePickerBackground::kEmptyFolder];
  }
}

// Gives the consumer an account-switching menu with an up-to-date list of
// available identities.
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
      menuToSelectDriveIdentityWithIdentities:signin::GetIdentitiesOnDevice(
                                                  _identityManager,
                                                  _accountManagerService)
                              currentIdentity:_collection->GetIdentity()
                                        block:actionResult];
  // TODO(crbug.com/344812396): Add the new account block.
  UIAction* addAccountAction =
      [actionFactory actionToAddAccountForDriveWithBlock:^{
        [weakSelf.delegate mediatorDidTapAddAccount:weakSelf];
      }];
  [self.consumer setEmailsMenu:[UIMenu menuWithChildren:@[
                   addAccountAction, identitiesMenu
                 ]]];
}

// Sets `fetchedIcon` as icon for all consumer items associated with image link
// `imageLink` and type `imageType`.
- (void)setFetchedIcon:(UIImage*)fetchedIcon
    forItemsWithImageLink:(NSString*)imageLink
                imageType:(DriveItem::ImageType)imageType {
  if (!fetchedIcon) {
    // If the icon could not be fetched, do nothing.
    return;
  }
  // Update items with the same image link in the consumer.
  NSMutableSet<NSString*>* itemsToUpdate = [NSMutableSet set];
  for (const DriveItem& item : _fetchedDriveItems) {
    if ([item.GetImageLink() isEqualToString:imageLink]) {
      [itemsToUpdate addObject:item.identifier];
    }
  }
  [self.consumer setFetchedIcon:fetchedIcon
                       forItems:itemsToUpdate
                    isThumbnail:imageType == DriveItem::ImageType::kThumbnail];
}

// Checks if the pending options i.e. sorting criterion/direction and filter are
// different from the current ones, and if it is the case, updates them.
- (void)applyPendingOptions {
  if (!_pendingOptions) {
    return;
  }

  DriveFilePickerOptions newOptions = *_pendingOptions;
  _pendingOptions.reset();

  BOOL refresh = NO;
  BOOL refreshAcceptedItems = NO;
  if (_options.filter != newOptions.filter) {
    _options.filter = newOptions.filter;
    refresh = YES;
  }
  if (_options.sorting_criterion != newOptions.sorting_criterion) {
    _options.sorting_criterion = newOptions.sorting_criterion;
    refresh = YES;
  }
  if (_options.sorting_direction != newOptions.sorting_direction) {
    _options.sorting_direction = newOptions.sorting_direction;
    refresh = YES;
  }
  if (_options.ignore_accepted_types != newOptions.ignore_accepted_types) {
    _options.ignore_accepted_types = newOptions.ignore_accepted_types;
    refreshAcceptedItems = YES;
  }

  if (refresh) {
    [self.consumer setFilter:_options.filter];
    [self.consumer setSortingCriterion:_options.sorting_criterion
                             direction:_options.sorting_direction];

    [self loadItemsAppending:NO delayed:NO animated:YES];
    // If items were refreshed, the acceptedItems were also automatically
    // refreshed.
    refreshAcceptedItems = NO;
  }
  if (refreshAcceptedItems) {
    [self updateAcceptableItems];
  }
}

// Helper function to compute whether the filter menu should be enabled.
- (BOOL)filterMenuShouldBeEnabled {
  if (_shouldShowSearchItems) {
    return _searchText.length > 0;
  }
  return _collection->SupportsFiltering();
}

// Helper function to compute whether the sorting menu should be enabled.
- (BOOL)sortingMenuShouldBeEnabled {
  if (_shouldShowSearchItems) {
    return _searchText.length > 0;
  }
  return _collection->SupportsSorting();
}

// Sets `_nextPageToken` and updates consumer accordingly.
- (void)setNextPageToken:(NSString*)nextPageToken {
  // Storing the token as-is so no need to call `isEqualToString:`.
  if (_nextPageToken != nextPageToken) {
    _nextPageToken = nextPageToken;
    [self.consumer setNextPageAvailable:(_nextPageToken != nil)];
  }
}

@end
