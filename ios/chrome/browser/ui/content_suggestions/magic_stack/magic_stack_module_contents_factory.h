// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MAGIC_STACK_MAGIC_STACK_MODULE_CONTENTS_FACTORY_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MAGIC_STACK_MAGIC_STACK_MODULE_CONTENTS_FACTORY_H_

#import <UIKit/UIKit.h>

@class MagicStackModule;
@class MostVisitedTilesConfig;

// Factory for the content views in a Magic Stack module.
@interface MagicStackModuleContentsFactory : NSObject

// Returns the module contents of `config`'s module type with the module's
// `traitCollection`.
- (UIView*)contentViewForConfig:(MagicStackModule*)config
                traitCollection:(UITraitCollection*)traitCollection;

@end
#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MAGIC_STACK_MAGIC_STACK_MODULE_CONTENTS_FACTORY_H_
