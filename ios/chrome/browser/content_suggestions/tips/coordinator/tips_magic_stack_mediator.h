// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_TIPS_COORDINATOR_TIPS_MAGIC_STACK_MEDIATOR_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_TIPS_COORDINATOR_TIPS_MAGIC_STACK_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import <memory>

#import "base/ios/block_types.h"

@protocol ContentSuggestionsViewControllerAudience;
class PrefService;
@protocol TipsMagicStackMediatorDelegate;
@class TipsModuleConfig;
namespace bookmarks {
class BookmarkModel;
}  // namespace bookmarks
namespace commerce {
class ShoppingService;
}  // namespace commerce
namespace image_fetcher {
class ImageDataFetcher;
}  // namespace image_fetcher
namespace segmentation_platform {
enum class TipIdentifier;
}  // namespace segmentation_platform

// Mediator for managing the state of the Tips (Magic Stack) module.
@interface TipsMagicStackMediator : NSObject

// Used by the Tips module for the current module configuration.
@property(nonatomic, strong, readonly) TipsModuleConfig* config;

// Delegate.
@property(nonatomic, weak) id<TipsMagicStackMediatorDelegate> delegate;

// Audience for presentation actions.
@property(nonatomic, weak) id<ContentSuggestionsViewControllerAudience>
    presentationAudience;

// Default initializer.
- (instancetype)
    initWithIdentifier:(segmentation_platform::TipIdentifier)identifier
    profilePrefService:(PrefService*)profilePrefService
       shoppingService:(commerce::ShoppingService*)shoppingService
         bookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
          imageFetcher:
              (std::unique_ptr<image_fetcher::ImageDataFetcher>)imageFetcher
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects the mediator.
- (void)disconnect;

// Reconfigures `TipsMagicStackMediator` with a new tip `identifier`.
- (void)reconfigureWithTipIdentifier:
    (segmentation_platform::TipIdentifier)identifier;

// Removes the module from the Magic Stack on the current homepage without
// disabling the underlying feature. This prevents the module from being shown
// on the current homepage but does not affect its functionality elsewhere.
// The `completion` is called after the removal is finished.
- (void)removeModuleWithCompletion:(ProceduralBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_TIPS_COORDINATOR_TIPS_MAGIC_STACK_MEDIATOR_H_
