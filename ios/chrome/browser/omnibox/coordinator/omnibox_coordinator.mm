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
#import "ios/chrome/browser/autocomplete/model/autocomplete_browser_agent.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_constants.h"
#import "ios/chrome/browser/omnibox/coordinator/omnibox_mediator.h"
#import "ios/chrome/browser/omnibox/coordinator/omnibox_mediator_delegate.h"
#import "ios/chrome/browser/omnibox/coordinator/popup/omnibox_popup_coordinator.h"
#import "ios/chrome/browser/omnibox/model/omnibox_autocomplete_controller.h"
#import "ios/chrome/browser/omnibox/model/omnibox_metrics_recorder.h"
#import "ios/chrome/browser/omnibox/model/omnibox_text_controller.h"
#import "ios/chrome/browser/omnibox/model/omnibox_text_model.h"
#import "ios/chrome/browser/omnibox/model/placeholder_service/placeholder_service.h"
#import "ios/chrome/browser/omnibox/model/placeholder_service/placeholder_service_factory.h"
#import "ios/chrome/browser/omnibox/model/suggestions/autocomplete_result_wrapper.h"
#import "ios/chrome/browser/omnibox/model/suggestions/omnibox_pedal_annotator.h"
#import "ios/chrome/browser/omnibox/public/omnibox_presentation_context.h"
#import "ios/chrome/browser/omnibox/public/omnibox_util.h"
#import "ios/chrome/browser/omnibox/ui/keyboard_assist/omnibox_assistive_keyboard_delegate.h"
#import "ios/chrome/browser/omnibox/ui/keyboard_assist/omnibox_assistive_keyboard_mediator.h"
#import "ios/chrome/browser/omnibox/ui/keyboard_assist/omnibox_assistive_keyboard_views.h"
#import "ios/chrome/browser/omnibox/ui/keyboard_assist/omnibox_keyboard_accessory_view.h"
#import "ios/chrome/browser/omnibox/ui/omnibox_text_field_ios.h"
#import "ios/chrome/browser/omnibox/ui/omnibox_text_field_paste_delegate.h"
#import "ios/chrome/browser/omnibox/ui/omnibox_text_input.h"
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

// The keyboard accessory view. Will be nil if the app is running on an iPad.
@property(nonatomic, strong)
    OmniboxKeyboardAccessoryView* keyboardAccessoryView;

// Redefined as readwrite.
@property(nonatomic, strong) OmniboxPopupCoordinator* popupCoordinator;

@end

@implementation OmniboxCoordinator {
  /// Omnibox client.
  std::unique_ptr<OmniboxClient> _client;

  // OmniboxCoordinator temporarely owns these class until they are removed
  // after the refactoring crbug.com/390409559.
  std::unique_ptr<OmniboxTextModel> _omniboxTextModel;

  /// Controller for the omnibox autocomplete.
  OmniboxAutocompleteController* _omniboxAutocompleteController;
  /// Controller for the omnibox text.
  OmniboxTextController* _omniboxTextController;

  /// Metrics recorder.
  OmniboxMetricsRecorder* _omniboxMetricsRecorder;

  /// Object handling interactions in the keyboard accessory view.
  OmniboxAssistiveKeyboardMediator* _keyboardMediator;

  // The handler for ToolbarCommands.
  id<ToolbarCommands> _toolbarHandler;

  // The context in which the omnibox is presented.
  OmniboxPresentationContext _presentationContext;
}
@synthesize viewController = _viewController;
@synthesize mediator = _mediator;

#pragma mark - public

- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
                 omniboxClient:(std::unique_ptr<OmniboxClient>)client
           presentationContext:(OmniboxPresentationContext)presentationContext {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _client = std::move(client);
    _presentationContext = presentationContext;
  }
  return self;
}

