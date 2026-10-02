// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/crw_web_view.h"

#import "base/feature_list.h"
#import "base/metrics/histogram_functions.h"
#import "ios/components/enterprise/data_controls/clipboard_enums.h"
#import "ios/components/enterprise/data_controls/metrics_utils.h"
#import "ios/web/common/crw_edit_menu_builder.h"
#import "ios/web/common/crw_input_view_provider.h"
#import "ios/web/common/features.h"
#import "ios/web/web_state/crw_data_controls_delegate.h"

using data_controls::ClipboardAction;
using data_controls::ClipboardSource;
using data_controls::RecordClipboardOutcomeMetrics;
using data_controls::RecordClipboardSourceMetrics;

namespace {

// Returns the first responder in `view` or its subview hierarchy, or `nil` if
// no view in the subtree is the first responder.
UIView* GetFirstResponderSubview(UIView* view) {
  if ([view isFirstResponder]) {
    return view;
  }

  for (UIView* subview in [view subviews]) {
    UIView* firstResponder = GetFirstResponderSubview(subview);
    if (firstResponder) {
      return firstResponder;
    }
  }

  return nil;
}

}  // namespace

@implementation CRWWebView {
  // Zero-sized view that replaces the system software keyboard and any custom
  // `inputViewProvider` input view when `shouldSuppressInputViews` is `YES`.
  UIView* _emptyInputView;
}

#pragma mark - Public Properties

- (void)setShouldSuppressInputViews:(BOOL)shouldSuppressInputViews {
  if (_shouldSuppressInputViews == shouldSuppressInputViews) {
    return;
  }
  _shouldSuppressInputViews = shouldSuppressInputViews;
  // Call `reloadInputViews` on the active first responder in the hierarchy, as
  // UIKit ignores it on non-first-responders.
  [GetFirstResponderSubview(self) reloadInputViews];
}

#pragma mark - UIResponder

- (void)buildMenuWithBuilder:(id<UIMenuBuilder>)builder {
  [super buildMenuWithBuilder:builder];
  if (!base::FeatureList::IsEnabled(
          web::features::kRestoreWKWebViewEditMenuHandler)) {
    if (![self canPerformAction:@selector(copy:) withSender:self]) {
      // `WKWebView buildMenuWithBuilder:` is called too often in WKWebView,
      // sometimes when there is no selection.
      // As a proxy to detect if we should add our items, only add Chrome
      // features if there is something to copy in the view.
      return;
    }
    [self.editMenuBuilder buildMenuWithBuilder:builder];
  }
}

- (UIView*)inputView {
  if (_shouldSuppressInputViews) {
    // When `UIResponder.inputView` is `nil`, the system software keyboard is
    // displayed. Return a zero-sized `UIView` instead to suppress the keyboard.
    if (!_emptyInputView) {
      _emptyInputView = [[UIView alloc] initWithFrame:CGRectZero];
    }
    return _emptyInputView;
  }
  id<CRWResponderInputView> responderInputView =
      self.inputViewProvider.responderInputView;
  if ([responderInputView respondsToSelector:@selector(inputView)]) {
    UIView* view = [responderInputView inputView];
    if (view) {
      return view;
    }
  }
  return [super inputView];
}

- (UIInputViewController*)inputViewController {
  if (_shouldSuppressInputViews) {
    return nil;
  }
  id<CRWResponderInputView> responderInputView =
      self.inputViewProvider.responderInputView;
  if ([responderInputView respondsToSelector:@selector(inputViewController)]) {
    UIInputViewController* controller =
        [responderInputView inputViewController];
    if (controller) {
      return controller;
    }
  }
  return [super inputViewController];
}

- (UIView*)inputAccessoryView {
  if (_shouldSuppressInputViews) {
    // Returning `nil` hides the form accessory toolbar when a web input element
    // is focused.
    return nil;
  }
  id<CRWResponderInputView> responderInputView =
      self.inputViewProvider.responderInputView;
  if ([responderInputView respondsToSelector:@selector(inputAccessoryView)]) {
    UIView* view = [responderInputView inputAccessoryView];
    if (view) {
      return view;
    }
  }
  return [super inputAccessoryView];
}

- (UIInputViewController*)inputAccessoryViewController {
  if (_shouldSuppressInputViews) {
    return nil;
  }
  id<CRWResponderInputView> responderInputView =
      self.inputViewProvider.responderInputView;
  if ([responderInputView
          respondsToSelector:@selector(inputAccessoryViewController)]) {
    UIInputViewController* controller =
        [responderInputView inputAccessoryViewController];
    if (controller) {
      return controller;
    }
  }
  return [super inputAccessoryViewController];
}

#pragma mark - UIResponderStandardEditActions

