// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MAGIC_STACK_MAGIC_STACK_EDIT_BUTTON_CELL_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MAGIC_STACK_MAGIC_STACK_EDIT_BUTTON_CELL_H_

#import <UIKit/UIKit.h>

@protocol MagicStackCollectionViewControllerAudience;

// Edit button cell in the Magic Stack.
@interface MagicStackEditButtonCell : UICollectionViewCell

// Audience for tap events.
@property(nonatomic, weak) id<MagicStackCollectionViewControllerAudience>
    audience;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MAGIC_STACK_MAGIC_STACK_EDIT_BUTTON_CELL_H_