- (void)start {
  DCHECK(!self.popupCoordinator);

  ProfileIOS* profile = self.profile;
  Browser* browser = self.browser;

  _toolbarHandler =
      HandlerForProtocol(browser->GetCommandDispatcher(), ToolbarCommands);

  OmniboxViewController* viewController = [[OmniboxViewController alloc]
      initWithPresentationContext:_presentationContext];
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
      presentationContext:_presentationContext];
  self.mediator = mediator;

  mediator.delegate = self;
  TemplateURLService* templateURLService =
      ios::TemplateURLServiceFactory::GetForProfile(profile);
  mediator.templateURLService = templateURLService;
  PlaceholderService* placeholderService =
      ios::PlaceholderServiceFactory::GetForProfile(profile);
  mediator.placeholderService = placeholderService;
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
  viewController.mutator = mediator;

  DCHECK(_client.get());

  _omniboxTextModel = std::make_unique<OmniboxTextModel>(_client.get());
  id<OmniboxTextInput> textInput = viewController.textInput;

  AutocompleteBrowserAgent* autocompleteBrowserAgent =
      AutocompleteBrowserAgent::FromBrowser(browser);
  AutocompleteController* autocompleteController =
      autocompleteBrowserAgent->GetAutocompleteController(_presentationContext);

  _omniboxAutocompleteController = [[OmniboxAutocompleteController alloc]
       initWithOmniboxClient:_client.get()
      autocompleteController:autocompleteController
            omniboxTextModel:_omniboxTextModel.get()
         presentationContext:_presentationContext];

  _omniboxMetricsRecorder =
      [[OmniboxMetricsRecorder alloc] initWithClient:_client.get()
                                           textModel:_omniboxTextModel.get()];
  viewController.metricsRecorder = _omniboxMetricsRecorder;
  [_omniboxMetricsRecorder setAutocompleteController:autocompleteController];

  self.pasteDelegate = [[OmniboxTextFieldPasteDelegate alloc] init];
  [textInput setPasteDelegate:self.pasteDelegate];
  self.pasteDelegate.textInput = textInput;

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
  _keyboardMediator.omniboxTextInput = textInput;
  _keyboardMediator.delegate = self;

  _omniboxTextController = [[OmniboxTextController alloc]
      initWithOmniboxClient:_client.get()
           omniboxTextModel:_omniboxTextModel.get()
        presentationContext:_presentationContext];
  _omniboxTextController.delegate = mediator;
  _omniboxTextController.focusDelegate = self.focusDelegate;
  _omniboxTextController.omniboxAutocompleteController =
      _omniboxAutocompleteController;
  _omniboxTextController.textInput = textInput;
  _omniboxAutocompleteController.omniboxTextController = _omniboxTextController;
  _omniboxAutocompleteController.omniboxMetricsRecorder =
      _omniboxMetricsRecorder;
  _omniboxAutocompleteController.lensHander = self.mediator;

  _omniboxMetricsRecorder.omniboxAutocompleteController =
      _omniboxAutocompleteController;

  mediator.omniboxTextController = _omniboxTextController;

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
               initWithOmniboxClient:_client.get()
          autocompleteProviderClient:_omniboxAutocompleteController
                                         .autocompleteProviderClient];
  autocompleteResultWrapper.pedalAnnotator = annotator;
  autocompleteResultWrapper.presentationContext = _presentationContext;
  autocompleteResultWrapper.templateURLService = templateURLService;
  autocompleteResultWrapper.incognito = incognito;
  autocompleteResultWrapper.delegate = _omniboxAutocompleteController;

  _omniboxAutocompleteController.autocompleteResultWrapper =
      autocompleteResultWrapper;
  autocompleteResultWrapper.delegate = _omniboxAutocompleteController;

  self.popupCoordinator = [self createPopupCoordinator:self.presenterDelegate];
  [self.popupCoordinator start];
  if (IsMultilineBrowserOmniboxEnabled()) {
    // Pre-render the input accessory view to make sure it shows on first launch
    // crbug.com/458003863.
    [self updateInputAccessoryView];
  }
}

- (void)stop {
  [self.popupCoordinator stop];
  self.popupCoordinator = nil;

  _omniboxTextModel.reset();
  _client.reset();

  self.viewController = nil;
  self.mediator.templateURLService = nullptr;  // Unregister the observer.
  if (self.keyboardAccessoryView) {
    // Unregister the observer.
    self.keyboardAccessoryView.templateURLService = nil;
  }

  _keyboardMediator.delegate = nil;
  _keyboardMediator = nil;
  self.keyboardAccessoryView = nil;
  self.mediator = nil;
  [_omniboxAutocompleteController disconnect];
  _omniboxAutocompleteController = nil;
  [_omniboxTextController disconnect];
  _omniboxTextController = nil;
  [_omniboxMetricsRecorder disconnect];
  _omniboxMetricsRecorder = nil;
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

- (void)acceptInput {
  [self.mediator acceptInput];
}

- (void)insertTextToOmnibox:(NSString*)text {
  [_omniboxTextController insertTextToOmnibox:text];
}

- (OmniboxPopupCoordinator*)createPopupCoordinator:
    (id<OmniboxPopupPresenterDelegate>)presenterDelegate {
  DCHECK(!_popupCoordinator);

  OmniboxPopupCoordinator* coordinator = [[OmniboxPopupCoordinator alloc]
         initWithBaseViewController:nil
                            browser:self.browser
             autocompleteController:[_omniboxAutocompleteController
                                        autocompleteController]
      omniboxAutocompleteController:_omniboxAutocompleteController
                presentationContext:_presentationContext];
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

- (void)clearSuggestionsWithRestartAutocomplete:(BOOL)restartAutocomplete {
  [_omniboxTextController removePreEditText];
  if (restartAutocomplete) {
    [_omniboxAutocompleteController clearAndRestartAutocomplete];
  } else {
    [_omniboxAutocompleteController stopAutocompleteWithClearSuggestions:YES];
  }
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
  return [self.viewController.textInput scribbleInput];
}

#pragma mark - OmniboxAssistiveKeyboardMediatorDelegate

- (void)omniboxAssistiveKeyboardDidTapDebuggerButton {
  [self.popupCoordinator toggleOmniboxDebuggerView];
}

- (void)presentLensKeyboardInProductHelper {
  id<HelpCommands> helpHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), HelpCommands);
  [helpHandler presentInProductHelpWithType:InProductHelpType::kLensKeyboard];
}

#pragma mark - OmniboxMediatorDelegate

- (void)omniboxMediatorDidBeginEditing:(OmniboxMediator*)mediator {
  [self updateInputAccessoryView];
}

#pragma mark - Private

- (void)updateInputAccessoryView {
  BOOL showKeyboardAccessory =
      experimental_flags::IsOmniboxDebuggingEnabled() ||
      (!self.searchOnlyUI &&
       _presentationContext != OmniboxPresentationContext::kComposebox);

  if (!self.keyboardAccessoryView && showKeyboardAccessory) {
    TemplateURLService* templateURLService =
        ios::TemplateURLServiceFactory::GetForProfile(self.profile);
    self.keyboardAccessoryView = ConfigureAssistiveKeyboardViews(
        self.viewController.textInput, kDotComTLD, _keyboardMediator,
        templateURLService);
  }
}

#pragma mark - Testing

- (OmniboxAutocompleteController*)omniboxAutocompleteController {
  return _omniboxAutocompleteController;
}

@end
