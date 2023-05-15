// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_ACTION_LIST_MODULE_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_ACTION_LIST_MODULE_H_

#import "ios/chrome/browser/ui/content_suggestions/cells/magic_stack_module_container.h"

// Module implementation intended to display four horizontally-laid out content
// elements, but can be used for any module that needs a title above some
// content.
@interface ActionListModule : MagicStackModuleContainer

// Initializes and configures this view to contain `contentView` and configure
// for `type`. If `contentView` is a UIView, it will need to either define it's
// intrinsicContentSize or set its own vertical/horizontal constraints.
- (instancetype)initWithContentView:(UIView*)contentView
                               type:(ContentSuggestionsModuleType)type;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_ACTION_LIST_MODULE_H_
