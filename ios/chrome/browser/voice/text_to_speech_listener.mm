// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/voice/text_to_speech_listener.h"

#include <memory>

#include "base/logging.h"
#import "ios/chrome/browser/voice/text_to_speech_parser.h"
#import "ios/chrome/browser/voice/voice_search_url_rewriter.h"
#include "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#include "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_observer_bridge.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - TextToSpeechListener Private Interface

@interface TextToSpeechListener ()<CRWWebStateObserver>

// The TextToSpeechListenerDelegate passed on initialization.
@property(weak, nonatomic, readonly) id<TextToSpeechListenerDelegate> delegate;

@end

#pragma mark - TextToSpeechListener

@implementation TextToSpeechListener {
  // The WebStateObserverBridge that listens for WebState events.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;
}

@synthesize webState = _webState;
@synthesize delegate = _delegate;

- (instancetype)initWithWebState:(web::WebState*)webState
                        delegate:(id<TextToSpeechListenerDelegate>)delegate {
  if ((self = [super init])) {
    DCHECK(webState);
    DCHECK(delegate);
    _webState = webState;
    _delegate = delegate;

    _webStateObserver = std::make_unique<web::WebStateObserverBridge>(self);
    _webState->AddObserver(_webStateObserver.get());
    _webState->GetNavigationManager()->AddTransientURLRewriter(
        &VoiceSearchURLRewriter);
  }
  return self;
}

- (void)dealloc {
  if (_webState) {
    _webState->RemoveObserver(_webStateObserver.get());
    _webStateObserver.reset();
    _webState = nullptr;
  }
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  DCHECK_EQ(_webState, webState);
  const GURL& URL = webState->GetLastCommittedURL();
  if ([_delegate shouldTextToSpeechListener:self parseDataFromURL:URL]) {
    __weak TextToSpeechListener* weakSelf = self;
    ExtractVoiceSearchAudioDataFromWebState(webState, ^(NSData* audioData) {
      [weakSelf.delegate textToSpeechListener:weakSelf
                             didReceiveResult:audioData];
    });
  } else {
    [self.delegate textToSpeechListener:self didReceiveResult:nil];
  }
}

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  if (_webState) {
    _webState->RemoveObserver(_webStateObserver.get());
    _webStateObserver.reset();
    _webState = nullptr;
  }
  [self.delegate textToSpeechListenerWebStateWasDestroyed:self];
}

@end
