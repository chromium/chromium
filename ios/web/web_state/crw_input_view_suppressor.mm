// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/crw_input_view_suppressor.h"

namespace {

// Returns the first responder in `view` or its subview hierarchy, or `nil` if
// no view in the subtree is the first responder.
UIView* GetFirstResponderSubview(UIView* view) {
  if ([view isFirstResponder]) {
    return view;
  }

  for (UIView* subview in [view subviews]) {
    UIView* first_responder = GetFirstResponderSubview(subview);
    if (first_responder) {
      return first_responder;
    }
  }

  return nil;
}

}  // namespace

@implementation CRWInputViewSuppressor {
  // Owning view.
  __weak UIView* _view;
  // Zero-sized view that replaces the system software keyboard.
  UIView* _emptyInputView;
  // The active first responder's suppressed `inputAssistantItem`.
  // Only one `UIResponder` is first responder at a time, and `WKWebView` uses
  // one `WKContentView` responder for all DOM elements.
  __weak UITextInputAssistantItem* _suppressedInputAssistantItem;
  // Saved bar button groups for `_suppressedInputAssistantItem`.
  NSArray<UIBarButtonItemGroup*>* _savedLeadingBarButtonGroups;
  NSArray<UIBarButtonItemGroup*>* _savedTrailingBarButtonGroups;
}

#pragma mark - Public

- (instancetype)initWithView:(UIView*)view {
  if ((self = [super init])) {
    _view = view;
    _emptyInputView = [[UIView alloc] initWithFrame:CGRectZero];
  }
  return self;
}

- (void)reloadInputViews {
  UIView* firstResponder = GetFirstResponderSubview(_view);
  // Suppress `inputAssistantItem` bar button groups before reloading input
  // views so UIKit picks up the updated shortcut bar state immediately.
  [self suppressInputAssistantItemForResponder:firstResponder];
  // Call `reloadInputViews` on the active first responder in the hierarchy,
  // as UIKit ignores it on non-first-responders.
  [firstResponder reloadInputViews];
}

- (void)disconnect {
  UIView* firstResponder = GetFirstResponderSubview(_view);
  _view = nil;
  [self restoreSuppressedInputAssistantItem];
  [firstResponder reloadInputViews];
}

#pragma mark - CRWResponderInputView

- (UIView*)inputView {
  [self suppressInputAssistantItemForResponder:GetFirstResponderSubview(_view)];
  // Return a zero-sized `UIView` to hide the software keyboard.
  return _emptyInputView;
}

- (UIInputViewController*)inputViewController {
  return nil;
}

- (UIView*)inputAccessoryView {
  // Returning `nil` hides the form accessory toolbar when a web input element
  // is focused.
  return nil;
}

- (UIInputViewController*)inputAccessoryViewController {
  return nil;
}

#pragma mark - Private

// Saves `UIBarButtonItemGroup` arrays for `firstResponder`'s
// `inputAssistantItem` and clears non-empty groups.
- (void)suppressInputAssistantItemForResponder:(UIView*)firstResponder {
  UITextInputAssistantItem* item = firstResponder.inputAssistantItem;
  if (!item) {
    if (!_suppressedInputAssistantItem) {
      [self restoreSuppressedInputAssistantItem];
    }
    return;
  }
  if (_suppressedInputAssistantItem != item) {
    // Restore any previously suppressed item before tracking `item`.
    [self restoreSuppressedInputAssistantItem];
    _suppressedInputAssistantItem = item;
  }

  if (item.leadingBarButtonGroups.count > 0) {
    _savedLeadingBarButtonGroups = [item.leadingBarButtonGroups copy];
    item.leadingBarButtonGroups = @[];
  }
  if (item.trailingBarButtonGroups.count > 0) {
    _savedTrailingBarButtonGroups = [item.trailingBarButtonGroups copy];
    item.trailingBarButtonGroups = @[];
  }
}

// Restores saved bar button groups on `_suppressedInputAssistantItem`, if any,
// even if its responder resigned while suppressed.
- (void)restoreSuppressedInputAssistantItem {
  if (_savedLeadingBarButtonGroups) {
    _suppressedInputAssistantItem.leadingBarButtonGroups =
        _savedLeadingBarButtonGroups;
  }
  if (_savedTrailingBarButtonGroups) {
    _suppressedInputAssistantItem.trailingBarButtonGroups =
        _savedTrailingBarButtonGroups;
  }
  _suppressedInputAssistantItem = nil;
  _savedLeadingBarButtonGroups = nil;
  _savedTrailingBarButtonGroups = nil;
}

@end
