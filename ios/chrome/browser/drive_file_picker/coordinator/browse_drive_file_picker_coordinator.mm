// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive_file_picker/coordinator/browse_drive_file_picker_coordinator.h"

#import "base/memory/weak_ptr.h"
#import "components/image_fetcher/core/image_data_fetcher.h"
#import "ios/chrome/browser/drive/model/drive_list.h"
#import "ios/chrome/browser/drive/model/drive_service.h"
#import "ios/chrome/browser/drive/model/drive_service_factory.h"
#import "ios/chrome/browser/drive_file_picker/coordinator/browse_drive_file_picker_coordinator_delegate.h"
#import "ios/chrome/browser/drive_file_picker/coordinator/drive_file_picker_mediator.h"
#import "ios/chrome/browser/drive_file_picker/coordinator/drive_file_picker_mediator_delegate.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_navigation_controller.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_table_view_controller.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_table_view_controller_delegate.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/drive_file_picker_commands.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_tab_helper.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

@interface BrowseDriveFilePickerCoordinator () <
    DriveFilePickerMediatorDelegate,
    DriveFilePickerTableViewControllerDelegate,
    BrowseDriveFilePickerCoordinatorDelegate>

@end

@implementation BrowseDriveFilePickerCoordinator {
  DriveFilePickerMediator* _mediator;

  DriveFilePickerTableViewController* _viewController;

  // WebState for which the Drive file picker is presented.
  base::WeakPtr<web::WebState> _webState;

  // A child `BrowseDriveFilePickerCoordinator` created and started to browse an
  // inner folder.
  BrowseDriveFilePickerCoordinator* _childBrowseCoordinator;

  // The query to execute to fetch this collection of items.
  DriveListQuery _query;

  // Title of this collection of items.
  NSString* _title;

  // Filter applied to this collection of items.
  DriveFilePickerFilter _filter;

  // Whether the list of types accepted by the website is ignored.
  BOOL _ignoreAcceptedTypes;

  // Sorting criteria.
  DriveItemsSortingType _sortingCriteria;

  // Sorting direction.
  DriveItemsSortingOrder _sortingDirection;

  // Identity whose Drive is being browsed.
  id<SystemIdentity> _identity;

  drive::DriveService* _driveService;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)
    initWithBaseNavigationViewController:
        (UINavigationController*)baseNavigationController
                                 browser:(Browser*)browser
                                webState:(base::WeakPtr<web::WebState>)webState
                                   title:(NSString*)title
                                   query:(DriveListQuery)query
                                  filter:(DriveFilePickerFilter)filter
                     ignoreAcceptedTypes:(BOOL)ignoreAcceptedTypes
                         sortingCriteria:(DriveItemsSortingType)sortingCriteria
                        sortingDirection:
                            (DriveItemsSortingOrder)sortingDirection
                                identity:(id<SystemIdentity>)identity {
  self = [super initWithBaseViewController:baseNavigationController
                                   browser:browser];
  if (self) {
    CHECK(webState);
    CHECK(title);
    CHECK(identity);
    _baseNavigationController = baseNavigationController;
    _webState = webState;
    _title = [title copy];
    _query = query;
    _filter = filter;
    _ignoreAcceptedTypes = ignoreAcceptedTypes;
    _sortingCriteria = sortingCriteria;
    _sortingDirection = sortingDirection;
    _identity = identity;
  }
  return self;
}

- (void)start {
  ChromeBrowserState* browserState =
      self.browser->GetBrowserState()->GetOriginalChromeBrowserState();
  drive::DriveService* driveService =
      drive::DriveServiceFactory::GetForBrowserState(browserState);
  ChromeAccountManagerService* accountManagerService =
      ChromeAccountManagerServiceFactory::GetForBrowserState(browserState);
  std::unique_ptr<image_fetcher::ImageDataFetcher> imageFetcher =
      std::make_unique<image_fetcher::ImageDataFetcher>(
          browserState->GetSharedURLLoaderFactory());
  _viewController = [[DriveFilePickerTableViewController alloc] init];
  _mediator = [[DriveFilePickerMediator alloc]
           initWithWebState:_webState.get()
                     isRoot:NO
                   identity:_identity
                      title:_title
                      query:_query
                     filter:_filter
        ignoreAcceptedTypes:_ignoreAcceptedTypes
            sortingCriteria:_sortingCriteria
           sortingDirection:_sortingDirection
               driveService:driveService
      accountManagerService:accountManagerService
               imageFetcher:std::move(imageFetcher)];

  id<DriveFilePickerCommands> driveFilePickerHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), DriveFilePickerCommands);
  _viewController.mutator = _mediator;
  _viewController.delegate = self;
  _mediator.consumer = _viewController;
  _mediator.delegate = self;
  _mediator.driveFilePickerHandler = driveFilePickerHandler;
  [_baseNavigationController pushViewController:_viewController animated:YES];
}

- (void)stop {
  [_mediator disconnect];
  if (![_viewController isMovingFromParentViewController]) {
    [_viewController.navigationController popViewControllerAnimated:YES];
  }
  [_childBrowseCoordinator stop];
  _childBrowseCoordinator = nil;
  _mediator = nil;
  _viewController = nil;

  _identity = nil;
  _query = {};
}

#pragma mark - DriveFilePickerMediatorDelegate

- (void)browseDriveCollectionWithMediator:
            (DriveFilePickerMediator*)driveFilePickerMediator
                                    title:(NSString*)title
                                    query:(DriveListQuery)query
                                   filter:(DriveFilePickerFilter)filter
                      ignoreAcceptedTypes:(BOOL)ignoreAcceptedTypes
                          sortingCriteria:(DriveItemsSortingType)sortingCriteria
                         sortingDirection:
                             (DriveItemsSortingOrder)sortingDirection {
  _childBrowseCoordinator = [[BrowseDriveFilePickerCoordinator alloc]
      initWithBaseNavigationViewController:_baseNavigationController
                                   browser:self.browser
                                  webState:_webState
                                     title:title
                                     query:query
                                    filter:filter
                       ignoreAcceptedTypes:ignoreAcceptedTypes
                           sortingCriteria:sortingCriteria
                          sortingDirection:sortingDirection
                                  identity:_identity];
  _childBrowseCoordinator.delegate = self;
  [_childBrowseCoordinator start];
}

- (void)mediatorDidSubmitFileSelection:(DriveFilePickerMediator*)mediator {
  __weak id<DriveFilePickerCommands> driveFilePickerHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(),
                         DriveFilePickerCommands);
  [self.baseNavigationController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:^{
                           [driveFilePickerHandler hideDriveFilePicker];
                         }];
}

- (void)browseToParentWithMediator:(DriveFilePickerMediator*)mediator {
  [self.delegate coordinatorShouldStop:self];
}

#pragma mark - DriveFilePickerTableViewControllerDelegate

- (void)viewControllerDidDisappear:(UIViewController*)viewController {
  [self.delegate coordinatorShouldStop:self];
}

#pragma mark - BrowseDriveFilePickerCoordinatorDelegate

- (void)coordinatorShouldStop:(ChromeCoordinator*)coordinator {
  CHECK(coordinator == _childBrowseCoordinator);
  [_childBrowseCoordinator stop];
  _childBrowseCoordinator = nil;
}

@end
