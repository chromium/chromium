// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MAGIC_STACK_MAGIC_STACK_MODULE_COLLECTION_VIEW_CELL_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MAGIC_STACK_MAGIC_STACK_MODULE_COLLECTION_VIEW_CELL_H_

#import <UIKit/UIKit.h>

enum class ContentSuggestionsModuleType;
@protocol MagicStackModuleContainerDelegate;
@class MagicStackModule;

// Cell for a module in the Magic Stack.
@interface MagicStackModuleCollectionViewCell : UICollectionViewCell

- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Configures this container with `config`.
- (void)configureWithConfig:(MagicStackModule*)config;

// Delegate for this container.
@property(nonatomic, weak) id<MagicStackModuleContainerDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MAGIC_STACK_MAGIC_STACK_MODULE_COLLECTION_VIEW_CELL_H_
