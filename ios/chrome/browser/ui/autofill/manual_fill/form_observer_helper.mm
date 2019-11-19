// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/form_observer_helper.h"

#import "components/autofill/ios/form_util/form_activity_observer_bridge.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FormObserverHelper ()<FormActivityObserver, WebStateListObserving>
// The WebStateList this instance is observing in order to update the
// active WebState.
@property(nonatomic, assign) WebStateList* webStateList;

// The WebState this instance is observing. Can be nullptr.
@property(nonatomic, assign) web::WebState* webState;

@end

@implementation FormObserverHelper {
  // Bridge to observe the web state list from Objective-C.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;

  // Bridge to observe form activity in |_webState|.
  std::unique_ptr<autofill::FormActivityObserverBridge>
      _formActivityObserverBridge;
}

@synthesize delegate = _delegate;

- (instancetype)initWithWebStateList:(WebStateList*)webStateList {
  DCHECK(webStateList);
  self = [super init];
  if (self) {
    _webStateList = webStateList;
    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
    _webStateList->AddObserver(_webStateListObserver.get());
    web::WebState* webState = webStateList->GetActiveWebState();
    if (webState) {
      _webState = webState;
      _formActivityObserverBridge =
          std::make_unique<autofill::FormActivityObserverBridge>(_webState,
                                                                 self);
    }
  }
  return self;
}

- (void)dealloc {
  if (_webState) {
    _formActivityObserverBridge.reset();
    _webState = nullptr;
  }
  if (_webStateList) {
    _webStateList->RemoveObserver(_webStateListObserver.get());
    _webStateListObserver.reset();
    _webStateList = nullptr;
  }
  _formActivityObserverBridge.reset();
}

#pragma mark - FormActivityObserver

- (void)webState:(web::WebState*)webState
    didRegisterFormActivity:(const autofill::FormActivityParams&)params
                    inFrame:(web::WebFrame*)frame {
  if ([self.delegate respondsToSelector:@selector
                     (webState:didRegisterFormActivity:inFrame:)]) {
    [self.delegate webState:webState
        didRegisterFormActivity:params
                        inFrame:frame];
  }
}

- (void)webState:(web::WebState*)webState
    didSubmitDocumentWithFormNamed:(const std::string&)formName
                          withData:(const std::string&)formData
                    hasUserGesture:(BOOL)hasUserGesture
                   formInMainFrame:(BOOL)formInMainFrame
                           inFrame:(web::WebFrame*)frame {
  if ([self.delegate respondsToSelector:@selector
                     (webState:didSubmitDocumentWithFormNamed:withData
                                 :hasUserGesture:formInMainFrame:inFrame:)]) {
    [self.delegate webState:webState
        didSubmitDocumentWithFormNamed:formName
                              withData:formData
                        hasUserGesture:hasUserGesture
                       formInMainFrame:formInMainFrame
                               inFrame:frame];
  }
}

#pragma mark - CRWWebStateListObserver

- (void)webStateList:(WebStateList*)webStateList
    didChangeActiveWebState:(web::WebState*)newWebState
                oldWebState:(web::WebState*)oldWebState
                    atIndex:(int)atIndex
                     reason:(int)reason {
  self.webState = newWebState;
}

#pragma mark - Setters

// Sets the new web state and detaches from the previous web state.
- (void)setWebState:(web::WebState*)webState {
  if (_webState) {
    _formActivityObserverBridge.reset();
  }

  _webState = webState;

  if (_webState) {
    _formActivityObserverBridge =
        std::make_unique<autofill::FormActivityObserverBridge>(_webState, self);
  }
}

@end
