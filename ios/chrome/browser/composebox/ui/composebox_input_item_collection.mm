// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/ui/composebox_input_item_collection.h"

#import "base/check.h"
#import "base/sequence_checker.h"
#import "base/unguessable_token.h"
#import "ios/chrome/browser/composebox/ui/composebox_input_item.h"

@implementation ComposeboxInputItemCollection {
  // Check that the different methods are called from the correct sequence, as
  // this class defers work via PostTask APIs.
  SEQUENCE_CHECKER(_sequenceChecker);

  // The ordered list of items for display.
  NSMutableArray<ComposeboxInputItem*>* _containedItems;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _containedItems = [[NSMutableArray alloc] init];
  }

  return self;
}

#pragma mark - Public properties

- (size_t)count {
  return _containedItems.count;
}

- (BOOL)isEmpty {
  return self.count == 0;
}

- (BOOL)hasImage {
  return self.imagesCount > 0;
}

- (BOOL)hasTabOrFile {
  return self.tabsCount > 0 || self.filesCount > 0;
}

- (size_t)nonTabAttachmentCount {
  return self.imagesCount + self.filesCount;
}

- (size_t)imagesCount {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  NSUInteger result = 0;
  for (ComposeboxInputItem* item in _containedItems) {
    if (item.type == ComposeboxInputItemType::kComposeboxInputItemTypeImage) {
      result++;
    }
  }
  return result;
}

- (size_t)tabsCount {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  NSUInteger result = 0;
  for (ComposeboxInputItem* item in _containedItems) {
    if (item.type == ComposeboxInputItemType::kComposeboxInputItemTypeFile) {
      result++;
    }
  }
  return result;
}

- (size_t)filesCount {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  NSUInteger result = 0;
  for (ComposeboxInputItem* item in _containedItems) {
    if (item.type == ComposeboxInputItemType::kComposeboxInputItemTypeTab) {
      result++;
    }
  }
  return result;
}

- (ComposeboxInputItem*)firstItem {
  if (self.empty) {
    return nil;
  }

  return _containedItems[0];
}

#pragma mark - Public methods

- (void)addItem:(ComposeboxInputItem*)item {
  [_containedItems addObject:item];
  [_delegate composeboxInputItemCollectionDidUpdateItems:self];
}

- (void)replaceWithItems:(NSArray<ComposeboxInputItem*>*)updatedItems {
  _containedItems = [[NSMutableArray alloc] initWithArray:updatedItems
                                                copyItems:YES];
  [_delegate composeboxInputItemCollectionDidUpdateItems:self];
}

- (void)removeItem:(ComposeboxInputItem*)item {
  [_containedItems removeObject:item];
  [_delegate composeboxInputItemCollectionDidUpdateItems:self];
}

- (void)clearItems {
  [_containedItems removeAllObjects];
  [_delegate composeboxInputItemCollectionDidUpdateItems:self];
}

- (ComposeboxInputItem*)itemForIdentifier:(base::UnguessableToken)identifier {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  for (ComposeboxInputItem* item in _containedItems) {
    if (item.identifier == identifier) {
      return item;
    }
  }
  return nil;
}

- (ComposeboxInputItem*)itemForServerToken:(base::UnguessableToken)serverToken {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  for (ComposeboxInputItem* item in _containedItems) {
    if (item.serverToken == serverToken) {
      return item;
    }
  }
  return nil;
}

- (BOOL)assetAlreadyLoaded:(NSString*)assetID {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);

  if (!assetID) {
    return NO;
  }
  for (ComposeboxInputItem* item in _containedItems) {
    if ([item.assetID isEqualToString:assetID]) {
      return YES;
    }
  }

  return NO;
}

@end
