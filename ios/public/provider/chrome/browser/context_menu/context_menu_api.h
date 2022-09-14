// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_CONTEXT_MENU_CONTEXT_MENU_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_CONTEXT_MENU_CONTEXT_MENU_API_H_

#import <UIKit/UIKit.h>

#import "ios/web/public/ui/context_menu_params.h"

class ChromeBrowserState;

// Wraps information to add/show to/in a context menu
@interface ElementsToAddToContextMenu : NSObject

// The title of the context menu. Can be nil.
@property(nonatomic, copy) NSString* title;

// List of elements to add to a context menu. Can be nil.
@property(nonatomic, copy) NSMutableArray<UIMenuElement*>* elements;

@end

namespace web {
class WebState;
}  // namespace web

namespace ios {
namespace provider {

// Returns the elements to add to the context menu, with their title. If no
// elements needs to be added, returns nil.
ElementsToAddToContextMenu* GetContextMenuElementsToAdd(
    ChromeBrowserState* browser_state,
    web::WebState* web_state,
    web::ContextMenuParams params,
    UIViewController* presenting_view_controller);

}  // namespace provider
}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_CONTEXT_MENU_CONTEXT_MENU_API_H_
