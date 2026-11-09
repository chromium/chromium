// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "ios/public/provider/chrome/browser/google_one/google_one_api.h"

@interface GlobalTestGoogleOneControllerFactory
    : NSObject <GoogleOneControllerFactory>
@end

@implementation GlobalTestGoogleOneControllerFactory
- (id<GoogleOneController>)createControllerWithConfiguration:
    (GoogleOneConfiguration*)configuration {
  return nil;
}
- (BOOL)canHandleURL:(NSURL*)url {
  return [[url host] isEqualToString:@"one.google.com"];
}
- (NSString*)emailFromURL:(NSURL*)url {
  NSURLComponents* components = [NSURLComponents componentsWithURL:url
                                           resolvingAgainstBaseURL:NO];
  for (NSURLQueryItem* item in components.queryItems) {
    if ([item.name isEqualToString:@"email"]) {
      return item.value;
    }
  }
  return nil;
}
@end

namespace {
id<GoogleOneControllerFactory> g_google_one_controller_factory =
    [[GlobalTestGoogleOneControllerFactory alloc] init];
}

@implementation GoogleOneConfiguration

@end

namespace ios {
namespace provider {

id<GoogleOneController> CreateGoogleOneController(
    GoogleOneConfiguration* configuration) {
  return [g_google_one_controller_factory
      createControllerWithConfiguration:configuration];
}

void SetGoogleOneControllerFactory(id<GoogleOneControllerFactory> factory) {
  if (factory) {
    g_google_one_controller_factory = factory;
  } else {
    g_google_one_controller_factory =
        [[GlobalTestGoogleOneControllerFactory alloc] init];
  }
}

BOOL CanHandleGoogleOneURL(NSURL* url) {
  if ([g_google_one_controller_factory
          respondsToSelector:@selector(canHandleURL:)]) {
    return [g_google_one_controller_factory canHandleURL:url];
  }
  return NO;
}

NSString* GoogleOneEmailFromURL(NSURL* url) {
  if ([g_google_one_controller_factory
          respondsToSelector:@selector(emailFromURL:)]) {
    return [g_google_one_controller_factory emailFromURL:url];
  }
  return nil;
}

}  // namespace provider
}  // namespace ios
