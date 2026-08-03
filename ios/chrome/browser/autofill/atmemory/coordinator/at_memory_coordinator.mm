// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/coordinator/at_memory_coordinator.h"

#import "ios/chrome/browser/autofill/atmemory/coordinator/at_memory_granular_fill_coordinator.h"
#import "ios/chrome/browser/autofill/atmemory/coordinator/at_memory_search_coordinator.h"
#import "ios/chrome/browser/autofill/atmemory/public/at_memory_commands.h"
#import "ios/chrome/browser/autofill/atmemory/public/at_memory_fill_commands.h"
#import "ios/chrome/browser/autofill/atmemory/public/at_memory_search_result_commands.h"
#import "ios/chrome/browser/autofill/manual_fill/public/manual_fill_content_injector.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"

@interface AtMemoryCoordinator () <AtMemoryFillCommands,
                                   AtMemorySearchResultCommands,
                                   UIAdaptivePresentationControllerDelegate>
@end

@implementation AtMemoryCoordinator {
  // NavigationController for the AtMemory flow.
  UINavigationController* _navigationController;
  // Injector for manual fill data.
  id<ManualFillContentInjector> _contentInjector;
  // Coordinator for AtMemory search.
  AtMemorySearchCoordinator* _atMemorySearchCoordinator;
  // Coordinator for AtMemory granular fill.
  AtMemoryGranularFillCoordinator* _atMemoryGranularFillCoordinator;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                           contentInjector:
                               (id<ManualFillContentInjector>)contentInjector {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _contentInjector = contentInjector;
  }
  return self;
}

- (void)start {
  _navigationController = [[UINavigationController alloc] init];
  _navigationController.presentationController.delegate = self;

  _atMemorySearchCoordinator = [[AtMemorySearchCoordinator alloc]
      initWithBaseNavigationController:_navigationController
                               browser:self.browser];
  _atMemorySearchCoordinator.searchResultHandler = self;
  _atMemorySearchCoordinator.fillHandler = self;
  [_atMemorySearchCoordinator start];

  _navigationController.modalPresentationStyle = UIModalPresentationPageSheet;
  UISheetPresentationController* sheet =
      _navigationController.sheetPresentationController;
  if (sheet) {
    sheet.detents = @[
      [UISheetPresentationControllerDetent mediumDetent],
      [UISheetPresentationControllerDetent largeDetent]
    ];
    sheet.prefersGrabberVisible = YES;
    sheet.prefersScrollingExpandsWhenScrolledToEdge = YES;
    sheet.prefersEdgeAttachedInCompactHeight = YES;
  }

  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [_atMemoryGranularFillCoordinator stop];
  _atMemoryGranularFillCoordinator = nil;

  [_atMemorySearchCoordinator stop];
  _atMemorySearchCoordinator = nil;

  [_navigationController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  _navigationController = nil;
}

#pragma mark - AtMemorySearchResultCommands

- (void)showAtMemoryGranularFillWithResult:(MemorySearchResult*)result {
  [_atMemoryGranularFillCoordinator stop];

  _atMemoryGranularFillCoordinator = [[AtMemoryGranularFillCoordinator alloc]
      initWithBaseNavigationController:_navigationController
                               browser:self.browser];
  _atMemoryGranularFillCoordinator.fillHandler = self;
  [_atMemoryGranularFillCoordinator start];
}

#pragma mark - AtMemoryFillCommands

- (void)fillWithContent:(NSString*)content {
  [_contentInjector userDidPickContent:content
                         passwordField:NO
                         requiresHTTPS:YES
                       jumpToNextField:NO];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  id<AtMemoryCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), AtMemoryCommands);
  [handler dismissAtMemory];
}

@end
