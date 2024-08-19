// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MAGIC_STACK_MAGIC_STACK_COLLECTION_VIEW_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MAGIC_STACK_MAGIC_STACK_COLLECTION_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_consumer.h"

typedef UICollectionViewDiffableDataSource<NSString*, MagicStackModule*>
    MagicStackDiffableDataSource;

@protocol MagicStackCollectionViewControllerAudience;
@protocol MagicStackModuleContainerDelegate;

// A UICollectionView that contains a horizontal list of Magic Stack cards.
@interface MagicStackCollectionViewController
    : UIViewController <MagicStackConsumer>

// Audience for Magic Stack module events.
@property(nonatomic, weak) id<MagicStackCollectionViewControllerAudience,
                              MagicStackModuleContainerDelegate>
    audience;

// Called when the module width has changed.
- (void)moduleWidthDidUpdate;

// Resets the Magic Stack.
- (void)reset;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MAGIC_STACK_MAGIC_STACK_COLLECTION_VIEW_H_
