// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_coordinator.h"

#include "base/logging.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/commands/load_query_commands.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_constants.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_mediator.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_ios.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_util.h"
#include "ios/chrome/browser/ui/omnibox/omnibox_view_controller.h"
#include "ios/chrome/browser/ui/omnibox/omnibox_view_ios.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_coordinator.h"
#include "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_view_ios.h"
#import "ios/chrome/browser/ui/toolbar/keyboard_assist/toolbar_assistive_keyboard_delegate.h"
#import "ios/chrome/browser/ui/toolbar/keyboard_assist/toolbar_assistive_keyboard_views.h"
#import "ios/chrome/browser/ui/toolbar/public/omnibox_focuser.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface OmniboxCoordinator ()
// Object taking care of adding the accessory views to the keyboard.
@property(nonatomic, strong)
    ToolbarAssistiveKeyboardDelegateImpl* keyboardDelegate;

// View controller managed by this coordinator.
@property(nonatomic, strong) OmniboxViewController* viewController;

// The mediator for the omnibox.
@property(nonatomic, strong) OmniboxMediator* mediator;

@end

@implementation OmniboxCoordinator {
  // TODO(crbug.com/818636): use a slimmer subclass of OmniboxView,
  // OmniboxPopupViewSuggestionsDelegate instead of OmniboxViewIOS.
  std::unique_ptr<OmniboxViewIOS> _editView;
}
@synthesize editController = _editController;
@synthesize browserState = _browserState;
@synthesize keyboardDelegate = _keyboardDelegate;
@synthesize dispatcher = _dispatcher;
@synthesize viewController = _viewController;
@synthesize mediator = _mediator;

#pragma mark - public

- (void)start {
  BOOL isIncognito = self.browserState->IsOffTheRecord();

  self.viewController =
      [[OmniboxViewController alloc] initWithIncognito:isIncognito];
  self.viewController.defaultLeadingImage =
      GetOmniboxSuggestionIcon(DEFAULT_FAVICON);
  self.viewController.emptyTextLeadingImage = GetOmniboxSuggestionIcon(SEARCH);

  self.viewController.dispatcher =
      static_cast<id<LoadQueryCommands, OmniboxFocuser>>(self.dispatcher);
  self.mediator = [[OmniboxMediator alloc] init];
  self.mediator.consumer = self.viewController;

  DCHECK(self.editController);
  _editView = std::make_unique<OmniboxViewIOS>(
      self.textField, self.editController, self.mediator, self.browserState);

  // Configure the textfield.
  self.textField.suggestionCommandsEndpoint =
      static_cast<id<OmniboxSuggestionCommands>>(self.dispatcher);

  self.keyboardDelegate = [[ToolbarAssistiveKeyboardDelegateImpl alloc] init];
  self.keyboardDelegate.dispatcher =
      static_cast<id<ApplicationCommands, BrowserCommands>>(self.dispatcher);
  self.keyboardDelegate.omniboxTextField = self.textField;
  ConfigureAssistiveKeyboardViews(self.textField, kDotComTLD,
                                  self.keyboardDelegate);
}

- (void)stop {
  _editView.reset();
  self.editController = nil;
  self.viewController = nil;
  self.mediator = nil;
}

- (void)updateOmniboxState {
  _editView->UpdateAppearance();
}

- (void)setNextFocusSourceAsSearchButton {
  OmniboxEditModel* model = _editView->model();
  model->set_focus_source(OmniboxEditModel::FocusSource::SEARCH_BUTTON);
}

- (BOOL)isOmniboxFirstResponder {
  return [self.textField isFirstResponder];
}

- (void)focusOmnibox {
  if (![self.textField isFirstResponder]) {
    base::RecordAction(base::UserMetricsAction("MobileOmniboxFocused"));
    [self.textField becomeFirstResponder];
  }
}

- (void)endEditing {
  _editView->HideKeyboardAndEndEditing();
}

- (void)insertTextToOmnibox:(NSString*)text {
  [self.textField insertTextWhileEditing:text];
  // The call to |setText| shouldn't be needed, but without it the "Go" button
  // of the keyboard is disabled.
  [self.textField setText:text];
  // Notify the accessibility system to start reading the new contents of the
  // Omnibox.
  UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification,
                                  self.textField);
}

- (OmniboxPopupCoordinator*)createPopupCoordinator:
    (id<OmniboxPopupPositioner>)positioner {
  std::unique_ptr<OmniboxPopupViewIOS> popupView =
      std::make_unique<OmniboxPopupViewIOS>(_editView->model(),
                                            _editView.get());

  _editView->model()->set_popup_model(popupView->model());
  _editView->SetPopupProvider(popupView.get());

  OmniboxPopupCoordinator* coordinator =
      [[OmniboxPopupCoordinator alloc] initWithPopupView:std::move(popupView)];
  coordinator.browserState = self.browserState;
  coordinator.positioner = positioner;

  return coordinator;
}

- (UIViewController*)managedViewController {
  return self.viewController;
}

- (id<LocationBarOffsetProvider>)offsetProvider {
  return self.viewController;
}

- (id<EditViewAnimatee>)animatee {
  return self.viewController;
}

#pragma mark - private

// Convenience accessor.
- (OmniboxTextFieldIOS*)textField {
  return self.viewController.textField;
}

@end
