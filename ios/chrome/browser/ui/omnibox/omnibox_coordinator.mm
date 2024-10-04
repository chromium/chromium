// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_coordinator.h"

#import "base/check.h"
#import "base/ios/ios_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/omnibox/browser/omnibox_controller.h"
#import "components/omnibox/browser/omnibox_edit_model.h"
#import "components/omnibox/common/omnibox_features.h"
#import "components/omnibox/common/omnibox_focus_state.h"
#import "components/open_from_clipboard/clipboard_recent_content.h"
#import "components/search_engines/template_url_service.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_constants.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/shared/public/commands/lens_commands.h"
#import "ios/chrome/browser/shared/public/commands/load_query_commands.h"
#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"
#import "ios/chrome/browser/shared/public/commands/qr_scanner_commands.h"
#import "ios/chrome/browser/shared/public/commands/toolbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/ui/omnibox/keyboard_assist/omnibox_assistive_keyboard_delegate.h"
#import "ios/chrome/browser/ui/omnibox/keyboard_assist/omnibox_assistive_keyboard_mediator.h"
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
#import "ios/chrome/browser/ui/omnibox/text_field_view_containing.h"
#import "ios/chrome/browser/ui/omnibox/zero_suggest_prefetch_helper.h"
#import "ios/chrome/browser/url_loading/model/image_search_param_generator.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/web/public/navigation/navigation_manager.h"

@interface OmniboxCoordinator () <OmniboxViewControllerTextInputDelegate,
                                  OmniboxAssistiveKeyboardMediatorDelegate>

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
  // TODO(crbug.com/40565663): use a slimmer subclass of OmniboxView,
  // OmniboxPopupViewSuggestionsDelegate instead of OmniboxViewIOS.
  std::unique_ptr<OmniboxViewIOS> _editView;

  // Omnibox client. Stored between init and start, then ownership is passed to
  // OmniboxView.
  std::unique_ptr<OmniboxClient> _client;

  /// Object handling interactions in the keyboard accessory view.
  OmniboxAssistiveKeyboardMediator* _keyboardMediator;

  // The handler for ToolbarCommands.
  id<ToolbarCommands> _toolbarHandler;

  // Whether it's the lens overlay omnibox.
  BOOL _isLensOverlay;
}
@synthesize viewController = _viewController;
@synthesize mediator = _mediator;

#pragma mark - public

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                             omniboxClient:
                                 (std::unique_ptr<OmniboxClient>)client
                             isLensOverlay:(BOOL)isLensOverlay {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _client = std::move(client);
    _isLensOverlay = isLensOverlay;
  }
  return self;
}

