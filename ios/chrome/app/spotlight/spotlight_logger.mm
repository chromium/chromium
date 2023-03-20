// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/spotlight/spotlight_logger.h"

#import <UIKit/UIKit.h>

#import "base/debug/dump_without_crashing.h"
#import "base/metrics/histogram_macros.h"
#import "ios/chrome/browser/flags/system_flags.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

NSString* kSpotlightDebuggerErrorLogKey = @"SpotlightDebuggerErrorLogKey";

}  // namespace

@interface SpotlightLogger ()

@property(nonatomic, strong) NSMutableDictionary* knownItems;

@end

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

- (instancetype)init {
  self = [super init];
  if (self) {
    _knownItems = [[NSMutableDictionary alloc] init];
  }
  return self;
}

- (void)logIndexedItem:(CSSearchableItem*)item {
  self.knownItems[item.uniqueIdentifier] = item;
}

- (void)logIndexedItems:(NSArray<CSSearchableItem*>*)items {
  for (CSSearchableItem* item in items) {
    [self logIndexedItem:item];
  }
}

- (void)logDeletionOfItemsWithIdentifiers:(NSArray<NSString*>*)identifiers {
  for (NSString* identifier in identifiers) {
    self.knownItems[identifier] = nil;
  }
}

- (void)logDeletionOfItemsInDomain:(NSString*)domain {
  for (NSString* key in self.knownItems.allKeys) {
    CSSearchableItem* item = self.knownItems[key];
    if ([item.domainIdentifier isEqualToString:domain]) {
      self.knownItems[key] = nil;
    }
  }
}

- (void)logDeletionOfItemsInDomains:(NSArray<NSString*>*)domains {
  for (NSString* domain in domains) {
    [self logDeletionOfItemsInDomain:domain];
  }
}

- (void)logDeletionOfAllItems {
  [self.knownItems removeAllObjects];
}

- (NSArray*)knownIndexedItems {
  return self.knownItems.allValues;
}

- (NSArray*)knownIndexedItemsInDomain:(NSString*)domain {
  NSMutableArray* items = [[NSMutableArray alloc] init];
  for (NSString* key in self.knownItems.allKeys) {
    CSSearchableItem* item = self.knownItems[key];
    if ([item.domainIdentifier isEqualToString:domain]) {
      [items addObject:item];
    }
  }
  return items;
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
  if ([self sharedLogger]) {
    [[self sharedLogger] logSpotlightError:error];
  } else {
    // Dump as much info from the wild as we can about the error.
    base::debug::DumpWithoutCrashing();
    UMA_HISTOGRAM_SPARSE("IOSSpotlightErrorCode", error.code);
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
