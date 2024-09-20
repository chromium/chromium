// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive_file_picker/coordinator/drive_file_picker_mediator.h"

#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/image_fetcher/core/cached_image_fetcher.h"
#import "components/image_fetcher/core/image_data_fetcher.h"
#import "ios/chrome/browser/drive/model/drive_list.h"
#import "ios/chrome/browser/drive/model/drive_service_factory.h"
#import "ios/chrome/browser/drive/model/test_drive_file_downloader.h"
#import "ios/chrome/browser/drive/model/test_drive_list.h"
#import "ios/chrome/browser/drive/model/test_drive_service.h"
#import "ios/chrome/browser/drive_file_picker/coordinator/drive_file_picker_mediator_delegate.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_constants.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_consumer.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_item.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_tab_helper.h"
#import "ios/chrome/browser/web/model/choose_file/fake_choose_file_controller.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

// Fake delegate for `DriveFilePickerMediator`.
@interface FakeDriveFilePickerMediatorDelegate
    : NSObject <DriveFilePickerMediatorDelegate>

@property(nonatomic, copy) NSString* titleOfBrowsedCollection;
@property(nonatomic, assign) DriveListQuery queryOfBrowsedCollection;
@property(nonatomic, assign) DriveFilePickerFilter filter;
@property(nonatomic, assign) BOOL ignoreAcceptedTypes;
@property(nonatomic, assign) DriveItemsSortingType sortingCriteria;
@property(nonatomic, assign) DriveItemsSortingOrder sortingDirection;

@property(nonatomic, assign) BOOL fileSelectionSubmitted;

@end

@implementation FakeDriveFilePickerMediatorDelegate

- (void)browseDriveCollectionWithMediator:
            (DriveFilePickerMediator*)driveFilePickerMediator
                                    title:(NSString*)title
                                    query:(DriveListQuery)query
                                   filter:(DriveFilePickerFilter)filter
                      ignoreAcceptedTypes:(BOOL)ignoreAcceptedTypes
                          sortingCriteria:(DriveItemsSortingType)sortingCriteria
                         sortingDirection:
                             (DriveItemsSortingOrder)sortingDirection {
  self.titleOfBrowsedCollection = title;
  self.queryOfBrowsedCollection = query;
  self.filter = filter;
  self.ignoreAcceptedTypes = ignoreAcceptedTypes;
  self.sortingCriteria = sortingCriteria;
  self.sortingDirection = sortingDirection;
}

- (void)mediatorDidSubmitFileSelection:(DriveFilePickerMediator*)mediator {
  self.fileSelectionSubmitted = YES;
}

- (void)browseToParentWithMediator:(DriveFilePickerMediator*)mediator {
}

@end

// Fake consumer for `DriveFilePickerMediator`.
@interface FakeDriveFilePickerConsumer : NSObject <DriveFilePickerConsumer>

@property(nonatomic, assign) DriveFileDownloadStatus downloadStatus;
@property(nonatomic, assign) DriveItemsSortingType sortingCriteria;
@property(nonatomic, assign) DriveItemsSortingOrder sortingDirection;
@property(nonatomic, strong) NSArray<DriveFilePickerItem*>* driveItems;

@end

@implementation FakeDriveFilePickerConsumer

- (instancetype)init {
  self = [super init];
  if (self) {
    _driveItems = [NSMutableArray array];
  }
  return self;
}

- (void)setSelectedUserIdentityEmail:(NSString*)selectedUserIdentityEmail {
}

- (void)setCurrentDriveFolderTitle:(NSString*)currentDriveFolderTitle {
}

- (void)populateItems:(NSArray<DriveFilePickerItem*>*)driveItems
               append:(BOOL)append
    nextPageAvailable:(BOOL)nextPageAvailable {
  if (append) {
    self.driveItems =
        [self.driveItems arrayByAddingObjectsFromArray:driveItems];
  } else {
    self.driveItems = driveItems;
  }
}

- (void)setEmailsMenu:(UIMenu*)emailsMenu {
}

