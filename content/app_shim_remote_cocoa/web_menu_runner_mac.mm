// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/app_shim_remote_cocoa/web_menu_runner_mac.h"

#include <stddef.h>

#include "base/base64.h"
#include "base/strings/sys_string_conversions.h"

@interface WebMenuRunner (PrivateAPI)

// Worker function used during initialization.
- (void)addItem:(const blink::mojom::MenuItemPtr&)item;

// A callback for the menu controller object to call when an item is selected
// from the menu. This is not called if the menu is dismissed without a
// selection.
- (void)menuItemSelected:(id)sender;

@end  // WebMenuRunner (PrivateAPI)

@implementation WebMenuRunner {
  // The native menu control.
  NSMenu* __strong _menu;

  // A flag set to YES if a menu item was chosen, or NO if the menu was
  // dismissed without selecting an item.
  BOOL _menuItemWasChosen;

  // The index of the selected menu item.
  int _index;

  // The font size being used for the menu.
  CGFloat _fontSize;

  // Whether the menu should be displayed right-aligned.
  BOOL _rightAligned;
}

- (id)initWithItems:(const std::vector<blink::mojom::MenuItemPtr>&)items
           fontSize:(CGFloat)fontSize
       rightAligned:(BOOL)rightAligned {
  if ((self = [super init])) {
    _menu = [[NSMenu alloc] initWithTitle:@""];
    _menu.autoenablesItems = NO;
    _index = -1;
    _fontSize = fontSize;
    _rightAligned = rightAligned;
    for (const auto& item : items) {
      [self addItem:item];
    }
  }
  return self;
}

- (void)addItem:(const blink::mojom::MenuItemPtr&)item {
  if (item->type == blink::mojom::MenuItem::Type::kSeparator) {
    [_menu addItem:[NSMenuItem separatorItem]];
    return;
  }

  NSString* title = base::SysUTF8ToNSString(item->label.value_or(""));
  // https://crbug.com/1140620: SysUTF8ToNSString will return nil if the bits
  // that it is passed cannot be turned into a CFString. If this nil value is
  // passed to -[NSMenuItem addItemWithTitle:action:keyEquivalent], Chromium
  // will crash. Therefore, for debugging, if the result is nil, substitute in
  // the raw bytes, encoded for safety in base64, to allow for investigation.
  if (!title) {
    title = base::SysUTF8ToNSString(base::Base64Encode(*item->label));
  }
  NSMenuItem* menuItem = [_menu addItemWithTitle:title
                                          action:@selector(menuItemSelected:)
                                   keyEquivalent:@""];
  if (item->tool_tip.has_value()) {
    NSString* toolTip = base::SysUTF8ToNSString(item->tool_tip.value());
    [menuItem setToolTip:toolTip];
  }
  [menuItem setEnabled:(item->enabled &&
                        item->type != blink::mojom::MenuItem::Type::kGroup)];
  [menuItem setTarget:self];

  // Set various alignment/language attributes.
  NSMutableDictionary* attrs = [[NSMutableDictionary alloc] initWithCapacity:3];
  NSMutableParagraphStyle* paragraphStyle =
      [[NSMutableParagraphStyle alloc] init];
  paragraphStyle.alignment =
      _rightAligned ? NSTextAlignmentRight : NSTextAlignmentLeft;
  NSWritingDirection writingDirection =
      item->text_direction == base::i18n::RIGHT_TO_LEFT
          ? NSWritingDirectionRightToLeft
          : NSWritingDirectionLeftToRight;
  paragraphStyle.baseWritingDirection = writingDirection;
  paragraphStyle.lineBreakMode = NSLineBreakByTruncatingTail;
  attrs[NSParagraphStyleAttributeName] = paragraphStyle;

  if (item->has_text_direction_override) {
    attrs[NSWritingDirectionAttributeName] =
        @[ @(long{writingDirection} | NSWritingDirectionOverride) ];
  }

  attrs[NSFontAttributeName] = [NSFont menuFontOfSize:_fontSize];

  NSAttributedString* attrTitle =
      [[NSAttributedString alloc] initWithString:title attributes:attrs];
  menuItem.attributedTitle = attrTitle;

  // Set the title as well as the attributed title here. The attributed title
  // will be displayed in the menu, but type-ahead will use the non-attributed
  // string that doesn't contain any leading or trailing whitespace.
  //
  // This is the approach that WebKit uses; see PopupMenuMac::populate():
  // https://github.com/search?q=repo%3AWebKit/WebKit%20PopupMenuMac%3A%3Apopulate&type=code
  NSCharacterSet* whitespaceSet = [NSCharacterSet whitespaceCharacterSet];
  [menuItem setTitle:[title stringByTrimmingCharactersInSet:whitespaceSet]];

  [menuItem setTag:[_menu numberOfItems] - 1];
}

