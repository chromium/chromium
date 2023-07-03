// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_WEB_MENU_RUNNER_IOS_H_
#define CONTENT_BROWSER_RENDERER_HOST_WEB_MENU_RUNNER_IOS_H_

#import <UIKit/UIKit.h>

#include "base/memory/weak_ptr.h"
#include "content/browser/renderer_host/popup_menu_interaction_delegate.h"
#include "third_party/blink/public/mojom/choosers/popup_menu.mojom.h"

@interface WebMenuRunner : NSObject

// Initializes a new native menu with a list of items sent from WebKit.
- (id)initWithDelegate:(base::WeakPtr<content::MenuInteractionDelegate>)delegate
                 items:(const std::vector<blink::mojom::MenuItemPtr>&)items
          initialIndex:(int)index
              fontSize:(CGFloat)fontSize
          rightAligned:(BOOL)rightAligned;

// Displays the popup menu at the location of the HTML <select> element.
// |bounds| represents the bounds of the <select> element from which the popup
// menu was triggered.
- (void)showMenuInView:(UIView*)view withBounds:(CGRect)bounds;

@end  // @interface WebMenuRunner

#endif  // CONTENT_BROWSER_RENDERER_HOST_WEB_MENU_RUNNER_IOS_H_
