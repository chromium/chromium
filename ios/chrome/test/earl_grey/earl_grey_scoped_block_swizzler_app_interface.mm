// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/earl_grey_scoped_block_swizzler_app_interface.h"

#import <map>

#import "base/check.h"
#import "ios/testing/scoped_block_swizzler.h"

@interface EarlGreyScopedBlockSwizzlerHelper : NSObject {
  // Unique IDs used with an EG2 safe basic type that can be used to later
  // delete the ScopedBlockSwizzler.
  int _swizzledIDs;

  // Map of ScopedBlockSwizzler-ed objects, with a tracking int.
  std::map<int, std::unique_ptr<ScopedBlockSwizzler>> _map;
}

// Inserts and removes from `map`.
- (int)insertScopedBlockSwizzler:(std::unique_ptr<ScopedBlockSwizzler>)swizzler;
- (void)removeScopedBlockSwizzler:(int)uniqueID;
@end

@implementation EarlGreyScopedBlockSwizzlerHelper

- (instancetype)init {
  if ((self = [super init])) {
    _swizzledIDs = 0;
  }
  return self;
}

+ (instancetype)sharedInstance {
  static EarlGreyScopedBlockSwizzlerHelper* instance;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    instance = [[EarlGreyScopedBlockSwizzlerHelper alloc] init];
  });
  return instance;
}

- (int)insertScopedBlockSwizzler:
    (std::unique_ptr<ScopedBlockSwizzler>)swizzler {
  _map[++_swizzledIDs] = std::move(swizzler);
  return _swizzledIDs;
}

- (void)removeScopedBlockSwizzler:(int)uniqueID {
  DCHECK(_map[uniqueID]);
  _map.erase(uniqueID);
}

@end

@implementation EarlGreyScopedBlockSwizzlerAppInterface

+ (int)createScopedBlockSwizzlerForTarget:(NSString*)targetString
                             withSelector:(NSString*)selectorString
                                withBlock:(id)block {
  Class target = NSClassFromString(targetString);
  SEL selector = NSSelectorFromString(selectorString);
  EarlGreyScopedBlockSwizzlerHelper* helper =
      [EarlGreyScopedBlockSwizzlerHelper sharedInstance];
  auto swizzler =
      std::make_unique<ScopedBlockSwizzler>(target, selector, block);
  return [helper insertScopedBlockSwizzler:std::move(swizzler)];
}

+ (void)deleteScopedBlockSwizzlerForID:(int)uniqueID {
  EarlGreyScopedBlockSwizzlerHelper* helper =
      [EarlGreyScopedBlockSwizzlerHelper sharedInstance];
  [helper removeScopedBlockSwizzler:uniqueID];
}

@end