- (void)copy:(id)sender {
  RecordClipboardSourceMetrics(ClipboardAction::kCopy,
                               ClipboardSource::kEditMenu);

  if (!self.dataControlsDelegate) {
    [super copy:sender];
    return;
  }

  __weak CRWWebView* weakSelf = self;
  [self.dataControlsDelegate
      shouldAllowCopyWithDecisionHandler:^(BOOL allowed) {
        [weakSelf onCopyAllowed:allowed sender:sender];
      }];
}

- (void)paste:(id)sender {
  RecordClipboardSourceMetrics(ClipboardAction::kPaste,
                               ClipboardSource::kEditMenu);

  if (!self.dataControlsDelegate) {
    [super paste:sender];
    return;
  }

  __weak CRWWebView* weakSelf = self;
  [self.dataControlsDelegate
      shouldAllowPasteWithDecisionHandler:^(BOOL allowed) {
        [weakSelf onPasteAllowed:allowed sender:sender];
      }];
}

- (void)cut:(id)sender {
  RecordClipboardSourceMetrics(ClipboardAction::kCut,
                               ClipboardSource::kEditMenu);

  if (!self.dataControlsDelegate) {
    [super cut:sender];
    return;
  }

  __weak CRWWebView* weakSelf = self;
  [self.dataControlsDelegate shouldAllowCutWithDecisionHandler:^(BOOL allowed) {
    [weakSelf onCutAllowed:allowed sender:sender];
  }];
}

- (void)pasteAndMatchStyle:(id)sender {
  RecordClipboardSourceMetrics(ClipboardAction::kPaste,
                               ClipboardSource::kEditMenu);

  if (!self.dataControlsDelegate) {
    [super pasteAndMatchStyle:sender];
    return;
  }

  __weak CRWWebView* weakSelf = self;
  [self.dataControlsDelegate
      shouldAllowPasteWithDecisionHandler:^(BOOL allowed) {
        [weakSelf onPasteAndMatchStyleAllowed:allowed sender:sender];
      }];
}

- (void)pasteAndSearch:(id)sender {
  RecordClipboardSourceMetrics(ClipboardAction::kPaste,
                               ClipboardSource::kEditMenu);

  if (!self.dataControlsDelegate) {
    [super pasteAndSearch:sender];
    return;
  }

  __weak CRWWebView* weakSelf = self;
  [self.dataControlsDelegate
      shouldAllowPasteWithDecisionHandler:^(BOOL allowed) {
        [weakSelf onPasteAndSearchAllowed:allowed sender:sender];
      }];
}

- (void)pasteAndGo:(id)sender {
  RecordClipboardSourceMetrics(ClipboardAction::kPaste,
                               ClipboardSource::kEditMenu);

  if (!self.dataControlsDelegate) {
    [super pasteAndGo:sender];
    return;
  }

  __weak CRWWebView* weakSelf = self;
  [self.dataControlsDelegate
      shouldAllowPasteWithDecisionHandler:^(BOOL allowed) {
        [weakSelf onPasteAndGoAllowed:allowed sender:sender];
      }];
}

#pragma mark - Private

- (void)onCopyAllowed:(BOOL)allowed sender:(id)sender {
  RecordClipboardOutcomeMetrics(ClipboardAction::kCopy, allowed);
  if (allowed && [super canPerformAction:@selector(copy:) withSender:sender]) {
    [super copy:sender];
  }
}

- (void)onPasteAllowed:(BOOL)allowed sender:(id)sender {
  RecordClipboardOutcomeMetrics(ClipboardAction::kPaste, allowed);
  if (allowed && [super canPerformAction:@selector(paste:) withSender:sender]) {
    [super paste:sender];
  }
}

- (void)onCutAllowed:(BOOL)allowed sender:(id)sender {
  RecordClipboardOutcomeMetrics(ClipboardAction::kCut, allowed);
  if (allowed && [super canPerformAction:@selector(cut:) withSender:sender]) {
    [super cut:sender];
  }
}

- (void)onPasteAndMatchStyleAllowed:(BOOL)allowed sender:(id)sender {
  RecordClipboardOutcomeMetrics(ClipboardAction::kPaste, allowed);
  if (allowed && [super canPerformAction:@selector(pasteAndMatchStyle:)
                              withSender:sender]) {
    [super pasteAndMatchStyle:sender];
  }
}

- (void)onPasteAndSearchAllowed:(BOOL)allowed sender:(id)sender {
  RecordClipboardOutcomeMetrics(ClipboardAction::kPaste, allowed);
  if (allowed && [super canPerformAction:@selector(pasteAndSearch:)
                              withSender:sender]) {
    [super pasteAndSearch:sender];
  }
}

- (void)onPasteAndGoAllowed:(BOOL)allowed sender:(id)sender {
  RecordClipboardOutcomeMetrics(ClipboardAction::kPaste, allowed);
  if (allowed && [super canPerformAction:@selector(pasteAndGo:)
                              withSender:sender]) {
    [super pasteAndGo:sender];
  }
}

@end
