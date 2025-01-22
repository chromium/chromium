// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/app_shim_remote_cocoa/web_menu_runner_mac.h"

#include <AppKit/AppKit.h>
#include <Foundation/Foundation.h>
#include <objc/runtime.h>
#include <stddef.h>

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
  // The native menu control.
  NSMenu* __strong _menu;

  // The index of the selected menu item. Set to -1 initially, and then set to
  // the index of the selected item if an item was selected.
  int _selectedItemIndex;

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
    if (rightAligned) {
      _menu.userInterfaceLayoutDirection =
          NSUserInterfaceLayoutDirectionRightToLeft;
    }

    _selectedItemIndex = -1;
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
  NSCharacterSet* whitespaceSet = NSCharacterSet.whitespaceCharacterSet;
  menuItem.title = [title stringByTrimmingCharactersInSet:whitespaceSet];

  menuItem.tag = _menu.numberOfItems - 1;
}

- (BOOL)menuItemWasChosen {
  return _selectedItemIndex != -1;
}

- (int)indexOfSelectedItem {
  return _selectedItemIndex;
}

- (void)menuItemSelected:(id)sender {
  _selectedItemIndex = [sender tag];
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

  // Add a checkmark to the initial item.
  NSMenuItem* item = [_menu itemWithTag:index];
  item.state = NSControlStateValueOn;

  // Create a rect roughly containing the initial item, and center it in the
  // provided bounds.
  NSRect initialItemBounds =
      NSInsetRect(bounds, /*dX=*/0.0f,
                  /*dY=*/(NSHeight(bounds) - _fontSize) / 2);
  initialItemBounds = NSIntegralRect(initialItemBounds);

  // Increase the minimum width of the menu so that we don't end up with a tiny
  // menu floating in a sea of the popup widget.
  _menu.minimumWidth = NSWidth(initialItemBounds);

  // The call to do the popup menu takes the location of the upper-left corner.
  // Tweak it to compensate for the overall padding of the menu.
  NSPoint initialPoint =
      NSMakePoint(NSMinX(initialItemBounds), NSMaxY(initialItemBounds));
  initialPoint.x -= 8;
  initialPoint.y += 4;

  // Do the popup.
  [_menu popUpMenuPositioningItem:item atLocation:initialPoint inView:view];
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
