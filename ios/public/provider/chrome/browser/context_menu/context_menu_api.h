// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_CONTEXT_MENU_CONTEXT_MENU_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_CONTEXT_MENU_CONTEXT_MENU_API_H_

#import <UIKit/UIKit.h>

#import "ios/web/public/ui/context_menu_params.h"

class ChromeBrowserState;

namespace web {
class WebState;
}  // namespace web

namespace ios {
namespace provider {

// Returns true if any items were added to |menuElements| based on associated
// |browserState|, |webState| and |params|.
bool AddContextMenuElements(NSMutableArray<UIMenuElement*>* menu_elements,
                            ChromeBrowserState* browser_state,
                            web::WebState* web_state,
                            web::ContextMenuParams params);

}  // namespace provider
}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_CONTEXT_MENU_CONTEXT_MENU_API_H_
