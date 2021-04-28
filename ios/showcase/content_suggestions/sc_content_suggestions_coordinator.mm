// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/content_suggestions/sc_content_suggestions_coordinator.h"

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller.h"
#import "ios/showcase/common/protocol_alerter.h"
#import "ios/showcase/content_suggestions/sc_content_suggestions_data_source.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SCContentSuggestionsCoordinator ()

@property(nonatomic, strong)
    ContentSuggestionsViewController* suggestionViewController;
@property(nonatomic, strong) ProtocolAlerter* alerter;
@property(nonatomic, strong) SCContentSuggestionsDataSource* dataSource;

@end

@implementation SCContentSuggestionsCoordinator

@synthesize baseViewController;
@synthesize suggestionViewController = _suggestionViewController;
@synthesize alerter = _alerter;
@synthesize dataSource = _dataSource;

#pragma mark - Coordinator

- (void)start {
  self.alerter = [[ProtocolAlerter alloc]
      initWithProtocols:@[ @protocol(ContentSuggestionsCommands) ]];
  self.alerter.baseViewController = self.baseViewController;

  _dataSource = [[SCContentSuggestionsDataSource alloc] init];

  _suggestionViewController = [[ContentSuggestionsViewController alloc]
              initWithStyle:CollectionViewControllerStyleDefault
                     offset:0
      refactoredFeedVisible:NO];
  _suggestionViewController.collectionView.accessibilityIdentifier =
      kContentSuggestionsCollectionIdentifier;
  [_suggestionViewController setDataSource:_dataSource];

  _suggestionViewController.suggestionCommandHandler =
      reinterpret_cast<id<ContentSuggestionsCommands>>(self.alerter);

  [self.baseViewController pushViewController:_suggestionViewController
                                     animated:YES];
}

@end
