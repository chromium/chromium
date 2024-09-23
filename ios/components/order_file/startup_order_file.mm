// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import "ios/components/order_file/save_order_file.h"

#define IOS_ORDER_FILE_STARTUP_END_DELAY_MS 4000

static NSString* const kOrderFileError = @"OrderFileGenerationError";

@interface CRWStartupOrderFile : NSObject
@end

@implementation CRWStartupOrderFile

+ (void)load {
  // `load` methods are called early in startup (immediately post-main).
  // Installing the observer here should always be completed before the relevant
  // launch signal is reached.
  [self installRunLoopObserver];
}

+ (void)saveOrderFile {
  int64_t saveDelayTimeNs =
      (int64_t)IOS_ORDER_FILE_STARTUP_END_DELAY_MS * NSEC_PER_MSEC;
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, saveDelayTimeNs),
                 dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
                   CRWSaveOrderFile();
                 });
}

+ (void)installRunLoopObserver {
  // Maximum priority means this observer will fire after all other similar
  // observers enqueued.
  CFRunLoopObserverRef runLoopObserver = CFRunLoopObserverCreateWithHandler(
      NULL, kCFRunLoopBeforeWaiting, NO, LONG_MAX,
      ^(CFRunLoopObserverRef observer, CFRunLoopActivity activity) {
        [CRWStartupOrderFile saveOrderFile];
        CFRunLoopRemoveObserver(CFRunLoopGetMain(), observer,
                                kCFRunLoopDefaultMode);
        CFRelease(observer);
      });
  CFRunLoopAddObserver(CFRunLoopGetMain(), runLoopObserver,
                       kCFRunLoopDefaultMode);
}

@end
