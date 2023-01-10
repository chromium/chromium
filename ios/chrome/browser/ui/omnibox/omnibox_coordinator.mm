// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_coordinator.h"

#import "base/check.h"
#import "base/ios/ios_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/omnibox/browser/omnibox_edit_model.h"
#import "components/omnibox/common/omnibox_features.h"
#import "components/omnibox/common/omnibox_focus_state.h"
#import "components/open_from_clipboard/clipboard_recent_content.h"
#import "components/search_engines/template_url_service.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/lens_commands.h"
#import "ios/chrome/browser/ui/commands/load_query_commands.h"
#import "ios/chrome/browser/ui/commands/omnibox_commands.h"
#import "ios/chrome/browser/ui/commands/qr_scanner_commands.h"
#import "ios/chrome/browser/ui/commands/thumb_strip_commands.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_promo_non_modal_scheduler.h"
#import "ios/chrome/browser/ui/gestures/view_revealing_animatee.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_constants.h"
#import "ios/chrome/browser/ui/main/default_browser_scene_agent.h"
#import "ios/chrome/browser/ui/main/layout_guide_util.h"
#import "ios/chrome/browser/ui/main/scene_state_browser_agent.h"
#import "ios/chrome/browser/ui/omnibox/keyboard_assist/omnibox_assistive_keyboard_delegate.h"
#import "ios/chrome/browser/ui/omnibox/keyboard_assist/omnibox_assistive_keyboard_views.h"
#import "ios/chrome/browser/ui/omnibox/keyboard_assist/omnibox_keyboard_accessory_view.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_mediator.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_return_key_forwarding_delegate.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_ios.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_paste_delegate.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_util.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_view_controller.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_view_ios.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_coordinator.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_view_ios.h"
#import "ios/chrome/browser/ui/omnibox/popup/pedal_section_extractor.h"
#import "ios/chrome/browser/ui/omnibox/zero_suggest_prefetch_helper.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/url_loading/image_search_param_generator.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/web/public/navigation/navigation_manager.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface OmniboxCoordinator () <OmniboxViewControllerTextInputDelegate>
// Object taking care of adding the accessory views to the keyboard.
@property(nonatomic, strong)
    OmniboxAssistiveKeyboardDelegateImpl* keyboardDelegate;

// View controller managed by this coordinator.
@property(nonatomic, strong) OmniboxViewController* viewController;

// The mediator for the omnibox.
@property(nonatomic, strong) OmniboxMediator* mediator;

// The paste delegate for the omnibox that prevents multipasting.
@property(nonatomic, strong) OmniboxTextFieldPasteDelegate* pasteDelegate;

// The return delegate.
@property(nonatomic, strong) ForwardingReturnDelegate* returnDelegate;

// Helper that starts ZPS prefetch when the user opens a NTP.
@property(nonatomic, strong)
    ZeroSuggestPrefetchHelper* zeroSuggestPrefetchHelper;

// The keyboard accessory view. Will be nil if the app is running on an iPad.
@property(nonatomic, strong)
    OmniboxKeyboardAccessoryView* keyboardAccessoryView;

// Redefined as readwrite.
@property(nonatomic, strong) OmniboxPopupCoordinator* popupCoordinator;

@end

@implementation OmniboxCoordinator {
  // TODO(crbug.com/818636): use a slimmer subclass of OmniboxView,
  // OmniboxPopupViewSuggestionsDelegate instead of OmniboxViewIOS.
  std::unique_ptr<OmniboxViewIOS> _editView;
}
@synthesize editController = _editController;
@synthesize keyboardDelegate = _keyboardDelegate;
@synthesize viewController = _viewController;
@synthesize mediator = _mediator;

#pragma mark - public

- (void)start {
  DCHECK(!self.popupCoordinator);

  BOOL isIncognito = self.browser->GetBrowserState()->IsOffTheRecord();

  self.viewController =
      [[OmniboxViewController alloc] initWithIncognito:isIncognito];

  self.viewController.defaultLeadingImage =
      GetOmniboxSuggestionIcon(OmniboxSuggestionIconType::kDefaultFavicon);
  self.viewController.textInputDelegate = self;
  self.mediator = [[OmniboxMediator alloc] initWithIncognito:isIncognito];

  TemplateURLService* templateURLService =
      ios::TemplateURLServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  self.mediator.templateURLService = templateURLService;
  self.mediator.faviconLoader =
      IOSChromeFaviconLoaderFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  self.mediator.consumer = self.viewController;
  self.mediator.omniboxCommandsHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), OmniboxCommands);
  self.mediator.lensCommandsHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), LensCommands);
  self.mediator.loadQueryCommandsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), LoadQueryCommands);
  self.mediator.sceneState =
      SceneStateBrowserAgent::FromBrowser(self.browser)->GetSceneState();
  self.mediator.URLLoadingBrowserAgent =
      UrlLoadingBrowserAgent::FromBrowser(self.browser);
  self.viewController.pasteDelegate = self.mediator;

  DCHECK(self.editController);

  id<OmniboxCommands> focuser =
      static_cast<id<OmniboxCommands>>(self.browser->GetCommandDispatcher());
  _editView = std::make_unique<OmniboxViewIOS>(
      self.textField, self.editController, self.browser->GetBrowserState(),
      focuser);
  self.pasteDelegate = [[OmniboxTextFieldPasteDelegate alloc] init];
  [self.textField setPasteDelegate:self.pasteDelegate];

  self.viewController.textChangeDelegate = _editView.get();

  self.keyboardDelegate = [[OmniboxAssistiveKeyboardDelegateImpl alloc] init];
  self.keyboardDelegate.applicationCommandsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  self.keyboardDelegate.lensCommandsHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), LensCommands);
  self.keyboardDelegate.qrScannerCommandsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), QRScannerCommands);
  self.keyboardDelegate.layoutGuideCenter =
      LayoutGuideCenterForBrowser(self.browser);
  // TODO(crbug.com/1045047): Use HandlerForProtocol after commands protocol
  // clean up.
  self.keyboardDelegate.browserCommandsHandler =
      static_cast<id<BrowserCommands>>(self.browser->GetCommandDispatcher());
  self.keyboardDelegate.omniboxTextField = self.textField;
  self.keyboardAccessoryView = ConfigureAssistiveKeyboardViews(
      self.textField, kDotComTLD, self.keyboardDelegate, templateURLService);

  if (base::FeatureList::IsEnabled(omnibox::kZeroSuggestPrefetching)) {
    self.zeroSuggestPrefetchHelper = [[ZeroSuggestPrefetchHelper alloc]
          initWithWebStateList:self.browser->GetWebStateList()
        autocompleteController:_editView->model()->autocomplete_controller()];
  }

  self.popupCoordinator = [self createPopupCoordinator:self.presenterDelegate];
  [self.popupCoordinator start];
}

