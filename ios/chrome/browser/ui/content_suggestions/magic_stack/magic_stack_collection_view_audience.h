// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MAGIC_STACK_MAGIC_STACK_COLLECTION_VIEW_AUDIENCE_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MAGIC_STACK_MAGIC_STACK_COLLECTION_VIEW_AUDIENCE_H_

#import <UIKit/UIKit.h>

// Audience for Magic Stack module events.
@protocol MagicStackCollectionViewControllerAudience

// Notifies the audience that the Magic Stack edit button was tapped.
- (void)didTapMagicStackEditButton;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MAGIC_STACK_MAGIC_STACK_COLLECTION_VIEW_AUDIENCE_H_
