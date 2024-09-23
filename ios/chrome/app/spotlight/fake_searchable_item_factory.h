// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_SPOTLIGHT_FAKE_SEARCHABLE_ITEM_FACTORY_H_
#define IOS_CHROME_APP_SPOTLIGHT_FAKE_SEARCHABLE_ITEM_FACTORY_H_

#import "ios/chrome/app/spotlight/searchable_item_factory.h"
#import "ios/chrome/app/spotlight/spotlight_util.h"

/// A fake factory class for creating searchable item.
/// It is designed to be used mainly for testing purposes.
@interface FakeSearchableItemFactory : SearchableItemFactory

- (instancetype)initWithDomain:(spotlight::Domain)domain;

@property(nonatomic, assign) NSInteger cancelItemsGenerationCallCount;

@end

#endif  // IOS_CHROME_APP_SPOTLIGHT_FAKE_SEARCHABLE_ITEM_FACTORY_H_
