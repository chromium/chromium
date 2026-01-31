// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_MAGIC_STACK_UI_MAGIC_STACK_LAYOUT_CONFIGURATOR_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_MAGIC_STACK_UI_MAGIC_STACK_LAYOUT_CONFIGURATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/content_suggestions/magic_stack/ui/magic_stack_collection_view.h"

// Manages the Magic Stack's UICollectionViewCompositionalLayout.
@interface MagicStackLayoutConfigurator : NSObject

@property(weak, nonatomic) MagicStackDiffableDataSource* dataSource;

// Returns the managed UICollectionViewCompositionalLayout.
- (UICollectionViewCompositionalLayout*)magicStackCompositionalLayout;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_MAGIC_STACK_UI_MAGIC_STACK_LAYOUT_CONFIGURATOR_H_
