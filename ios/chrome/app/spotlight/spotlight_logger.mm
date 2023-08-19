// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/spotlight/spotlight_logger.h"

#import <UIKit/UIKit.h>

#import "base/debug/dump_without_crashing.h"
#import "base/metrics/histogram_macros.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"

namespace {

NSString* kSpotlightDebuggerErrorLogKey = @"SpotlightDebuggerErrorLogKey";

}  // namespace

@implementation SpotlightLogger

+ (instancetype)sharedLogger {
  // Read this flag once; if it changes while the app is running, don't start
  // logging.
  static BOOL debuggingEnabled =
      experimental_flags::IsSpotlightDebuggingEnabled();
  if (!debuggingEnabled) {
    return nil;
  }

  static SpotlightLogger* sharedLogger = [[SpotlightLogger alloc] init];
  return sharedLogger;
}

- (void)logSpotlightError:(NSError*)error {
  NSArray* errorLog = [[NSUserDefaults standardUserDefaults]
      objectForKey:kSpotlightDebuggerErrorLogKey];

  NSMutableArray* mutableErrorLog = [[NSMutableArray alloc] init];
  if (errorLog) {
    [mutableErrorLog addObjectsFromArray:errorLog];
  }

  [[NSUserDefaults standardUserDefaults]
      setObject:mutableErrorLog
         forKey:kSpotlightDebuggerErrorLogKey];

  [self showAlertImmediately:error.localizedDescription];
}

+ (void)logSpotlightError:(NSError*)error {
  UMA_HISTOGRAM_SPARSE("IOSSpotlightErrorCode", error.code);
  if (error) {
    [[self sharedLogger] logSpotlightError:error];
  }
}

#pragma mark - internal

- (void)showAlertImmediately:(NSString*)errorMessage {
  UIAlertController* alert =
      [UIAlertController alertControllerWithTitle:@"Spotlight Error"
                                          message:errorMessage
                                   preferredStyle:UIAlertControllerStyleAlert];
  [alert addAction:[UIAlertAction actionWithTitle:@"OK"
                                            style:UIAlertActionStyleDefault
                                          handler:nil]];
  UIWindowScene* scene = (UIWindowScene*)
      [UIApplication.sharedApplication.connectedScenes anyObject];

  [scene.windows[0].rootViewController presentViewController:alert
                                                    animated:YES
                                                  completion:nil];
}

@end
