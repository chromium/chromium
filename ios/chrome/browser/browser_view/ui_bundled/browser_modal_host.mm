// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browser_view/ui_bundled/browser_modal_host.h"

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/intelligence/page_action_menu/coordinator/page_action_menu_coordinator.h"
#import "ios/chrome/browser/page_info/coordinator/page_info_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/page_action_menu_commands.h"
#import "ios/chrome/browser/shared/public/commands/page_info_commands.h"

@interface BrowserModalHost () <PageActionMenuCommands, PageInfoCommands>

// The webState of the active tab.
@property(nonatomic, readonly) web::WebState* activeWebState;
// The dispatcher.
@property(nonatomic, readonly) CommandDispatcher* dispatcher;

@end

@implementation BrowserModalHost {
  raw_ptr<Browser> _browser;
  UIViewController* _baseViewController;

  // The list of sub-coordinators
  PageActionMenuCoordinator* _pageActionMenuCoordinator;
  PageInfoCoordinator* _pageInfoCoordinator;
}

- (instancetype)initWithBrowser:(Browser*)browser {
  self = [super init];
  if (self) {
    _browser = browser;
  }
  return self;
}

- (void)startHostingCommandProtocols {
  [self startDispatching];
}

- (void)setBaseViewControllerForModals:(UIViewController*)baseViewController {
  _baseViewController = baseViewController;
}

- (void)stopHostingCommandProtocols {
  [self clearPresentedState];
  [self.dispatcher stopDispatchingToTarget:self];
}

- (void)clearPresentedState {
  [self hidePageInfo];
  [self dismissPageActionMenuWithCompletion:nil];
}

#pragma mark - Private properties

- (web::WebState*)activeWebState {
  WebStateList* webStateList = _browser->GetWebStateList();
  return webStateList ? webStateList->GetActiveWebState() : nullptr;
}

- (CommandDispatcher*)dispatcher {
  return _browser->GetCommandDispatcher();
}

#pragma mark - Private helpers

// Starts dispatching to the various command protocols.
- (void)startDispatching {
  NSArray<Protocol*>* protocols = @[
    @protocol(PageActionMenuCommands),
    @protocol(PageInfoCommands),
  ];

  for (Protocol* protocol in protocols) {
    [self.dispatcher startDispatchingToTarget:self forProtocol:protocol];
  }
}

#pragma mark - PageActionMenuCommands

- (void)showPageActionMenu {
  if (!self.activeWebState) {
    // The page action menu requires an active tab. Return early if there is
    // none.
    return;
  }
  // TODO(crbug.com/465505528) Propagate page action menu entry point source to
  // page action menu coordinator.
  _pageActionMenuCoordinator = [[PageActionMenuCoordinator alloc]
      initWithBaseViewController:_baseViewController
                         browser:_browser];
  [_pageActionMenuCoordinator start];
}

- (void)dismissPageActionMenuWithCompletion:(ProceduralBlock)completion {
  [_pageActionMenuCoordinator stopWithCompletion:completion];
  _pageActionMenuCoordinator = nil;
}

#pragma mark - PageInfoCommands

- (void)showPageInfo {
  PageInfoCoordinator* pageInfoCoordinator = [[PageInfoCoordinator alloc]
      initWithBaseViewController:_baseViewController
                         browser:_browser];
  [_pageInfoCoordinator stop];
  _pageInfoCoordinator = pageInfoCoordinator;
  [_pageInfoCoordinator start];
}

- (void)hidePageInfo {
  [_pageInfoCoordinator stop];
  _pageInfoCoordinator = nil;
}

@end