- (void)stop {
  [self.popupCoordinator stop];
  self.popupCoordinator = nil;

  self.viewController.textChangeDelegate = nil;
  self.returnDelegate.acceptDelegate = nil;
  _editView.reset();
  self.editController = nil;
  self.viewController = nil;
  self.mediator.templateURLService = nullptr;  // Unregister the observer.
  if (self.keyboardAccessoryView) {
    // Unregister the observer.
    self.keyboardAccessoryView.templateURLService = nil;
  }

  self.keyboardAccessoryView = nil;
  self.mediator = nil;
  self.returnDelegate = nil;
  self.zeroSuggestPrefetchHelper = nil;

  [NSNotificationCenter.defaultCenter removeObserver:self];
}

- (void)updateOmniboxState {
  _editView->UpdateAppearance();
}

- (BOOL)isOmniboxFirstResponder {
  return [self.textField isFirstResponder];
}

- (void)focusOmnibox {
  if (![self.textField isFirstResponder]) {
    base::RecordAction(base::UserMetricsAction("MobileOmniboxFocused"));

    // Thumb strip is not necessarily active, so only close it if it is active.
    if ([self.browser->GetCommandDispatcher()
            dispatchingForProtocol:@protocol(ThumbStripCommands)]) {
      id<ThumbStripCommands> thumbStripHandler = HandlerForProtocol(
          self.browser->GetCommandDispatcher(), ThumbStripCommands);
      [thumbStripHandler
          closeThumbStripWithTrigger:ViewRevealTrigger::OmniboxFocus];
    }

    // In multiwindow context, -becomeFirstRepsonder is not enough to get the
    // keyboard input. The window will not automatically become key. Make it key
    // manually. UITextField does this under the hood when tapped from
    // -[UITextInteractionAssistant(UITextInteractionAssistant_Internal)
    // setFirstResponderIfNecessaryActivatingSelection:]
    if (base::ios::IsMultipleScenesSupported()) {
      [self.textField.window makeKeyAndVisible];
    }

    [self.textField becomeFirstResponder];
  }
}

- (void)endEditing {
  [self.textField resignFirstResponder];
  _editView->EndEditing();
}

- (void)insertTextToOmnibox:(NSString*)text {
  [self.textField insertTextWhileEditing:text];
  // The call to `setText` shouldn't be needed, but without it the "Go" button
  // of the keyboard is disabled.
  [self.textField setText:text];
  // Notify the accessibility system to start reading the new contents of the
  // Omnibox.
  UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification,
                                  self.textField);
}

- (OmniboxPopupCoordinator*)createPopupCoordinator:
    (id<OmniboxPopupPresenterDelegate>)presenterDelegate {
  DCHECK(!_popupCoordinator);
  std::unique_ptr<OmniboxPopupViewIOS> popupView =
      std::make_unique<OmniboxPopupViewIOS>(_editView->model(),
                                            _editView.get());

  _editView->SetPopupProvider(popupView.get());

  OmniboxPopupCoordinator* coordinator = [[OmniboxPopupCoordinator alloc]
      initWithBaseViewController:nil
                         browser:self.browser
          autocompleteController:_editView->model()->autocomplete_controller()
                       popupView:std::move(popupView)];
  coordinator.presenterDelegate = presenterDelegate;

  self.returnDelegate = [[ForwardingReturnDelegate alloc] init];
  self.returnDelegate.acceptDelegate = _editView.get();

  coordinator.popupMatchPreviewDelegate = self.mediator;
  coordinator.acceptReturnDelegate = self.returnDelegate;
  self.viewController.returnKeyDelegate = coordinator.popupReturnDelegate;
  self.viewController.popupKeyboardDelegate = coordinator.KeyboardDelegate;

  _popupCoordinator = coordinator;
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

#pragma mark Scribble

- (void)focusOmniboxForScribble {
  [self.textField becomeFirstResponder];
  [self.viewController prepareOmniboxForScribble];
}

- (UIResponder<UITextInput>*)scribbleInput {
  return self.viewController.textField;
}

#pragma mark - OmniboxViewControllerTextInputDelegate

- (void)omniboxViewControllerTextInputModeDidChange:
    (OmniboxViewController*)omniboxViewController {
  _editView->UpdatePopupAppearance();
}

#pragma mark - private

// Convenience accessor.
- (OmniboxTextFieldIOS*)textField {
  return self.viewController.textField;
}

@end