- (void)setIcon:(UIImage*)iconImage forItem:(NSString*)itemIdentifier {
}

- (void)setDownloadStatus:(DriveFileDownloadStatus)downloadStatus {
  _downloadStatus = downloadStatus;
}

- (void)setEnabledItems:(NSSet<NSString*>*)identifiers {
}

- (void)setAllFilesEnabled:(BOOL)allFilesEnabled {
}

- (void)setFilter:(DriveFilePickerFilter)filter {
}

- (void)setSortingCriteria:(DriveItemsSortingType)criteria
                 direction:(DriveItemsSortingOrder)direction {
  self.sortingCriteria = criteria;
  self.sortingDirection = direction;
}

- (void)showInterruptionAlertWithBlock:(ProceduralBlock)block {
}

- (void)setSelectedItemIdentifier:(NSString*)selectedIdentifier {
}

@end

// Test fixture for testing DriveFilePickerMediator class.
class DriveFilePickerMediatorTest : public PlatformTest {
 protected:
  void SetUp() final {
    PlatformTest::SetUp();
    scoped_feature_list_.InitAndEnableFeature(kIOSSaveToDrive);
    browser_state_ = TestChromeBrowserState::Builder().Build();
    drive_service_ =
        drive::DriveServiceFactory::GetForBrowserState(browser_state_.get());
    _accountManagerService =
        ChromeAccountManagerServiceFactory::GetForBrowserState(
            browser_state_.get());
    image_fetcher_ = std::make_unique<image_fetcher::ImageDataFetcher>(
        browser_state_.get()->GetSharedURLLoaderFactory());
    web_state_ = std::make_unique<web::FakeWebState>();
    StartChoosingFiles();
    mediator_ = [[DriveFilePickerMediator alloc]
             initWithWebState:web_state_.get()
                       isRoot:YES
                     identity:[FakeSystemIdentity fakeIdentity1]
                        title:nil
                        query:{}
                       filter:DriveFilePickerFilter::kShowAllFiles
          ignoreAcceptedTypes:NO
              sortingCriteria:DriveItemsSortingType::kName
             sortingDirection:DriveItemsSortingOrder::kAscending
                 driveService:drive_service_
        accountManagerService:_accountManagerService
                 imageFetcher:std::move(image_fetcher_)];
    // Start file selection in `web_state_`.
    choose_file_tab_helper_ =
        ChooseFileTabHelper::GetOrCreateForWebState(web_state_.get());
    auto controller = std::make_unique<FakeChooseFileController>(
        ChooseFileEvent(false, std::vector<std::string>{},
                        std::vector<std::string>{}, web_state_.get()));
    choose_file_tab_helper_->StartChoosingFiles(std::move(controller));
    fake_delegate_ = [[FakeDriveFilePickerMediatorDelegate alloc] init];
    mediator_.delegate = fake_delegate_;
    fake_consumer_ = [[FakeDriveFilePickerConsumer alloc] init];
    mediator_.consumer = fake_consumer_;
    std::unique_ptr<TestDriveList> drive_list =
        std::make_unique<TestDriveList>([FakeSystemIdentity fakeIdentity1]);
    drive_list_ = drive_list.get();
    GetTestDriveService()->SetDriveList(std::move(drive_list));
    std::unique_ptr<TestDriveFileDownloader> file_downloader =
        std::make_unique<TestDriveFileDownloader>(
            [FakeSystemIdentity fakeIdentity1]);
    file_downloader_ = file_downloader.get();
    GetTestDriveService()->SetFileDownloader(std::move(file_downloader));
  }

  // Starts file selection in the WebState.
  void StartChoosingFiles() {
    ChooseFileTabHelper* tab_helper =
        ChooseFileTabHelper::GetOrCreateForWebState(web_state_.get());
    auto controller = std::make_unique<FakeChooseFileController>(
        ChooseFileEvent(false, std::vector<std::string>{},
                        std::vector<std::string>{}, web_state_.get()));
    tab_helper->StartChoosingFiles(std::move(controller));
  }

