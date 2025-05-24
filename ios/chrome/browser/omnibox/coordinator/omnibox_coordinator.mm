// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/coordinator/omnibox_coordinator.h"

#import "base/check.h"
#import "base/ios/ios_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/omnibox/browser/omnibox_client.h"
#import "components/omnibox/common/omnibox_features.h"
#import "components/omnibox/common/omnibox_focus_state.h"
#import "components/open_from_clipboard/clipboard_recent_content.h"
#import "components/search_engines/template_url_service.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_constants.h"
#import "ios/chrome/browser/omnibox/coordinator/omnibox_mediator.h"
#import "ios/chrome/browser/omnibox/coordinator/omnibox_mediator_delegate.h"
#import "ios/chrome/browser/omnibox/coordinator/popup/omnibox_popup_coordinator.h"
#import "ios/chrome/browser/omnibox/coordinator/zero_suggest_prefetch_helper.h"
#import "ios/chrome/browser/omnibox/model/autocomplete_result_wrapper.h"
#import "ios/chrome/browser/omnibox/model/omnibox_autocomplete_controller.h"
#import "ios/chrome/browser/omnibox/model/omnibox_controller_ios.h"
#import "ios/chrome/browser/omnibox/model/omnibox_edit_model_ios.h"
#import "ios/chrome/browser/omnibox/model/omnibox_pedal_annotator.h"
#import "ios/chrome/browser/omnibox/model/omnibox_popup_view_ios.h"
#import "ios/chrome/browser/omnibox/model/omnibox_text_controller.h"
#import "ios/chrome/browser/omnibox/model/omnibox_view_ios.h"
#import "ios/chrome/browser/omnibox/public/omnibox_util.h"
#import "ios/chrome/browser/omnibox/ui/keyboard_assist/omnibox_assistive_keyboard_delegate.h"
#import "ios/chrome/browser/omnibox/ui/keyboard_assist/omnibox_assistive_keyboard_mediator.h"
#import "ios/chrome/browser/omnibox/ui/keyboard_assist/omnibox_assistive_keyboard_views.h"
#import "ios/chrome/browser/omnibox/ui/keyboard_assist/omnibox_keyboard_accessory_view.h"
#import "ios/chrome/browser/omnibox/ui/omnibox_text_field_ios.h"
#import "ios/chrome/browser/omnibox/ui/omnibox_text_field_paste_delegate.h"
#import "ios/chrome/browser/omnibox/ui/omnibox_view_controller.h"
#import "ios/chrome/browser/omnibox/ui/text_field_view_containing.h"
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
#import "ios/chrome/browser/shared/public/commands/quick_delete_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/toolbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/url_loading/model/image_search_param_generator.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/web/public/navigation/navigation_manager.h"

@interface OmniboxCoordinator () <OmniboxAssistiveKeyboardMediatorDelegate,
                                  OmniboxMediatorDelegate>

// View controller managed by this coordinator.
@property(nonatomic, strong) OmniboxViewController* viewController;

// The mediator for the omnibox.
@property(nonatomic, strong) OmniboxMediator* mediator;

