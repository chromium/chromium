// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/spotlight/spotlight_logger.h"

#import "ios/chrome/browser/flags/system_flags.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

@end
