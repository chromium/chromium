// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/deprecated/crw_js_injection_manager.h"

#import <UIKit/UIKit.h>

#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/web/js_messaging/page_script_util.h"
#import "ios/web/public/deprecated/crw_js_injection_receiver.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation CRWJSInjectionManager {
  // JS to inject into the page. This may be nil if it has been purged due to
  // low memory.
  NSString* _injectObject;
  // An object the can receive JavaScript injection.
  __weak CRWJSInjectionReceiver* _receiver;
}

- (id)initWithReceiver:(CRWJSInjectionReceiver*)receiver {
  DCHECK(receiver);
  self = [super init];
  if (self) {
    _receiver = receiver;
    // Register for low-memory warnings.
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(lowMemoryWarning:)
               name:UIApplicationDidReceiveMemoryWarningNotification
             object:nil];
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (BOOL)hasBeenInjected {
  return [_receiver scriptHasBeenInjectedForClass:[self class]];
}

- (void)inject {
  if ([self hasBeenInjected])
    return;
  [_receiver injectScript:[self injectionContent] forClass:[self class]];
  DCHECK([self hasBeenInjected]);
}

- (void)lowMemoryWarning:(NSNotification*)notify {
  _injectObject = nil;
}

- (void)executeJavaScript:(NSString*)script
        completionHandler:(void (^)(id, NSError*))completionHandler {
  [_receiver executeJavaScript:script completionHandler:completionHandler];
}

#pragma mark -
#pragma mark ProtectedMethods

- (CRWJSInjectionReceiver*)receiver {
  return _receiver;
}

- (NSString*)scriptPath {
  NOTREACHED();
  return nil;
}

- (NSString*)injectionContent {
  if (!_injectObject)
    _injectObject = [[self staticInjectionContent] copy];
  return _injectObject;
}

- (NSString*)staticInjectionContent {
  return web::GetPageScript([self scriptPath]);
}

@end
