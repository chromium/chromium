// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_MULTI_ROW_MODULE_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_MULTI_ROW_MODULE_H_

#import "ios/chrome/browser/ui/content_suggestions/cells/magic_stack_module_container.h"

// Module implementation intended to display multiple rows of content elements.
@interface MultiRowModule : MagicStackModuleContainer

// Initializes and configures this view to contain each element in `views` in a
// row and configure the module for `type`. Note that there is a height limit
// defined in MagicStackModuleContainer. It is the job of the implementor to
// ensure `views` are laid out with that in mind.
- (instancetype)initWithViews:(NSArray<UIView*>*)views
                         type:(ContentSuggestionsModuleType)type;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_MULTI_ROW_MODULE_H_
