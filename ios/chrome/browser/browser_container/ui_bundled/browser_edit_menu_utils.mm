// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browser_container/ui_bundled/browser_edit_menu_utils.h"

#import "ios/chrome/browser/browser_container/ui_bundled/browser_edit_menu_constants.h"
#import "ios/chrome/browser/intelligence/features/features.h"

namespace {
// Creates the Chrome sub menu if needed.
void EnsureChromeMenuCreated(id<UIMenuBuilder> builder) {
  UIMenu* chrome_menu =
      [builder menuForIdentifier:edit_menu::kBrowserEditMenuChromeMenuId];
  if (chrome_menu) {
    return;
  }
  chrome_menu = [UIMenu menuWithTitle:@""
                                image:nil
                           identifier:edit_menu::kBrowserEditMenuChromeMenuId
                              options:UIMenuOptionsDisplayInline
                             children:@[]];
  // By default, try to add it before the lookup menu.
  // Test if lookup menu exists.
  if ([builder menuForIdentifier:UIMenuLookup]) {
    [builder insertSiblingMenu:chrome_menu
        beforeMenuForIdentifier:UIMenuLookup];
    return;
  }
  // Otherwise, put the menu after the following submenus.
  NSArray* put_menu_after =
      @[ UIMenuFormat, UIMenuReplace, UIMenuStandardEdit ];
  for (NSString* menu_id : put_menu_after) {
    if ([builder menuForIdentifier:menu_id]) {
      [builder insertSiblingMenu:chrome_menu afterMenuForIdentifier:menu_id];
      return;
    }
  }

  // Last possibility, put the menu first
  [builder insertChildMenu:chrome_menu atStartOfMenuForIdentifier:UIMenuRoot];
}

void EnsureChromeSecondaryMenuCreated(id<UIMenuBuilder> builder) {
  UIMenu* chrome_secondary_menu =
      [builder menuForIdentifier:edit_menu::kBrowserEditMenuSecondaryMenuId];
  if (chrome_secondary_menu) {
    return;
  }
  UIMenu* explainWithGeminiMenu =
      [UIMenu menuWithTitle:@""
                      image:nil
                 identifier:edit_menu::kBrowserEditMenuSecondaryMenuId
                    options:UIMenuOptionsDisplayInline
                   children:@[]];

  [builder insertChildMenu:explainWithGeminiMenu
      atEndOfMenuForIdentifier:UIMenuStandardEdit];
}
}  // namespace

namespace edit_menu {

void AddElementToChromeMenu(id<UIMenuBuilder> builder,
                            UIMenuElement* element,
                            BOOL is_primary_menu) {
  NSString* identifier;
  if (is_primary_menu) {
    EnsureChromeMenuCreated(builder);
    identifier = kBrowserEditMenuChromeMenuId;
  } else {
    EnsureChromeSecondaryMenuCreated(builder);
    identifier = kBrowserEditMenuSecondaryMenuId;
  }
  [builder replaceChildrenOfMenuForIdentifier:identifier
                            fromChildrenBlock:^NSArray<UIMenuElement*>*(
                                NSArray<UIMenuElement*>* oldElements) {
                              return [oldElements arrayByAddingObject:element];
                            }];
}

}  // namespace edit_menu
