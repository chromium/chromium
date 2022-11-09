// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/open_in/open_in_mediator.h"

#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/open_in/open_in_tab_helper.h"
#import "ios/chrome/browser/ui/open_in/open_in_controller.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/web_state.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface OpenInMediator () <WebStateListObserving> {
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
  // A map associating webStates with their OpenInControllers.
  std::map<web::WebState*, OpenInController*> _openInControllersForWebStates;
}

// The Browser that accesses the WebStateList.
@property(nonatomic, assign) Browser* browser;

// The WebStateList that this mediator listens for newly added Webstates.
@property(nonatomic, assign) WebStateList* webStateList;

// The base view controller from which to present UI.
@property(nonatomic, weak) UIViewController* baseViewController;

@end

@implementation OpenInMediator

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser {
  self = [super init];
  if (self) {
    _baseViewController = baseViewController;
    _browser = browser;
    _webStateList = browser->GetWebStateList();
    // Set the delegates for all existing webstates in the `_webStateList`.
    for (int i = 0; i < _webStateList->count(); i++) {
      web::WebState* webState = _webStateList->GetWebStateAt(i);
      OpenInTabHelper::FromWebState(webState)->SetDelegate(self);
    }
    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
    _webStateList->AddObserver(_webStateListObserver.get());
  }
  return self;
}

- (void)dealloc {
  [self disconnect];
}

- (void)disableAll {
  for (const auto& element : _openInControllersForWebStates)
    [element.second detachFromWebState];
  _openInControllersForWebStates.clear();
}

- (void)dismissAll {
  for (const auto& element : _openInControllersForWebStates)
    [element.second dismissModalView];
}

- (void)disconnect {
  if (_webStateList) {
    _webStateList->RemoveObserver(_webStateListObserver.get());
    _webStateListObserver.reset();
    _webStateList = nullptr;
  }
  [self disableAll];
}

#pragma mark - WebStateListObserver

- (void)webStateList:(WebStateList*)webStateList
    didInsertWebState:(web::WebState*)webState
              atIndex:(int)index
           activating:(BOOL)activating {
  DCHECK_EQ(_webStateList, webStateList);
  OpenInTabHelper::FromWebState(webState)->SetDelegate(self);
}

#pragma mark - OpenInTabHelperDelegate

// Creates OpenInController and set its base view to the `webState` view. Then
// enables the OpenIn view for the `webState`.
- (void)enableOpenInForWebState:(web::WebState*)webState
                withDocumentURL:(const GURL&)documentURL
              suggestedFileName:(NSString*)suggestedFileName {
  if (!_openInControllersForWebStates[webState]) {
    OpenInController* openInController = [[OpenInController alloc]
        initWithBaseViewController:_baseViewController
                  URLLoaderFactory:webState->GetBrowserState()
                                       ->GetSharedURLLoaderFactory()
                          webState:webState
                           browser:_browser];
    _openInControllersForWebStates[webState] = openInController;
  }
  OpenInController* controller = _openInControllersForWebStates[webState];
  controller.baseView = webState->GetView();
  [controller enableWithDocumentURL:documentURL
                  suggestedFilename:suggestedFileName];
}

// Disables the openIn view for the `webState`.
- (void)disableOpenInForWebState:(web::WebState*)webState {
  if (_openInControllersForWebStates[webState])
    [_openInControllersForWebStates[webState] disable];
}

// Detaches the webState from its' OpenInController.
- (void)destroyOpenInForWebState:(web::WebState*)webState {
  if (!_openInControllersForWebStates[webState])
    return;
  [_openInControllersForWebStates[webState] detachFromWebState];
  _openInControllersForWebStates.erase(webState);
}

@end
