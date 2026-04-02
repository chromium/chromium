// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_item.h"

@implementation AutofillAIEntityEditItem

@synthesize attributeType = _attributeType;
@synthesize hasValidValueStatus = _hasValidValueStatus;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    _hasValidValueStatus = YES;
  }
  return self;
}

- (void)setHasValidValueStatus:(BOOL)hasValidValueStatus {
  _hasValidValueStatus = hasValidValueStatus;
  [self setHasValidText:hasValidValueStatus];
}

@end
