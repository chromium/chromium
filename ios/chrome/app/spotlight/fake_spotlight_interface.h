// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_SPOTLIGHT_FAKE_SPOTLIGHT_INTERFACE_H_
#define IOS_CHROME_APP_SPOTLIGHT_FAKE_SPOTLIGHT_INTERFACE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/app/spotlight/spotlight_interface.h"

/// A fake spotlight interface API class.
/// It is designed to be used mainly for testing purposes.
@interface FakeSpotlightInterface : SpotlightInterface

- (instancetype)init;

// Keeps track on how many calls were made to 'indexSearchableItems'
@property(nonatomic, assign) NSUInteger indexSearchableItemsCallsCount;

// Keeps track on how many calls were made to
// 'deleteSearchableItemsWithIdentifiers'
@property(nonatomic, assign)
    NSUInteger deleteSearchableItemsWithIdentifiersCallsCount;

// Keeps track on how many calls were made to
// 'deleteSearchableItemsWithDomainIdentifiers'
@property(nonatomic, assign)
    NSUInteger deleteSearchableItemsWithDomainIdentifiersCallsCount;

// Keeps track on how many calls were made to
// 'deleteAllSearchableItemsWithCompletionHandler'
@property(nonatomic, assign)
    NSUInteger deleteAllSearchableItemsWithCompletionHandlerCallsCount;

@end
#endif  // IOS_CHROME_APP_SPOTLIGHT_FAKE_SPOTLIGHT_INTERFACE_H_
