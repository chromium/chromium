// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_APP_SHIM_REMOTE_COCOA_WEB_MENU_RUNNER_MAC_H_
#define CONTENT_APP_SHIM_REMOTE_COCOA_WEB_MENU_RUNNER_MAC_H_

#import <Cocoa/Cocoa.h>

#include <vector>

#include "base/functional/callback_forward.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/choosers/popup_menu.mojom.h"

// WebMenuRunner ---------------------------------------------------------------
// A class for presenting a menu when a HTML select element is clicked, and
// returning the user selection or dismissal without selection. If a menu item
// is selected, MenuDelegate is informed and sets a flag which can be queried
// after the menu has finished running.
CONTENT_EXPORT
@interface WebMenuRunner : NSObject

// Initializes the MenuDelegate with a list of items sent from Blink.
- (id)initWithItems:(const std::vector<blink::mojom::MenuItemPtr>&)items
           fontSize:(CGFloat)fontSize
       rightAligned:(BOOL)rightAligned;

// Returns YES if an item was selected from the menu, NO if the menu was
// dismissed.
@property(readonly) BOOL menuItemWasChosen;

// Returns the index of selected menu item, or its initial value (-1) if no item
// was selected.
@property(readonly) int indexOfSelectedItem;

// Displays and runs a native popup menu.
- (void)runMenuInView:(NSView*)view
           withBounds:(NSRect)bounds
         initialIndex:(int)index;

// Cancels the display of a menu if it is shown. This is called in situations
// where Blink is asking for the cancellation (e.g. the page closed or the
// contents of the menu changed so the menu has to be rebuilt). Because this is
// driven by Blink, and in some cases Blink will immediately re-issue the menu,
// this is a synchronous cancellation with no animation. See
// https://crbug.com/41370640.
- (void)cancelSynchronously;

@end  // @interface WebMenuRunner

// The callback for testing. Parameters are the same as on
// -runMenuInView:withBounds:initialIndex:.
using MenuWasRunCallback = base::RepeatingCallback<void(NSView*, NSRect, int)>;

@interface WebMenuRunner (TestingAPI)
// Register a callback to be called if a popup menu is invoked for a specific
// view. If a callback is registered for a view, the menu will not be invoked
// but instead, the callback will be run.
+ (void)registerForTestingMenuRunCallback:(MenuWasRunCallback)callback
                                  forView:(NSView*)view;
+ (void)unregisterForTestingMenuRunCallbackForView:(NSView*)view;
@end

#endif  // CONTENT_APP_SHIM_REMOTE_COCOA_WEB_MENU_RUNNER_MAC_H_
