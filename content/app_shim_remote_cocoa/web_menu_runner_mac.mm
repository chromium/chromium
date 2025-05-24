// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/app_shim_remote_cocoa/web_menu_runner_mac.h"

#include <AppKit/AppKit.h>
#include <Foundation/Foundation.h>
#include <objc/runtime.h>
#include <stddef.h>

#include <optional>

#include "base/base64.h"
#include "base/mac/mac_util.h"
#include "base/strings/sys_string_conversions.h"

namespace {

// A key to attach a MenuWasRunCallbackHolder to the NSView*.
static const char kMenuWasRunCallbackKey = 0;

}  // namespace

@interface MenuWasRunCallbackHolder : NSObject
@property MenuWasRunCallback callback;
@end

@implementation MenuWasRunCallbackHolder
@synthesize callback = _callback;
@end

@implementation WebMenuRunner {
  // The native menu.
  NSMenu* __strong _menu;

  // The index of the selected menu item.
  std::optional<int> _selectedMenuItemIndex;

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

  std::string label = item->label.value_or("");
  NSString* title = base::SysUTF8ToNSString(label);
  // https://crbug.com/40726719: SysUTF8ToNSString will return nil if the bits
  // that it is passed cannot be turned into a CFString. If this nil value is
  // passed to -[NSMenuItem addItemWithTitle:action:keyEquivalent:], Chromium
  // will crash. Therefore, for debugging, if the result is nil, substitute in
  // the raw bytes, encoded for safety in base64, to allow for investigation.
  if (!title) {
    title = base::SysUTF8ToNSString(base::Base64Encode(label));
  }

  // TODO(https://crbug.com/389084419): Figure out how to handle
  // blink::mojom::MenuItem::Type::kGroup items. This should use the macOS 14+
  // support for section headers, but popup menus have to resize themselves to
  // match the scale of the page, and there's no good way (currently) to get the
  // font used for section header items in order to scale it and set it.
  NSMenuItem* menuItem = [_menu addItemWithTitle:title
                                          action:@selector(menuItemSelected:)
                                   keyEquivalent:@""];

  if (item->tool_tip.has_value()) {
    menuItem.toolTip = base::SysUTF8ToNSString(item->tool_tip.value());
  }
  menuItem.enabled =
      item->enabled && item->type != blink::mojom::MenuItem::Type::kGroup;
  menuItem.target = self;

  // Set various alignment/language attributes.
  NSMutableDictionary* attrs = [NSMutableDictionary dictionary];
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
  NSCharacterSet* whitespaceSet = NSCharacterSet.whitespaceCharacterSet;
  menuItem.title = [title stringByTrimmingCharactersInSet:whitespaceSet];

  menuItem.tag = _menu.numberOfItems - 1;
}

- (std::optional<int>)selectedMenuItemIndex {
  return _selectedMenuItemIndex;
}

- (void)menuItemSelected:(id)sender {
  _selectedMenuItemIndex = [sender tag];
}

- (void)runMenuInView:(NSView*)view
           withBounds:(NSRect)bounds
         initialIndex:(int)index {
  // In a testing situation, make the callback and early-exit.
  MenuWasRunCallbackHolder* holder =
      objc_getAssociatedObject(view, &kMenuWasRunCallbackKey);
  if (holder) {
    holder.callback.Run(view, bounds, index);
    return;
  }

  // Using NSPopUpButtonCell in this way is not SPI, but there is new(er) API to
  // show a pop-up menu in a way that avoids the hassle of instantiating a cell
  // just to use its innards.
  //
  // However, that API, -[NSMenu popUpMenuPositioningItem:atLocation:inView:],
  // is broken and displays menus that are the incorrect width and which
  // improperly truncate their contents (see https://crbug.com/401443090).
  //
  // This has been filed as FB16843355. TODO(https://crbug.com/389067059): When
  // this FB is resolved, switch to the new API by relanding an adapted version
  // of https://crrev.com/c/6173642.
  //
  // In addition, note that there are web pages that use popups with a font size
  // of 0. When relanding, font size will likely play a part in the calculation
  // of the menu position of the reland, so be sure to not regress menu
  // positioning in that case (https://crbug.com/404294118).

  // Set up the button cell, converting to NSView coordinates. The menu is
  // positioned such that the currently selected menu item appears over the
  // popup button, which is the expected Mac popup menu behavior.
  NSPopUpButtonCell* cell = [[NSPopUpButtonCell alloc] initTextCell:@""
                                                          pullsDown:NO];
  cell.menu = _menu;
  // Use -selectItemWithTag: so if the index is out-of-bounds nothing bad
  // happens.
  [cell selectItemWithTag:index];

  if (_rightAligned) {
    cell.userInterfaceLayoutDirection =
        NSUserInterfaceLayoutDirectionRightToLeft;
    _menu.userInterfaceLayoutDirection =
        NSUserInterfaceLayoutDirectionRightToLeft;
  }

  // When popping up a menu near the Dock, Cocoa restricts the menu size to not
  // overlap the Dock, with a scroll arrow. At a certain point, though, this
  // doesn't work, so the menu is repositioned, so that the current item can be
  // selected without mouse-tracking selecting a different item immediately.
  //
  // Unfortunately, in that situation, the cell will try to reposition the menu
  // relative to the view passed in, as it believes that the view is the
  // NSPopUpButton control. However, `view` is the view containing the entire
  // web page, so if it were to be passed in, the menu would be repositioned
  // relative to that, and would end up being wildly misplaced.
  //
  // Therefore, set up a fake "control" view corresponding to the visual bounds
  // of the HTML element, so that if the menu needs to be repositioned, it is
  // repositioned relative to that.
  NSView* fakeControlView = [[NSView alloc] initWithFrame:bounds];
  [view addSubview:fakeControlView];

  // Display the menu.
  [cell attachPopUpWithFrame:fakeControlView.bounds inView:fakeControlView];
  [cell performClickWithFrame:fakeControlView.bounds inView:fakeControlView];

  [fakeControlView removeFromSuperview];
}

- (void)cancelSynchronously {
  [_menu cancelTrackingWithoutAnimation];

  // Starting with macOS 14, menus were reimplemented with Cocoa (rather than
  // with the old Carbon). However, in macOS 14, with that reimplementation came
  // a bug whereupon using -cancelTrackingWithoutAnimation did not consistently
  // immediately cancel the tracking, and left associated state remaining
  // uncleared for an indeterminate amount of time. If a new tracking session
  // began before that state was cleared, an NSInternalInconsistencyException
  // was thrown. See the discussion on https://crbug.com/40939221 and
  // FB13320260.
  //
  // On macOS 14, therefore, when cancelling synchronously, clear out that state
  // so that a new tracking session can begin immediately.
  //
  // With macOS 15, these global state methods moved from being class methods on
  // NSPopupMenuWindow to being instance methods on NSMenuTrackingSession, so
  // this workaround is inapplicable.
  if (base::mac::MacOSMajorVersion() == 14) {
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

+ (void)registerForTestingMenuRunCallback:(MenuWasRunCallback)callback
                                  forView:(NSView*)view {
  MenuWasRunCallbackHolder* holder = [[MenuWasRunCallbackHolder alloc] init];
  holder.callback = callback;
  objc_setAssociatedObject(view, &kMenuWasRunCallbackKey, holder,
                           OBJC_ASSOCIATION_RETAIN);
}

+ (void)unregisterForTestingMenuRunCallbackForView:(NSView*)view {
  objc_setAssociatedObject(view, &kMenuWasRunCallbackKey, nil,
                           OBJC_ASSOCIATION_RETAIN);
}

@end  // WebMenuRunner
