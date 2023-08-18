// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"

#import <objc/runtime.h>

#import <ostream>
#import <unordered_map>
#import <vector>

#import "base/check.h"
#import "base/containers/contains.h"
#import "base/strings/sys_string_conversions.h"

#pragma mark - SilentlyFailingObject

// Object that responds to any selector and does nothing when its called.
// Used as a "nice OCMock" equivalent for CommandDispatcher that's preparing for
// shutdown.
@interface SilentlyFailingObject : NSProxy
@end
@implementation SilentlyFailingObject

- (instancetype)init {
  return self;
}

- (void)forwardInvocation:(NSInvocation*)invocation {
}

- (NSMethodSignature*)methodSignatureForSelector:(SEL)selector {
  // Return some method signature to silence errors.
  // Here it's (void)(self, _cmd).
  return [NSMethodSignature signatureWithObjCTypes:"v@:"];
}

@end

#pragma mark - CommandDispatcher

@implementation CommandDispatcher {
  // Stores which target to forward to for a given selector.
  std::unordered_map<SEL, __weak id> _forwardingTargets;

  // Stores remembered targets while preparing for shutdown.
  std::unordered_map<SEL, __weak id> _silentlyFailingTargets;

  // Tracks if preparing for shutdown has been requested.
  // This is an ivar, not a property, to avoid having synthesized getter/setter
  // methods.
  BOOL _preparingForShutdown;
}

- (void)startDispatchingToTarget:(id)target forSelector:(SEL)selector {
  DCHECK(![self targetForSelector:selector]);
  DCHECK(![self shouldFailSilentlyForSelector:selector]);

  _forwardingTargets[selector] = target;
}

- (void)startDispatchingToTarget:(id)target forProtocol:(Protocol*)protocol {
  unsigned int methodCount;
  objc_method_description* requiredInstanceMethods =
      protocol_copyMethodDescriptionList(protocol, YES /* isRequiredMethod */,
                                         YES /* isInstanceMethod */,
                                         &methodCount);
  for (unsigned int i = 0; i < methodCount; i++) {
    [self startDispatchingToTarget:target
                       forSelector:requiredInstanceMethods[i].name];
  }
  free(requiredInstanceMethods);
}

- (void)stopDispatchingForSelector:(SEL)selector {
  if (_preparingForShutdown) {
    id target = _forwardingTargets[selector];
    _silentlyFailingTargets[selector] = target;
  }
  _forwardingTargets.erase(selector);
}

- (void)stopDispatchingForProtocol:(Protocol*)protocol {
  unsigned int methodCount;
  objc_method_description* requiredInstanceMethods =
      protocol_copyMethodDescriptionList(protocol, YES /* isRequiredMethod */,
                                         YES /* isInstanceMethod */,
                                         &methodCount);
  for (unsigned int i = 0; i < methodCount; i++) {
    [self stopDispatchingForSelector:requiredInstanceMethods[i].name];
  }
  free(requiredInstanceMethods);
}

// `-stopDispatchingToTarget` should be called much less often than
// `-forwardingTargetForSelector`, so removal is intentionally O(n) in order
// to prioritize the speed of lookups.
- (void)stopDispatchingToTarget:(id)target {
  std::vector<SEL> selectorsToErase;
  for (auto& kv : _forwardingTargets) {
    if (kv.second == target) {
      selectorsToErase.push_back(kv.first);
    }
  }

  for (auto* selector : selectorsToErase) {
    [self stopDispatchingForSelector:selector];
  }
}

- (BOOL)dispatchingForProtocol:(Protocol*)protocol {
  // Special-case the NSObject protocol.
  if ([@"NSObject" isEqualToString:NSStringFromProtocol(protocol)]) {
    return YES;
  }

  unsigned int methodCount;
  objc_method_description* requiredInstanceMethods =
      protocol_copyMethodDescriptionList(protocol, YES /* isRequiredMethod */,
                                         YES /* isInstanceMethod */,
                                         &methodCount);
  BOOL conforming = YES;
  for (unsigned int i = 0; i < methodCount; i++) {
    SEL selector = requiredInstanceMethods[i].name;
    BOOL targetFound = base::Contains(_forwardingTargets, selector);
    if (!targetFound && ![self shouldFailSilentlyForSelector:selector]) {
      conforming = NO;
      break;
    }
  }
  free(requiredInstanceMethods);
  if (!conforming) {
    return NO;
  }

  unsigned int protocolCount;
  Protocol* __unsafe_unretained _Nonnull* _Nullable conformedProtocols =
      protocol_copyProtocolList(protocol, &protocolCount);
  for (unsigned int i = 0; i < protocolCount; i++) {
    if (![self dispatchingForProtocol:conformedProtocols[i]]) {
      conforming = NO;
      break;
    }
  }

  free(conformedProtocols);
  return conforming;
}

- (CommandDispatcher*)strictCallableForProtocol:(Protocol*)protocol {
  CHECK([self dispatchingForProtocol:protocol])
      << "Dispatcher failed protocol conformance";
  return self;
}

- (void)prepareForShutdown {
  _preparingForShutdown = YES;
}

#pragma mark - NSObject

// Overridden to forward messages to registered handlers.
- (id)forwardingTargetForSelector:(SEL)selector {
  id target = [self targetForSelector:selector];
  if (target) {
    return target;
  }

  return [super forwardingTargetForSelector:selector];
}

// Overriden to return YES for any registered method.
- (BOOL)respondsToSelector:(SEL)selector {
  if ([self targetForSelector:selector]) {
    return YES;
  }
  return [super respondsToSelector:selector];
}

// Overriden because overrides of `forwardInvocation` also require an override
// of `methodSignatureForSelector`, as the method signature is needed to
// construct NSInvocations.
- (NSMethodSignature*)methodSignatureForSelector:(SEL)aSelector {
  NSMethodSignature* signature = [super methodSignatureForSelector:aSelector];
  if (signature) {
    return signature;
  }

  id target = [self targetForSelector:aSelector];
  return [target methodSignatureForSelector:aSelector];
}

#pragma mark - Private

// Returns the target registered to receive messeages for `selector`.
- (id)targetForSelector:(SEL)selector {
  auto target = _forwardingTargets.find(selector);
  if (target == _forwardingTargets.end()) {
    if ([self shouldFailSilentlyForSelector:selector]) {
      return [[SilentlyFailingObject alloc] init];
    }
    return nil;
  }
  return target->second;
}

- (BOOL)shouldFailSilentlyForSelector:(SEL)selector {
  return _preparingForShutdown &&
         base::Contains(_silentlyFailingTargets, selector);
}

@end
