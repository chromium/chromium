// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_MAGIC_STACK_UI_MAGIC_STACK_COLLECTION_VIEW_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_MAGIC_STACK_UI_MAGIC_STACK_COLLECTION_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/content_suggestions/magic_stack/ui/magic_stack_consumer.h"

typedef UICollectionViewDiffableDataSource<NSString*, MagicStackModule*>
    MagicStackDiffableDataSource;

// Layout types supported by MagicStackCollectionViewController.
enum class MagicStackLayoutType {
  // Classic compositional layout for standard NTP.
  kClassic,
  // Redesign Smart Stack layout with 3D roll/stack transition.
  kSmartStack,
};

@protocol MagicStackCollectionViewControllerAudience;
@protocol MagicStackModuleContainerDelegate;

// A UICollectionView that contains a horizontal list of Magic Stack cards.
@interface MagicStackCollectionViewController
    : UIViewController <MagicStackConsumer>

// Whether the Magic Stack should show the edit button at the end of the stack.
@property(nonatomic, assign) BOOL showsEditButton;

// Audience for Magic Stack module events.
@property(nonatomic, weak) id<MagicStackCollectionViewControllerAudience,
                              MagicStackModuleContainerDelegate>
    audience;

// Initializes the collection view controller with the default classic layout.
- (instancetype)init;

// Initializes the collection view controller with the specified layout type.
- (instancetype)initWithLayoutType:(MagicStackLayoutType)layoutType
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

// Called when the module width has changed.
- (void)moduleWidthDidUpdate;


// Resets the Magic Stack.
- (void)reset;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_MAGIC_STACK_UI_MAGIC_STACK_COLLECTION_VIEW_H_