- (void)start {
  DCHECK(!self.popupCoordinator);

  _toolbarHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), ToolbarCommands);

  self.viewController =
      [[OmniboxViewController alloc] initWithIsLensOverlay:_isLensOverlay];
  self.viewController.defaultLeadingImage =
      GetOmniboxSuggestionIcon(OmniboxSuggestionIconType::kDefaultFavicon);
  self.viewController.textInputDelegate = self;
  self.viewController.layoutGuideCenter =
      LayoutGuideCenterForBrowser(self.browser);
  self.viewController.isSearchOnlyUI = self.isSearchOnlyUI;

  BOOL isIncognito = self.browser->GetProfile()->IsOffTheRecord();
  self.mediator = [[OmniboxMediator alloc]
      initWithIncognito:isIncognito
                tracker:feature_engagement::TrackerFactory::GetForBrowserState(
                            self.browser->GetProfile())
          isLensOverlay:_isLensOverlay];

  TemplateURLService* templateURLService =
      ios::TemplateURLServiceFactory::GetForBrowserState(
          self.browser->GetProfile());
  self.mediator.templateURLService = templateURLService;
  self.mediator.faviconLoader =
      IOSChromeFaviconLoaderFactory::GetForBrowserState(
          self.browser->GetProfile());
  self.mediator.consumer = self.viewController;
  self.mediator.omniboxCommandsHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), OmniboxCommands);
  self.mediator.lensCommandsHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), LensCommands);
  self.mediator.loadQueryCommandsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), LoadQueryCommands);
  self.mediator.sceneState = self.browser->GetSceneState();
  self.mediator.URLLoadingBrowserAgent =
      UrlLoadingBrowserAgent::FromBrowser(self.browser);
  self.viewController.pasteDelegate = self.mediator;

  DCHECK(_client.get());

  id<OmniboxCommands> omniboxHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), OmniboxCommands);
  _editView = std::make_unique<OmniboxViewIOS>(
      self.textField, std::move(_client), self.browser->GetProfile(),
      omniboxHandler, self.focusDelegate, _toolbarHandler, self.viewController,
      _isLensOverlay);
  self.pasteDelegate = [[OmniboxTextFieldPasteDelegate alloc] init];
  [self.textField setPasteDelegate:self.pasteDelegate];

  self.viewController.textChangeDelegate = _editView.get();

  _keyboardMediator = [[OmniboxAssistiveKeyboardMediator alloc] init];
  _keyboardMediator.applicationCommandsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  _keyboardMediator.lensCommandsHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), LensCommands);
  _keyboardMediator.qrScannerCommandsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), QRScannerCommands);
  _keyboardMediator.layoutGuideCenter =
      LayoutGuideCenterForBrowser(self.browser);
  // TODO(crbug.com/40670043): Use HandlerForProtocol after commands protocol
  // clean up.
  _keyboardMediator.browserCoordinatorCommandsHandler =
      static_cast<id<BrowserCoordinatorCommands>>(
          self.browser->GetCommandDispatcher());
  _keyboardMediator.omniboxTextField = self.textField;
  _keyboardMediator.delegate = self;

  if (base::FeatureList::IsEnabled(omnibox::kZeroSuggestPrefetching)) {
    self.zeroSuggestPrefetchHelper = [[ZeroSuggestPrefetchHelper alloc]
        initWithWebStateList:self.browser->GetWebStateList()
                  controller:_editView->controller()];
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
  self.viewController = nil;
  self.mediator.templateURLService = nullptr;  // Unregister the observer.
  if (self.keyboardAccessoryView) {
    // Unregister the observer.
    self.keyboardAccessoryView.templateURLService = nil;
  }

  _keyboardMediator = nil;
  self.keyboardAccessoryView = nil;
  self.mediator = nil;
  self.returnDelegate = nil;
  [self.zeroSuggestPrefetchHelper disconnect];
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
  if (!self.keyboardAccessoryView &&
      (!self.isSearchOnlyUI ||
       experimental_flags::IsOmniboxDebuggingEnabled())) {
    TemplateURLService* templateURLService =
        ios::TemplateURLServiceFactory::GetForBrowserState(
            self.browser->GetProfile());
    self.keyboardAccessoryView = ConfigureAssistiveKeyboardViews(
        self.textField, kDotComTLD, _keyboardMediator, templateURLService,
        HandlerForProtocol(self.browser->GetCommandDispatcher(), HelpCommands));
  }

  if (![self.textField isFirstResponder]) {
    base::RecordAction(base::UserMetricsAction("MobileOmniboxFocused"));

    // In multiwindow context, -becomeFirstRepsonder is not enough to get the
    // keyboard input. The window will not automatically become key. Make it key
    // manually. UITextField does this under the hood when tapped from
    // -[UITextInteractionAssistant(UITextInteractionAssistant_Internal)
    // setFirstResponderIfNecessaryActivatingSelection:]
    if (base::ios::IsMultipleScenesSupported()) {
      [self.textField.window makeKeyAndVisible];
    }

    [self.textField becomeFirstResponder];
    // Ensures that the accessibility system focuses the text field instead of
    // the popup crbug.com/1469173.
    UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification,
                                    self.textField);
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
      std::make_unique<OmniboxPopupViewIOS>(_editView->controller(),
                                            _editView.get());

  _editView->SetPopupProvider(popupView.get());

  OmniboxPopupCoordinator* coordinator = [[OmniboxPopupCoordinator alloc]
      initWithBaseViewController:nil
                         browser:self.browser
          autocompleteController:_editView->controller()
                                     ->autocomplete_controller()
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

- (id<ToolbarOmniboxConsumer>)toolbarOmniboxConsumer {
  return self.popupCoordinator.toolbarOmniboxConsumer;
}

- (UIView<TextFieldViewContaining>*)editView {
  return self.viewController.viewContainingTextField;
}

- (void)setThumbnailImage:(UIImage*)image {
  if (_editView) {
    _editView->SetThumbnailImage(image);
  }
}

#pragma mark Scribble

- (void)focusOmniboxForScribble {
  [self.textField becomeFirstResponder];
  [self.viewController prepareOmniboxForScribble];
}

- (UIResponder<UITextInput>*)scribbleInput {
  return self.viewController.textField;
}

#pragma mark - OmniboxAssistiveKeyboardMediatorDelegate

- (void)omniboxAssistiveKeyboardDidTapDebuggerButton {
  [self.popupCoordinator toggleOmniboxDebuggerView];
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