// The paste delegate for the omnibox that prevents multipasting.
@property(nonatomic, strong) OmniboxTextFieldPasteDelegate* pasteDelegate;

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

  /// Controller for the omnibox autocomplete.
  OmniboxAutocompleteController* _omniboxAutocompleteController;
  /// Controller for the omnibox text.
  OmniboxTextController* _omniboxTextController;

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

  ProfileIOS* profile = self.profile;
  Browser* browser = self.browser;

  _toolbarHandler =
      HandlerForProtocol(browser->GetCommandDispatcher(), ToolbarCommands);

  OmniboxViewController* viewController =
      [[OmniboxViewController alloc] initWithIsLensOverlay:_isLensOverlay];
  self.viewController = viewController;
  viewController.defaultLeadingImage =
      GetOmniboxSuggestionIcon(OmniboxSuggestionIconType::kDefaultFavicon);
  viewController.layoutGuideCenter = LayoutGuideCenterForBrowser(browser);
  viewController.searchOnlyUI = self.searchOnlyUI;

  BOOL incognito = profile->IsOffTheRecord();
  OmniboxMediator* mediator = [[OmniboxMediator alloc]
      initWithIncognito:incognito
                tracker:feature_engagement::TrackerFactory::GetForProfile(
                            profile)
          isLensOverlay:_isLensOverlay];
  self.mediator = mediator;

  mediator.delegate = self;
  TemplateURLService* templateURLService =
      ios::TemplateURLServiceFactory::GetForProfile(profile);
  mediator.templateURLService = templateURLService;
  mediator.faviconLoader =
      IOSChromeFaviconLoaderFactory::GetForProfile(profile);
  mediator.consumer = viewController;
  mediator.omniboxCommandsHandler =
      HandlerForProtocol(browser->GetCommandDispatcher(), OmniboxCommands);
  mediator.lensCommandsHandler =
      HandlerForProtocol(browser->GetCommandDispatcher(), LensCommands);
  mediator.loadQueryCommandsHandler =
      HandlerForProtocol(browser->GetCommandDispatcher(), LoadQueryCommands);
  mediator.sceneState = browser->GetSceneState();
  mediator.URLLoadingBrowserAgent =
      UrlLoadingBrowserAgent::FromBrowser(browser);
  viewController.pasteDelegate = mediator;
  viewController.mutator = mediator;

  DCHECK(_client.get());

  OmniboxTextFieldIOS* textField = viewController.textField;
  id<OmniboxCommands> omniboxHandler =
      HandlerForProtocol(browser->GetCommandDispatcher(), OmniboxCommands);
  _editView = std::make_unique<OmniboxViewIOS>(
      textField, std::move(_client), profile, omniboxHandler, _toolbarHandler);
  self.pasteDelegate = [[OmniboxTextFieldPasteDelegate alloc] init];
  [textField setPasteDelegate:self.pasteDelegate];

  _keyboardMediator = [[OmniboxAssistiveKeyboardMediator alloc] init];
  _keyboardMediator.applicationCommandsHandler =
      HandlerForProtocol(browser->GetCommandDispatcher(), ApplicationCommands);
  _keyboardMediator.lensCommandsHandler =
      HandlerForProtocol(browser->GetCommandDispatcher(), LensCommands);
  _keyboardMediator.qrScannerCommandsHandler =
      HandlerForProtocol(browser->GetCommandDispatcher(), QRScannerCommands);
  _keyboardMediator.layoutGuideCenter = LayoutGuideCenterForBrowser(browser);
  // TODO(crbug.com/40670043): Use HandlerForProtocol after commands protocol
  // clean up.
  _keyboardMediator.browserCoordinatorCommandsHandler =
      static_cast<id<BrowserCoordinatorCommands>>(
          browser->GetCommandDispatcher());
  _keyboardMediator.omniboxTextField = textField;
  _keyboardMediator.delegate = self;

  self.zeroSuggestPrefetchHelper = [[ZeroSuggestPrefetchHelper alloc]
      initWithWebStateList:browser->GetWebStateList()
                controller:_editView->controller()];

  _omniboxAutocompleteController = [[OmniboxAutocompleteController alloc]
      initWithOmniboxController:_editView->controller()];

  _omniboxTextController = [[OmniboxTextController alloc]
      initWithOmniboxController:_editView->controller()
                 omniboxViewIOS:_editView.get()
                  inLensOverlay:_isLensOverlay];
  _omniboxTextController.delegate = mediator;
  _omniboxTextController.focusDelegate = self.focusDelegate;
  _omniboxTextController.omniboxAutocompleteController =
      _omniboxAutocompleteController;
  _omniboxTextController.textField = textField;
  _omniboxAutocompleteController.omniboxTextController = _omniboxTextController;

  mediator.omniboxTextController = _omniboxTextController;
  _editView->SetOmniboxTextController(_omniboxTextController);

  CommandDispatcher* dispatcher = browser->GetCommandDispatcher();
  OmniboxPedalAnnotator* annotator = [[OmniboxPedalAnnotator alloc] init];
  annotator.applicationHandler =
      HandlerForProtocol(dispatcher, ApplicationCommands);
  annotator.settingsHandler = HandlerForProtocol(dispatcher, SettingsCommands);
  annotator.omniboxHandler = HandlerForProtocol(dispatcher, OmniboxCommands);
  annotator.quickDeleteHandler =
      HandlerForProtocol(dispatcher, QuickDeleteCommands);

  AutocompleteResultWrapper* autocompleteResultWrapper =
      [[AutocompleteResultWrapper alloc]
          initWithOmniboxClient:_editView->controller()
                                    ? _editView->controller()->client()
                                    : nullptr];
  autocompleteResultWrapper.pedalAnnotator = annotator;
  autocompleteResultWrapper.templateURLService = templateURLService;
  autocompleteResultWrapper.incognito = incognito;
  autocompleteResultWrapper.delegate = _omniboxAutocompleteController;

  _omniboxAutocompleteController.autocompleteResultWrapper =
      autocompleteResultWrapper;
  autocompleteResultWrapper.delegate = _omniboxAutocompleteController;

  self.popupCoordinator = [self createPopupCoordinator:self.presenterDelegate];
  [self.popupCoordinator start];
}

