// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_search_item.h"

@implementation AtMemorySearchItem

- (BOOL)isEqual:(id)object {
  if (self == object) {
    return YES;
  }
  if (![object isKindOfClass:[AtMemorySearchItem class]]) {
    return NO;
  }
  AtMemorySearchItem* other = (AtMemorySearchItem*)object;
  return [self.text isEqualToString:other.text] &&
         [self.detailText isEqualToString:other.detailText] &&
         (self.itemType == other.itemType ||
          [self.itemType isEqualToString:other.itemType]) &&
         self.loading == other.loading;
}

- (NSUInteger)hash {
  return [self.text hash] ^ [self.detailText hash] ^ [self.itemType hash] ^
         self.loading;
}

@end
