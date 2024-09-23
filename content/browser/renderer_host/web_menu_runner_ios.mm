// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/browser/renderer_host/web_menu_runner_ios.h"

#include "base/strings/sys_string_conversions.h"

@interface UIContextMenuInteraction ()
- (void)_presentMenuAtLocation:(CGPoint)location;
@end

@interface WebMenuRunner () <UIContextMenuInteractionDelegate>
@end

@implementation WebMenuRunner {
  // The UIView in which the popup menu will be displayed.
  UIView* __weak _view;

  // The bounds of the select element from which the menu was triggered.
  CGRect _elementBounds;

  // The index of the selected menu item.
  size_t _selectedIndex;

  // A flag set to YES if a menu item was chosen, or NO if the menu was
  // dismissed without selecting an item.
  BOOL _menuItemWasChosen;

  // The native UIMenu object.
  UIMenu* __strong _menu;

  // Interaction for displaying a popup menu.
  UIContextMenuInteraction* __strong _selectContextMenuInteraction;

  // Delegate to handle menu select/cancel events.
  base::WeakPtr<content::MenuInteractionDelegate> _delegate;
}

- (id)initWithDelegate:(base::WeakPtr<content::MenuInteractionDelegate>)delegate
                 items:(const std::vector<blink::mojom::MenuItemPtr>&)items
          initialIndex:(int)index
              fontSize:(CGFloat)fontSize
          rightAligned:(BOOL)rightAligned {
  if ((self = [super init])) {
    _delegate = delegate;

    DCHECK_GE(index, 0);
    _selectedIndex = static_cast<size_t>(index);

    [self createMenu:items];
  }
  return self;
}

- (void)showMenuInView:(UIView*)view withBounds:(CGRect)bounds {
  _view = view;
  _elementBounds = bounds;

  _selectContextMenuInteraction =
      [[UIContextMenuInteraction alloc] initWithDelegate:self];
  [_view addInteraction:_selectContextMenuInteraction];

  // TODO(crbug.com/40274444): _presentMenuAtLocation is a private API
  // which triggers the ContextMenu immediately at a specified location. By
  // default, the ContextMenu is only triggered on long press or 3D touch. This
  // private API is needed to use because we expect the popup menu to appear
  // immediately when the user touches the <select> element area.
  [_selectContextMenuInteraction _presentMenuAtLocation:_elementBounds.origin];
}

- (void)dealloc {
  [_view removeInteraction:_selectContextMenuInteraction];
}

#pragma mark - UIContextMenuInteractionDelegate

// TODO(crbug.com/40266320): This menu is being shown with unwanted effects.
// Need to find a way to show just the menu without using private API.
- (UIContextMenuConfiguration*)contextMenuInteraction:
                                   (UIContextMenuInteraction*)interaction
                       configurationForMenuAtLocation:(CGPoint)location {
  return [UIContextMenuConfiguration
      configurationWithIdentifier:nil
                  previewProvider:nil
                   actionProvider:^UIMenu* _Nullable(
                       NSArray<UIMenuElement*>* _Nonnull suggestedActions) {
                     return self->_menu;
                   }];
}

- (UITargetedPreview*)contextMenuInteraction:
                          (UIContextMenuInteraction*)interaction
                               configuration:
                                   (UIContextMenuConfiguration*)configuration
       highlightPreviewForItemWithIdentifier:(id<NSCopying>)identifier {
  UIView* snapshotView = [_view resizableSnapshotViewFromRect:_elementBounds
                                           afterScreenUpdates:NO
                                                withCapInsets:UIEdgeInsetsZero];

  UIPreviewTarget* previewTarget = [[UIPreviewTarget alloc]
      initWithContainer:_view
                 center:CGPointMake(CGRectGetMidX(_elementBounds),
                                    CGRectGetMidY(_elementBounds))];

  return
      [[UITargetedPreview alloc] initWithView:snapshotView
                                   parameters:[[UIPreviewParameters alloc] init]
                                       target:previewTarget];
}

- (void)contextMenuInteraction:(UIContextMenuInteraction*)interaction
       willEndForConfiguration:(UIContextMenuConfiguration*)configuration
                      animator:(id<UIContextMenuInteractionAnimating>)animator {
  _menu = nil;
  if (!_delegate) {
    return;
  }

  if (_menuItemWasChosen) {
    _delegate->OnMenuItemSelected(_selectedIndex);
  } else {
    _delegate->OnMenuCanceled();
  }
}

#pragma mark - Private

// Creates the native UIMenu object using the provided list of menu items.
- (void)createMenu:(const std::vector<blink::mojom::MenuItemPtr>&)items {
  NSMutableArray* actions = [NSMutableArray array];

  for (size_t i = 0; i < items.size(); ++i) {
    UIAction* action = [self addItem:items[i] itemIndex:i];
    if (i == _selectedIndex) {
      action.state = UIMenuElementStateOn;
    }
    [actions addObject:action];
  }

  _menu = [UIMenu menuWithTitle:@""
                          image:nil
                     identifier:nil
                        options:UIMenuOptionsDisplayInline
                       children:actions];
}

// Worker function used during initialization.
- (UIAction*)addItem:(const blink::mojom::MenuItemPtr&)item
           itemIndex:(size_t)index {
  NSString* title = base::SysUTF8ToNSString(item->label.value_or(""));
  UIAction* itemAction =
      [UIAction actionWithTitle:title
                          image:nil
                     identifier:nil
                        handler:^(__kindof UIAction* action) {
                          [self menuItemSelected:index];
                        }];

  return itemAction;
}

// A callback for the menu controller object to call when an item is selected
// from the menu. This is not called if the menu is dismissed without a
// selection.
- (void)menuItemSelected:(size_t)index {
  _menuItemWasChosen = YES;
  _selectedIndex = index;
}

@end  // WebMenuRunner
