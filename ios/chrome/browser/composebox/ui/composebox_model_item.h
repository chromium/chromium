// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_MODEL_ITEM_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_MODEL_ITEM_H_

#import <UIKit/UIKit.h>

#import "third_party/omnibox_proto/model_mode.pb.h"

// A read-only, displayable model item of the composebox model picker. Items
// are built by the input state manager and rebuilt on every input state
// change. The UI must treat `modelMode` as an opaque identifier: it should only
// render the item and pass it back when the item is picked.
@interface ComposeboxModelItem : NSObject

// The server model this item represents.
@property(nonatomic, readonly) omnibox::ModelMode modelMode;
// The localized label of the model.
@property(nonatomic, readonly, copy) NSString* title;
// The icon of the model.
@property(nonatomic, readonly, strong) UIImage* icon;
// Whether the model is the active one.
@property(nonatomic, readonly, getter=isSelected) BOOL selected;
// Whether the model is allowed but cannot be picked in the current state.
@property(nonatomic, readonly, getter=isDisabled) BOOL disabled;

// Initializes a model item with the given `modelMode`, display `title`,
// display `icon`, active selection state (`selected`), and availability
// (`disabled`).
- (instancetype)initWithModelMode:(omnibox::ModelMode)modelMode
                            title:(NSString*)title
                             icon:(UIImage*)icon
                         selected:(BOOL)selected
                         disabled:(BOOL)disabled NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_MODEL_ITEM_H_