// Reflects the result of the user's interaction with the popup menu. If NO, the
// menu was dismissed without the user choosing an item, which can happen if the
// user clicked outside the menu region or hit the escape key. If YES, the user
// selected an item from the menu.
- (BOOL)menuItemWasChosen {
  return _menuItemWasChosen;
}

- (void)menuItemSelected:(id)sender {
  _menuItemWasChosen = YES;
}

- (void)runMenuInView:(NSView*)view
           withBounds:(NSRect)bounds
         initialIndex:(int)index {
  // Set up the button cell, converting to NSView coordinates. The menu is
  // positioned such that the currently selected menu item appears over the
  // popup button, which is the expected Mac popup menu behavior.
  NSPopUpButtonCell* cell = [[NSPopUpButtonCell alloc] initTextCell:@""
                                                          pullsDown:NO];
  cell.menu = _menu;
  // We use selectItemWithTag below so if the index is out-of-bounds nothing
  // bad happens.
  [cell selectItemWithTag:index];

  if (_rightAligned) {
    cell.userInterfaceLayoutDirection =
        NSUserInterfaceLayoutDirectionRightToLeft;
    _menu.userInterfaceLayoutDirection =
        NSUserInterfaceLayoutDirectionRightToLeft;
  }

  // When popping up a menu near the Dock, Cocoa restricts the menu
  // size to not overlap the Dock, with a scroll arrow.  Below a
  // certain point this doesn't work.  At that point the menu is
  // popped up above the element, so that the current item can be
  // selected without mouse-tracking selecting a different item
  // immediately.
  //
  // Unfortunately, instead of popping up above the passed |bounds|,
  // it pops up above the bounds of the view passed to inView:.  Use a
  // dummy view to fake this out.
  NSView* dummyView = [[NSView alloc] initWithFrame:bounds];
  [view addSubview:dummyView];

  // Display the menu, and set a flag if a menu item was chosen.
  [cell attachPopUpWithFrame:dummyView.bounds inView:dummyView];
  [cell performClickWithFrame:dummyView.bounds inView:dummyView];

  [dummyView removeFromSuperview];

  if ([self menuItemWasChosen])
    _index = [cell indexOfSelectedItem];
}

- (void)cancelSynchronously {
  [_menu cancelTrackingWithoutAnimation];

  // Starting with macOS 14, menus were reimplemented with Cocoa (rather than
  // with the old Carbon). However, with that reimplementation came a bug
  // whereupon using -cancelTrackingWithoutAnimation does not consistently
  // immediately cancel the tracking, and leaves associated state remaining
  // uncleared for an indeterminate amount of time. If a new tracking session is
  // begun before that state is cleared, an NSInternalInconsistencyException is
  // thrown. See the discussion on https://crbug.com/1497774 and FB13320260.
  // Therefore, on macOS 14+, clear out that state so that a new tracking
  // session can begin immediately.
  if (@available(macOS 14, *)) {
    // When running a menu tracking session, the instances of
    // NSMenuTrackingSession make calls to class methods of NSPopupMenuWindow:
    //
    // -[NSMenuTrackingSession sendBeginTrackingNotifications]
    //   -> +[NSPopupMenuWindow enableWindowReuse]
    // and
    // -[NSMenuTrackingSession sendEndTrackingNotifications]
    //   -> +[NSPopupMenuWindow disableWindowReusePurgingCache]
    //
    // +enableWindowReuse populates the _NSContextMenuWindowReuseSet global, and
    // +disableWindowReusePurgingCache walks the set, clears out some state
    // inside of each item, and then nils out the global, preparing for the next
    // call to +enableWindowReuse.
    //
    // +disableWindowReusePurgingCache can be called directly here, as it's
    // idempotent enough.

    Class popupMenuWindowClass = NSClassFromString(@"NSPopupMenuWindow");
    if ([popupMenuWindowClass
            respondsToSelector:@selector(disableWindowReusePurgingCache)]) {
      [popupMenuWindowClass
          performSelector:@selector(disableWindowReusePurgingCache)];
    }
  }
}

- (int)indexOfSelectedItem {
  return _index;
}

@end  // WebMenuRunner
