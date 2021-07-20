// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/sharing/sharing_coordinator.h"

#import <MaterialComponents/MaterialSnackbar.h>

#import "base/ios/block_types.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/activity_services/activity_params.h"
#import "ios/chrome/browser/ui/activity_services/activity_service_coordinator.h"
#import "ios/chrome/browser/ui/activity_services/requirements/activity_service_positioner.h"
#import "ios/chrome/browser/ui/activity_services/requirements/activity_service_presentation.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_edit_coordinator.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_mediator.h"
#import "ios/chrome/browser/ui/commands/bookmark_add_command.h"
#import "ios/chrome/browser/ui/commands/bookmarks_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/qr_generation_commands.h"
#import "ios/chrome/browser/ui/commands/snackbar_commands.h"
#import "ios/chrome/browser/ui/qr_generator/qr_generator_coordinator.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using bookmarks::BookmarkNode;

@interface SharingCoordinator () <ActivityServicePositioner,
                                  ActivityServicePresentation,
                                  BookmarkEditCoordinatorDelegate,
                                  BookmarksCommands,
                                  QRGenerationCommands>
@property(nonatomic, strong)
    ActivityServiceCoordinator* activityServiceCoordinator;

@property(nonatomic, strong) BookmarkEditCoordinator* bookmarkEditCoordinator;

@property(nonatomic, strong) QRGeneratorCoordinator* qrGeneratorCoordinator;

@property(nonatomic, strong) ActivityParams* params;

@property(nonatomic, weak) UIView* originView;

@property(nonatomic, assign) CGRect originRect;

@property(nonatomic, weak) UIBarButtonItem* anchor;

@end

@implementation SharingCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                    params:(ActivityParams*)params
                                originView:(UIView*)originView {
  DCHECK(originView);
  self = [self initWithBaseViewController:viewController
                                  browser:browser
                                   params:params
                               originView:originView
                               originRect:originView.bounds
                                   anchor:nil];
  return self;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                    params:(ActivityParams*)params
                                    anchor:(UIBarButtonItem*)anchor {
  DCHECK(anchor);
  self = [self initWithBaseViewController:viewController
                                  browser:browser
                                   params:params
                               originView:nil
                               originRect:CGRectZero
                                   anchor:anchor];
  return self;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                    params:(ActivityParams*)params
                                originView:(UIView*)originView
                                originRect:(CGRect)originRect
                                    anchor:(UIBarButtonItem*)anchor {
  DCHECK(params);
  if (self = [super initWithBaseViewController:viewController
                                       browser:browser]) {
    _params = params;
    _originView = originView;
    _originRect = originRect;
    _anchor = anchor;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  self.activityServiceCoordinator = [[ActivityServiceCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                          params:self.params];

  self.activityServiceCoordinator.positionProvider = self;
  self.activityServiceCoordinator.presentationProvider = self;
  self.activityServiceCoordinator.scopedHandler = self;

  [self.activityServiceCoordinator start];
}

- (void)stop {
  [self activityServiceDidEndPresenting];
  [self bookmarkEditDismissed:self.bookmarkEditCoordinator];
  [self hideQRCode];
  self.originView = nil;
}

#pragma mark - ActivityServicePositioner

- (UIView*)sourceView {
  return self.originView;
}

- (CGRect)sourceRect {
  return self.originRect;
}

- (UIBarButtonItem*)barButtonItem {
  return self.anchor;
}

#pragma mark - ActivityServicePresentation

- (void)activityServiceDidEndPresenting {
  [self.activityServiceCoordinator stop];
  self.activityServiceCoordinator = nil;
}

#pragma mark - BookmarksCommands

- (void)bookmarkPage:(BookmarkAddCommand*)command {
  DCHECK(command);
  GURL URL = command.URLs.firstObject.URL;
  NSString* title = command.URLs.firstObject.title;
  DCHECK(URL.is_valid());

  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  bookmarks::BookmarkModel* bookmarkModel =
      ios::BookmarkModelFactory::GetForBrowserState(browserState);
  if (!bookmarkModel && bookmarkModel->loaded()) {
    return;
  }

  const BookmarkNode* node =
      bookmarkModel->GetMostRecentlyAddedUserNodeForURL(URL);
  if (node) {
    // Trigger the Edit bookmark scenario.
    [self editBookmark:node];
  } else {
    // Add the bookmark and show a snackbar message.
    id<SnackbarCommands> handler = HandlerForProtocol(
        self.browser->GetCommandDispatcher(), SnackbarCommands);
    BookmarkMediator* mediator = [[BookmarkMediator alloc]
        initWithBrowserState:self.browser->GetBrowserState()];

    __weak __typeof(self) weakSelf = self;
    ProceduralBlock editAction = ^{
      if (!weakSelf || !bookmarkModel) {
        return;
      }

      const BookmarkNode* newNode =
          bookmarkModel->GetMostRecentlyAddedUserNodeForURL(URL);
      DCHECK(newNode);
      if (newNode) {
        [weakSelf editBookmark:newNode];
      }
    };

    MDCSnackbarMessage* snackbarMessage =
        [mediator addBookmarkWithTitle:title URL:URL editAction:editAction];
    [handler showSnackbarMessage:snackbarMessage];
  }
}

#pragma mark - BookmarkEditCoordinatorDelegate

- (void)bookmarkEditDismissed:(BookmarkEditCoordinator*)coordinator {
  DCHECK(self.bookmarkEditCoordinator == coordinator);
  [self.bookmarkEditCoordinator stop];
  self.bookmarkEditCoordinator = nil;
}

#pragma mark - QRGenerationCommands

- (void)generateQRCode:(GenerateQRCodeCommand*)command {
  self.qrGeneratorCoordinator = [[QRGeneratorCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                           title:command.title
                             URL:command.URL
                         handler:self];
  [self.qrGeneratorCoordinator start];
}

- (void)hideQRCode {
  [self.qrGeneratorCoordinator stop];
  self.qrGeneratorCoordinator = nil;
}

#pragma mark - Private Methods

- (void)editBookmark:(const BookmarkNode*)bookmark {
  self.bookmarkEditCoordinator = [[BookmarkEditCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                        bookmark:bookmark];
  self.bookmarkEditCoordinator.delegate = self;

  [self.bookmarkEditCoordinator start];
}

@end
