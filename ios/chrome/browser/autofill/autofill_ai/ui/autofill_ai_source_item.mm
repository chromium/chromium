// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/autofill_ai/ui/autofill_ai_source_item.h"

#import "url/gurl.h"

@implementation AutofillAiSourceItem

#pragma mark - Public

- (instancetype)initWithTitle:(NSString*)title
                     subtitle:(NSString*)subtitle
                          URL:(const GURL&)URL
                         type:(AutofillAiSourceType)type
                         icon:(UIImage*)icon {
  self = [super init];
  if (self) {
    _title = [title copy];
    _subtitle = [subtitle copy];
    _URL = URL;
    _type = type;
    _icon = icon;
  }
  return self;
}

@end

@implementation AutofillAiSourceGroup

#pragma mark - Public

- (instancetype)initWithTitle:(NSString*)title
                        items:(NSArray<AutofillAiSourceItem*>*)items {
  self = [super init];
  if (self) {
    _title = [title copy];
    _items = [items copy];
  }
  return self;
}

@end