- (void)stop {
  [_omniboxAutocompleteController disconnect];
  _omniboxAutocompleteController = nil;
  [_omniboxTextController disconnect];
  _omniboxTextController = nil;

  [self.popupCoordinator stop];
  self.popupCoordinator = nil;

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
  [self.zeroSuggestPrefetchHelper disconnect];
  self.zeroSuggestPrefetchHelper = nil;
}

- (void)updateOmniboxState {
  [_omniboxTextController updateAppearance];
}

- (BOOL)isOmniboxFirstResponder {
  return [_omniboxTextController isOmniboxFirstResponder];
}

- (void)focusOmnibox {
  [_omniboxTextController focusOmnibox];
}

- (void)endEditing {
  [_omniboxTextController endEditing];
}

- (void)insertTextToOmnibox:(NSString*)text {
  [_omniboxTextController insertTextToOmnibox:text];
}

- (OmniboxPopupCoordinator*)createPopupCoordinator:
    (id<OmniboxPopupPresenterDelegate>)presenterDelegate {
  DCHECK(!_popupCoordinator);
  std::unique_ptr<OmniboxPopupViewIOS> popupView =
      std::make_unique<OmniboxPopupViewIOS>(_editView->controller(),
                                            _omniboxAutocompleteController);

  OmniboxPopupCoordinator* coordinator = [[OmniboxPopupCoordinator alloc]
         initWithBaseViewController:nil
                            browser:self.browser
             autocompleteController:_editView->controller()
                                        ->autocomplete_controller()
                          popupView:std::move(popupView)
      omniboxAutocompleteController:_omniboxAutocompleteController];
  coordinator.presenterDelegate = presenterDelegate;

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
  [self.mediator setThumbnailImage:image];
}

#pragma mark Scribble

- (void)focusOmniboxForScribble {
  [_omniboxTextController focusOmnibox];
  [self.viewController prepareOmniboxForScribble];
}

- (UIResponder<UITextInput>*)scribbleInput {
  return self.viewController.textField;
}

#pragma mark - OmniboxAssistiveKeyboardMediatorDelegate

- (void)omniboxAssistiveKeyboardDidTapDebuggerButton {
  [self.popupCoordinator toggleOmniboxDebuggerView];
}

#pragma mark - OmniboxMediatorDelegate

- (void)omniboxMediatorDidBeginEditing:(OmniboxMediator*)mediator {
  if (!self.keyboardAccessoryView &&
      (!self.searchOnlyUI || experimental_flags::IsOmniboxDebuggingEnabled())) {
    TemplateURLService* templateURLService =
        ios::TemplateURLServiceFactory::GetForProfile(self.profile);
    self.keyboardAccessoryView = ConfigureAssistiveKeyboardViews(
        self.viewController.textField, kDotComTLD, _keyboardMediator,
        templateURLService,
        HandlerForProtocol(self.browser->GetCommandDispatcher(), HelpCommands));
  }
}

#pragma mark - Testing

- (OmniboxControllerIOS*)omniboxController {
  return _editView ? _editView->controller() : nullptr;
}

@end
