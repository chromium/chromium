// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/dialogs/completion_block_util.h"

#include "base/logging.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using completion_block_util::AlertCallback;
using completion_block_util::ConfirmCallback;
using completion_block_util::PromptCallback;
using completion_block_util::HTTPAuthCallack;
using completion_block_util::DecidePolicyCallback;

#pragma mark - WebCompletionWrapper

// Superclass for wrapper objects through which completion blocks from web//
// are executed.  The blocks passed on creation will be called before the
// wrapper is deallocated in order to prevent exceptions from being thrown.
// TODO(crbug.com/918189): Convert to C++ callback checker object when C++14
// lambda expressions are allowed by the Chromium style guide.
@interface WebCompletionWrapper : NSObject

// Whether the completion block has been executed.
@property(nonatomic, assign, getter=isExecuted) BOOL executed;

// Executes the wrapped completion block for cancelled dialogs.
- (void)executeCompletionForCancellation;

@end

@implementation WebCompletionWrapper

- (void)dealloc {
  if (!self.executed)
    [self executeCompletionForCancellation];
}

- (void)executeCompletionForCancellation {
  NOTREACHED() << "Subclasses must implement.";
}

@end

#pragma mark - JavaScriptAlertCompletionWrapper

// Completion wrapper for JavaScript alert completions.
@interface JavaScriptAlertCompletionWrapper : WebCompletionWrapper {
  AlertCallback _callback;
}

// Initializer that takes a JavaScript dialog callback.
- (instancetype)initWithCallback:(AlertCallback)callback
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Executes |_callback|.
- (void)executeCallback;

@end

@implementation JavaScriptAlertCompletionWrapper

- (instancetype)initWithCallback:(AlertCallback)callback {
  if (self = [super init]) {
    _callback = callback;
  }
  return self;
}

- (void)executeCompletionForCancellation {
  [self executeCallback];
}

- (void)executeCallback {
  if (self.executed || !_callback)
    return;
  _callback();
  self.executed = YES;
}

@end

#pragma mark - JavaScriptConfirmCompletionWrapper

// Completion wrapper for JavaScript confirmation completions.
@interface JavaScriptConfirmCompletionWrapper : WebCompletionWrapper {
  ConfirmCallback _callback;
}

// Initializer that takes a JavaScript dialog callback.
- (instancetype)initWithCallback:(ConfirmCallback)callback
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Executes |_callback| with |confirmed|.
- (void)executeCallbackWithConfirmation:(BOOL)confirmed;

@end

@implementation JavaScriptConfirmCompletionWrapper

- (instancetype)initWithCallback:(ConfirmCallback)callback {
  if (self = [super init]) {
    _callback = callback;
  }
  return self;
}

- (void)executeCompletionForCancellation {
  [self executeCallbackWithConfirmation:NO];
}

- (void)executeCallbackWithConfirmation:(BOOL)confirmed {
  if (self.executed || !_callback)
    return;
  _callback(confirmed);
  self.executed = YES;
}

@end

#pragma mark - JavaScriptPromptCompletionWrapper

// Completion wrapper for JavaScript prompt completions.
@interface JavaScriptPromptCompletionWrapper : WebCompletionWrapper {
  PromptCallback _callback;
}

// Initializer that takes a JavaScript dialog callback.
- (instancetype)initWithCallback:(PromptCallback)callback
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Executes |_callback| with |input|.
- (void)executeCallbackWithInput:(NSString*)input;

@end

@implementation JavaScriptPromptCompletionWrapper

- (instancetype)initWithCallback:(PromptCallback)callback {
  if (self = [super init]) {
    _callback = callback;
  }
  return self;
}

- (void)executeCompletionForCancellation {
  [self executeCallbackWithInput:nil];
}

- (void)executeCallbackWithInput:(NSString*)input {
  if (self.executed || !_callback)
    return;
  _callback(input);
  self.executed = YES;
}

@end

#pragma mark - HTTPAuthCompletionWrapper

// Web completion wrapper for HTTP authentication dialog completions.
@interface HTTPAuthCompletionWrapper : WebCompletionWrapper {
  HTTPAuthCallack _callback;
}

// Initializer that takes an HTTP authentication dialog callback.
- (instancetype)initWithCallback:(HTTPAuthCallack)callback
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Executes |_callback| with |user| and |password|.
- (void)executeCallbackWithUser:(NSString*)user password:(NSString*)password;

@end

@implementation HTTPAuthCompletionWrapper

- (instancetype)initWithCallback:(HTTPAuthCallack)callback {
  if (self = [super init]) {
    _callback = callback;
  }
  return self;
}

- (void)executeCompletionForCancellation {
  [self executeCallbackWithUser:nil password:nil];
}

- (void)executeCallbackWithUser:(NSString*)user password:(NSString*)password {
  if (self.executed || !_callback)
    return;
  _callback(user, password);
  self.executed = YES;
}

@end

#pragma mark - DecidePolicyCompletionWrapper

// Completion wrapper for decide policy completions.
@interface DecidePolicyCompletionWrapper : WebCompletionWrapper {
  DecidePolicyCallback _callback;
}

// Initializer that takes a JavaScript dialog callback.
- (instancetype)initWithCallback:(DecidePolicyCallback)callback
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Executes |_callback| with |shouldContinue|.
- (void)executeCallbackToConinue:(BOOL)shouldContinue;

@end

@implementation DecidePolicyCompletionWrapper

- (instancetype)initWithCallback:(DecidePolicyCallback)callback {
  if (self = [super init]) {
    _callback = callback;
  }
  return self;
}

- (void)executeCompletionForCancellation {
  [self executeCallbackToConinue:NO];
}

- (void)executeCallbackToConinue:(BOOL)shouldContinue {
  if (self.executed || !_callback)
    return;
  _callback(shouldContinue);
  self.executed = YES;
}

@end

namespace completion_block_util {

AlertCallback GetSafeJavaScriptAlertCompletion(AlertCallback callback) {
  JavaScriptAlertCompletionWrapper* wrapper =
      [[JavaScriptAlertCompletionWrapper alloc] initWithCallback:callback];
  return ^{
    [wrapper executeCallback];
  };
}

ConfirmCallback GetSafeJavaScriptConfirmationCompletion(
    ConfirmCallback callback) {
  JavaScriptConfirmCompletionWrapper* wrapper =
      [[JavaScriptConfirmCompletionWrapper alloc] initWithCallback:callback];
  return ^(BOOL is_confirmed) {
    [wrapper executeCallbackWithConfirmation:is_confirmed];
  };
}

PromptCallback GetSafeJavaScriptPromptCompletion(PromptCallback callback) {
  JavaScriptPromptCompletionWrapper* wrapper =
      [[JavaScriptPromptCompletionWrapper alloc] initWithCallback:callback];
  return ^(NSString* text_input) {
    [wrapper executeCallbackWithInput:text_input];
  };
}

HTTPAuthCallack GetSafeHTTPAuthCompletion(HTTPAuthCallack callback) {
  HTTPAuthCompletionWrapper* wrapper =
      [[HTTPAuthCompletionWrapper alloc] initWithCallback:callback];
  return ^(NSString* user, NSString* password) {
    [wrapper executeCallbackWithUser:user password:password];
  };
}

DecidePolicyCallback GetSafeDecidePolicyCompletion(
    DecidePolicyCallback callback) {
  DecidePolicyCompletionWrapper* wrapper =
      [[DecidePolicyCompletionWrapper alloc] initWithCallback:callback];
  return ^(BOOL shouldContinue) {
    [wrapper executeCallbackToConinue:shouldContinue];
  };
}

}  // completion_block_util
