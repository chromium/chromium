// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTEXT_MENU_UI_BUNDLED_CONTEXT_MENU_CONFIGURATION_PROVIDER_TESTING_H_
#define IOS_CHROME_BROWSER_CONTEXT_MENU_UI_BUNDLED_CONTEXT_MENU_CONFIGURATION_PROVIDER_TESTING_H_

#import "ios/chrome/browser/context_menu/ui_bundled/context_menu_configuration_provider.h"

// Testing category to expose a method used for tests only.
@interface ContextMenuConfigurationProvider (Testing)

// Returns an action provider for a context menu, based on its associated
// `webState`, `params` and `baseViewController`.
// `params` is copied in order to be used in blocks.
- (UIContextMenuActionProvider)
    contextMenuActionProviderForWebState:(web::WebState*)webState
                                  params:(web::ContextMenuParams)params;

@end

#endif  // IOS_CHROME_BROWSER_CONTEXT_MENU_UI_BUNDLED_CONTEXT_MENU_CONFIGURATION_PROVIDER_TESTING_H_