  // Returns the testing Drive service.
  drive::TestDriveService* GetTestDriveService() {
    return static_cast<drive::TestDriveService*>(
        drive::DriveServiceFactory::GetForBrowserState(browser_state_.get()));
  }

  void TearDown() final {
    [mediator_ disconnect];
    mediator_ = nil;
    PlatformTest::TearDown();
  }

  using TaskEnvironment = base::test::TaskEnvironment;
  TaskEnvironment task_environment_{TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList scoped_feature_list_;
  DriveFilePickerMediator* mediator_;
  std::unique_ptr<web::FakeWebState> web_state_;
  raw_ptr<ChooseFileTabHelper> choose_file_tab_helper_;
  raw_ptr<drive::DriveService> drive_service_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  raw_ptr<ChromeAccountManagerService> _accountManagerService;
  std::unique_ptr<image_fetcher::ImageDataFetcher> image_fetcher_;
  FakeDriveFilePickerMediatorDelegate* fake_delegate_;
  FakeDriveFilePickerConsumer* fake_consumer_;
  raw_ptr<TestDriveList> drive_list_;
  raw_ptr<TestDriveFileDownloader> file_downloader_;
};

// Tests that disconnecting the root mediator stops the file selection.
TEST_F(DriveFilePickerMediatorTest, StopsChoosingFiles) {
  EXPECT_TRUE(choose_file_tab_helper_->IsChoosingFiles());
  // Disconnect the mediator.
  [mediator_ disconnect];
  mediator_ = nil;
  EXPECT_FALSE(choose_file_tab_helper_->IsChoosingFiles());
}

// Tests that selecting a collection Drive item browses this collection.
TEST_F(DriveFilePickerMediatorTest, SelectCollectionItemBrowsesCollection) {
  // My Drive items have 'root' as their folder identifier.
  DriveFilePickerItem* myDriveFilePickerItem =
      [DriveFilePickerItem myDriveItem];
  [mediator_ selectDriveItem:myDriveFilePickerItem.identifier];
  EXPECT_NSEQ(@"root",
              fake_delegate_.queryOfBrowsedCollection.folder_identifier);
  // Starred items have 'starred' equal to true.
  DriveFilePickerItem* starredItemIdentifier =
      [DriveFilePickerItem starredItem];
  [mediator_ selectDriveItem:starredItemIdentifier.identifier];
  EXPECT_NSEQ(@"starred=true",
              fake_delegate_.queryOfBrowsedCollection.extra_term);
  // Recent items are all items sorted by recency, except for folders.
  DriveFilePickerItem* recentItemIdentifier = [DriveFilePickerItem recentItem];
  [mediator_ selectDriveItem:recentItemIdentifier.identifier];
  EXPECT_NSEQ(@"mimeType!='application/vnd.google-apps.folder'",
              fake_delegate_.queryOfBrowsedCollection.extra_term);
  EXPECT_NSEQ(@"recency desc",
              fake_delegate_.queryOfBrowsedCollection.order_by);
  // 'Shared with me' have 'sharedWithMe' equal to true and are sorted by
  // sharing time.
  DriveFilePickerItem* sharedWithMeItemIdentifier =
      [DriveFilePickerItem sharedWithMeItem];
  [mediator_ selectDriveItem:sharedWithMeItemIdentifier.identifier];
  EXPECT_NSEQ(@"sharedWithMe=true",
              fake_delegate_.queryOfBrowsedCollection.extra_term);
  EXPECT_NSEQ(@"sharedWithMeTime desc",
              fake_delegate_.queryOfBrowsedCollection.order_by);

  // Items in a given folder have 'folder_identifier' equal to that folder's
  // identifier.
  // Set up Drive list to return a folder to browse into.
  DriveItem folder_to_browse;
  folder_to_browse.is_folder = true;
  folder_to_browse.can_download = false;
  folder_to_browse.identifier = @"fake_folder_identifier";
  folder_to_browse.name = @"Fake Folder";
  DriveListResult fake_result;
  fake_result.items = {folder_to_browse};
  drive_list_->SetDriveListResult(fake_result);
  // Fetch items.
  drive_list_->SetListItemsCompletionQuitClosure(
      task_environment_.QuitClosure());
  [mediator_ fetchNextPage];
  task_environment_.RunUntilQuit();
  // Test items have been forwarded to the consumer.
  EXPECT_NE(nil, fake_consumer_.driveItems);
  EXPECT_EQ(1U, fake_consumer_.driveItems.count);
  EXPECT_NSEQ(folder_to_browse.identifier,
              fake_consumer_.driveItems[0].identifier);
  // Select the folder.
  [mediator_ selectDriveItem:folder_to_browse.identifier];
  EXPECT_NSEQ(folder_to_browse.identifier,
              fake_delegate_.queryOfBrowsedCollection.folder_identifier);
}

// Tests that setting the sorting criteria and direction updates the consumer
// and fetches new items, unless they have not changed.
TEST_F(DriveFilePickerMediatorTest, SelectSortingCriteria) {
  // Setting to the same criteria and direction should not fetch new items.
  [mediator_ setSortingCriteria:DriveItemsSortingType::kName
                      direction:DriveItemsSortingOrder::kAscending];
  EXPECT_EQ(DriveItemsSortingType::kName, fake_consumer_.sortingCriteria);
  EXPECT_EQ(DriveItemsSortingOrder::kAscending,
            fake_consumer_.sortingDirection);
  EXPECT_EQ(0U, fake_consumer_.driveItems.count);
  // Changing either criteria or direction should update consumer and fetch new
  // items.
  drive_list_->SetListItemsCompletionQuitClosure(
      task_environment_.QuitClosure());
  [mediator_ setSortingCriteria:DriveItemsSortingType::kModificationTime
                      direction:DriveItemsSortingOrder::kDescending];
  EXPECT_EQ(DriveItemsSortingType::kModificationTime,
            fake_consumer_.sortingCriteria);
  EXPECT_EQ(DriveItemsSortingOrder::kDescending,
            fake_consumer_.sortingDirection);
  task_environment_.RunUntilQuit();
  // This test assumes that the fake DriveList object returns items by default.
  EXPECT_NE(0U, fake_consumer_.driveItems.count);
  fake_consumer_.driveItems = nil;
}

// Tests that selecting a file and submitting the selection works as expected.
TEST_F(DriveFilePickerMediatorTest, SubmitFileSelection) {
  // Set up Drive list to return a downloadable file.
  DriveItem file_to_select;
  file_to_select.is_folder = false;
  file_to_select.can_download = true;
  file_to_select.identifier = [[NSUUID UUID] UUIDString];
  file_to_select.name = @"Fake File";
  DriveListResult fake_result;
  fake_result.items = {file_to_select};
  drive_list_->SetDriveListResult(fake_result);

  // Fetch items.
  drive_list_->SetListItemsCompletionQuitClosure(
      task_environment_.QuitClosure());
  [mediator_ fetchNextPage];
  task_environment_.RunUntilQuit();
  EXPECT_NE(nil, fake_consumer_.driveItems);
  EXPECT_EQ(1U, fake_consumer_.driveItems.count);
  EXPECT_NSEQ(file_to_select.identifier,
              fake_consumer_.driveItems[0].identifier);

  // Download the file.
  file_downloader_->SetDownloadFileCompletionQuitClosure(
      task_environment_.QuitClosure());
  EXPECT_EQ(DriveFileDownloadStatus::kNotStarted,
            fake_consumer_.downloadStatus);
  [mediator_ selectDriveItem:fake_consumer_.driveItems[0].identifier];
  EXPECT_EQ(DriveFileDownloadStatus::kInProgress,
            fake_consumer_.downloadStatus);
  task_environment_.RunUntilQuit();
  EXPECT_EQ(DriveFileDownloadStatus::kSuccess, fake_consumer_.downloadStatus);
  EXPECT_FALSE(fake_delegate_.fileSelectionSubmitted);
  [mediator_ submitFileSelection];
  EXPECT_TRUE(fake_delegate_.fileSelectionSubmitted);
}
