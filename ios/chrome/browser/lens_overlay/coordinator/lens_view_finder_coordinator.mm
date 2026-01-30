// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/coordinator/lens_view_finder_coordinator.h"

#import "base/ios/block_types.h"
#import "base/not_fatal_until.h"
#import "base/strings/sys_string_conversions.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/prefs/pref_service.h"
#import "components/search_engines/template_url.h"
#import "components/search_engines/template_url_service.h"
#import "components/segmentation_platform/embedder/home_modules/tips_manager/signal_constants.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/device_orientation/ui_bundled/scoped_force_portrait_orientation.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/first_run/public/best_features_item.h"
#import "ios/chrome/browser/intents/model/intents_constants.h"
#import "ios/chrome/browser/intents/model/intents_donation_helper.h"
#import "ios/chrome/browser/lens/ui_bundled/lens_availability.h"
#import "ios/chrome/browser/lens/ui_bundled/lens_entrypoint.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_configuration_factory.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_entrypoint.h"
#import "ios/chrome/browser/lens_overlay/model/lens_view_finder_metrics_recorder.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_view_finder_transition_manager.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/lens_commands.h"
#import "ios/chrome/browser/shared/public/commands/lens_overlay_commands.h"
#import "ios/chrome/browser/shared/public/commands/new_tab_page_commands.h"
#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_lens_input_selection_command.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/commands/search_image_with_lens_command.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/tips_manager/model/tips_manager_ios.h"
#import "ios/chrome/browser/tips_manager/model/tips_manager_ios_factory.h"
#import "ios/chrome/browser/welcome_back/model/features.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/lens/lens_api.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Maps the presentation style to transition type.
LensViewFinderTransition TransitionFromPresentationStyle(
    LensInputSelectionPresentationStyle style) {
  switch (style) {
    case LensInputSelectionPresentationStyle::SlideFromLeft:
      return LensViewFinderTransitionSlideFromLeft;
    case LensInputSelectionPresentationStyle::SlideFromRight:
      return LensViewFinderTransitionSlideFromRight;
  }
}

}  // namespace

@interface LensViewFinderCoordinator () <
    LensCommands,
    ChromeLensViewFinderDelegate,
    UIViewControllerTransitioningDelegate,
    UIAdaptivePresentationControllerDelegate>

// Whether post capture view is shown.
@property(nonatomic, assign) BOOL postCaptureShown;

@end

@implementation LensViewFinderCoordinator {
  // The user interface to be presented.
  UIViewController<ChromeLensViewFinderController>* _lensViewController;

  // Manages the presenting & dismissal of the LVF user interface.
  LensViewFinderTransitionManager* _transitionManager;

  /// Forces the device orientation in portrait mode.
  std::unique_ptr<ScopedForcePortraitOrientation> _scopedForceOrientation;

  /// Records LVF related metrics.
  LensViewFinderMetricsRecorder* _metricsRecorder;

  // If set to YES, an IPH bubble will be presented on the NTP that points to
  // the Lens icon in the NTP fakebox, if Lens is dismissed by the user.
  BOOL _presentNTPLensIconBubbleOnDismiss;
}

@synthesize baseViewController = _baseViewController;

- (instancetype)initWithBrowser:(Browser*)browser {
  return [super initWithBaseViewController:nil browser:browser];
}

#pragma mark - ChromeCoordinator

- (void)start {
  _metricsRecorder = [[LensViewFinderMetricsRecorder alloc] init];
  [self.browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(LensCommands)];
  [self updateLensAvailabilityForWidgets];
  [self updateQRCodeOrLensAppShortcutItem];
}

- (void)stop {
  [self.browser->GetCommandDispatcher() stopDispatchingToTarget:self];
  [self lockOrientationPortrait:NO];
  _metricsRecorder = nil;
}

#pragma mark - LensCommands

- (void)searchImageWithLens:(SearchImageWithLensCommand*)command {
  id<LensOverlayCommands> lensOverlayHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), LensOverlayCommands);
  [lensOverlayHandler
          searchImageWithLens:command.image
                   entrypoint:LensOverlayEntrypoint::kSearchImageContextMenu
      initialPresentationBase:_baseViewController
      resultsPresenterFactory:nil
                   completion:nil];
}

- (void)openLensInputSelection:(OpenLensInputSelectionCommand*)command {
  __weak __typeof(self) weakSelf = self;
  // As a new Lens sessions starts, cleanup any inactive post capture before
  // presenting the input selection UI.
  [self destroyInactivePostCaptureSessionsWithCompletion:^{
    [weakSelf presentLensInputSelectionUIForCommand:command];
  }];
}

- (void)lensOverlayWillDismissWithCause:
    (LensOverlayDismissalCause)dismissalCause {
  if (!self.postCaptureShown) {
    return;
  }

  // If it was a swipe down of the bottom sheet, restart capturing.
  if (dismissalCause == LensOverlayDismissalCauseSwipeDownFromSelection) {
    [_lensViewController buildCaptureInfrastructureForSelection];
  } else if (dismissalCause ==
             LensOverlayDismissalCauseSwipeDownFromTranslate) {
    [_lensViewController buildCaptureInfrastructureForTranslate];
  } else if (dismissalCause == LensOverlayDismissalCauseDismissButton) {
    [_lensViewController tearDownCaptureInfrastructureWithPlaceholder:NO];
  }
}

- (void)lensOverlayDidDismissWithCause:
    (LensOverlayDismissalCause)dismissalCause {
  if (!self.postCaptureShown) {
    return;
  }

  self.postCaptureShown = NO;
  if (dismissalCause != LensOverlayDismissalCauseSwipeDownFromSelection &&
      dismissalCause != LensOverlayDismissalCauseSwipeDownFromTranslate) {
    // All other dismissal sources cause the UI to shut down.
    [self exitLensViewFinderAnimated:NO completion:nil];
  }
}

#pragma mark - ChromeLensViewFinderDelegate

- (void)lensController:(id<ChromeLensViewFinderController>)lensController
    didSelectImageWithMetadata:(id<LensImageMetadata>)imageMetadata {
  [_lensViewController tearDownCaptureInfrastructureWithPlaceholder:YES];

  __weak __typeof(self) weakSelf = self;
  auto startPostCapture = ^{
    [weakSelf startPostCaptureWithMetadata:imageMetadata];
  };
  if (_lensViewController.presentedViewController) {
    [_lensViewController.presentedViewController
        dismissViewControllerAnimated:YES
                           completion:startPostCapture];
  } else {
    startPostCapture();
  }
}

- (void)lensController:(id<ChromeLensViewFinderController>)lensController
          didSelectURL:(GURL)url {
  [_metricsRecorder recordLensViewFinderCameraURLOpen];
  __weak __typeof(self) weakSelf = self;
  [self exitLensViewFinderAnimated:YES
                        completion:^{
                          [weakSelf openInNewTab:url];
                        }];
}

- (void)lensControllerDidTapDismissButton:
    (id<ChromeLensViewFinderController>)lensController {
  [_metricsRecorder recordLensViewFinderDismissTapped];
  ProceduralBlock completion = nil;
  if (_presentNTPLensIconBubbleOnDismiss) {
    completion = ^{
      [self presentNTPLensIconBubble];
    };
  }
  [self exitLensViewFinderAnimated:YES completion:completion];
}

- (void)lensControllerWillAppear:
    (id<ChromeLensViewFinderController>)lensController {
  [self lockOrientationPortrait:YES];
}

- (void)lensControllerWillDisappear:
    (id<ChromeLensViewFinderController>)lensController {
  [self lockOrientationPortrait:NO];
  self.postCaptureShown = NO;
}

#pragma mark - Private

- (void)presentLensInputSelectionUIForCommand:
    (OpenLensInputSelectionCommand*)command {
  [self cancelOmniboxEdit];
  [self prepareLensViewControllerForCommand:command];

  if (!_lensViewController) {
    return;
  }

  _presentNTPLensIconBubbleOnDismiss =
      command.presentNTPLensIconBubbleOnDismiss;

  LensEntrypoint entrypoint = command.entryPoint;

  [self signalTrackerCameraOpenFromEntrypoint:entrypoint];

  [_lensViewController setLensViewFinderDelegate:self];
  [_metricsRecorder recordLensViewFinderOpenedFromEntrypoint:entrypoint];

  [self.baseViewController
      presentViewController:_lensViewController
                   animated:YES
                 completion:command.presentationCompletion];
}

- (void)prepareLensViewControllerForCommand:
    (OpenLensInputSelectionCommand*)command {
  LensOverlayConfigurationFactory* configurationFactory =
      [[LensOverlayConfigurationFactory alloc] init];
  LensConfiguration* configuration =
      [configurationFactory configurationForLensEntrypoint:command.entryPoint
                                                   profile:self.profile];

  _transitionManager = [[LensViewFinderTransitionManager alloc]
      initWithLVFTransitionType:TransitionFromPresentationStyle(
                                    command.presentationStyle)];

  _lensViewController =
      ios::provider::NewChromeLensViewFinderController(configuration);

  _lensViewController.transitioningDelegate = _transitionManager;
  _lensViewController.modalPresentationStyle =
      UIModalPresentationOverCurrentContext;
  _lensViewController.modalTransitionStyle =
      UIModalTransitionStyleCrossDissolve;

  [_lensViewController setLensViewFinderDelegate:nil];
}

- (void)destroyInactivePostCaptureSessionsWithCompletion:
    (ProceduralBlock)completion {
  id<LensOverlayCommands> lensOverlayCommands = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), LensOverlayCommands);
  [lensOverlayCommands
      destroyLensUI:NO
             reason:lens::LensOverlayDismissalSource::kSearchWithCameraRequested
         completion:completion];
}

- (void)exitLensViewFinderAnimated:(BOOL)animated
                        completion:(ProceduralBlock)completion {
  if (_lensViewController &&
      self.baseViewController.presentedViewController == _lensViewController) {
    [self.baseViewController dismissViewControllerAnimated:animated
                                                completion:completion];
  } else if (completion) {
    completion();
  }
}

- (void)lockOrientationPortrait:(BOOL)portraitLock {
  if (!portraitLock) {
    _scopedForceOrientation = nil;
    return;
  }

  SceneState* sceneState = self.browser->GetSceneState();
  if (!self.browser) {
    return;
  }
  if (AppState* appState = sceneState.profileState.appState) {
    _scopedForceOrientation = ForcePortraitOrientationOnIphone(appState);
  }
}

- (void)openInNewTab:(GURL)URL {
  OpenNewTabCommand* command =
      [OpenNewTabCommand commandWithURLFromChrome:URL
                                      inIncognito:self.isOffTheRecord];

  [HandlerForProtocol(self.browser->GetCommandDispatcher(), SceneCommands)
      openURLInNewTab:command];
}

- (void)startPostCaptureWithMetadata:(id<LensImageMetadata>)imageMetadata {
  LensOverlayEntrypoint entrypoint =
      imageMetadata.isCameraImage ? LensOverlayEntrypoint::kLVFCameraCapture
                                  : LensOverlayEntrypoint::kLVFImagePicker;

  __weak __typeof(self) weakSelf = self;
  id<LensOverlayCommands> lensOverlayCommands = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), LensOverlayCommands);
  [lensOverlayCommands searchWithLensImageMetadata:imageMetadata
                                        entrypoint:entrypoint
                           initialPresentationBase:_lensViewController
                                        completion:^(BOOL) {
                                          weakSelf.postCaptureShown = YES;
                                        }];
}

// Cancel any editing before presenting the Lens View Finder experience to
// prevent the omnibox popup from obscuring the view.
- (void)cancelOmniboxEdit {
  Browser* browser = self.browser;
  CommandDispatcher* dispatcher = browser->GetCommandDispatcher();
  id<BrowserCoordinatorCommands> browserCoordinatorHandler =
      HandlerForProtocol(dispatcher, BrowserCoordinatorCommands);
  [browserCoordinatorHandler hideComposebox];
}

// Sets the visibility of the Lens replacement for the QR code scanner in the
// home screen widget.
- (void)updateLensAvailabilityForWidgets {
  NSUserDefaults* sharedDefaults = app_group::GetGroupUserDefaults();
  NSString* enableLensInWidgetKey =
      base::SysUTF8ToNSString(app_group::kChromeAppGroupEnableLensInWidget);

  // Determine the availability of the Lens entrypoint in the home screen
  // widget. We don't use LensAvailability here because the seach engine status
  // is determined elsewhere in the Extension Search Engine Data Updater.
  const bool enableLensInWidget =
      ios::provider::IsLensSupported() &&
      GetApplicationContext()->GetLocalState()->GetBoolean(
          prefs::kLensCameraAssistedSearchPolicyAllowed) &&
      !base::FeatureList::IsEnabled(kDisableLensCamera) &&
      ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_TABLET;
  [sharedDefaults setBool:enableLensInWidget forKey:enableLensInWidgetKey];
}

// Returns whether Google is the default search engine.
- (BOOL)isGoogleDefaultSearchEngine {
  TemplateURLService* templateURLService =
      ios::TemplateURLServiceFactory::GetForProfile(self.profile);
  const TemplateURL* defaultURL =
      templateURLService->GetDefaultSearchProvider();
  BOOL isGoogleDefaultSearchProvider =
      defaultURL &&
      defaultURL->GetEngineType(templateURLService->search_terms_data()) ==
          SEARCH_ENGINE_GOOGLE;
  return isGoogleDefaultSearchProvider;
}

// Sets the app shortcut item for either the QR code scanner or Lens.
- (void)updateQRCodeOrLensAppShortcutItem {
  const bool useLens =
      lens_availability::CheckAndLogAvailabilityForLensEntryPoint(
          LensEntrypoint::AppIconLongPress, [self isGoogleDefaultSearchEngine]);

  NSString* shortcutType;
  NSString* shortcutTitle;
  UIApplicationShortcutIcon* shortcutIcon;
  if (useLens) {
    shortcutType = kShortcutLensFromAppIconLongPress;
    shortcutTitle = l10n_util::GetNSStringWithFixup(
        IDS_IOS_APPLICATION_SHORTCUT_LENS_TITLE);
    shortcutIcon =
        [UIApplicationShortcutIcon iconWithTemplateImageName:kCameraLensSymbol];
  } else {
    shortcutType = kShortcutQRScanner;
    shortcutTitle = l10n_util::GetNSStringWithFixup(
        IDS_IOS_APPLICATION_SHORTCUT_QR_SCANNER_TITLE);
    shortcutIcon =
        [UIApplicationShortcutIcon iconWithSystemImageName:kQRCodeSymbol];
  }
  UIApplicationShortcutItem* item =
      [[UIApplicationShortcutItem alloc] initWithType:shortcutType
                                       localizedTitle:shortcutTitle
                                    localizedSubtitle:nil
                                                 icon:shortcutIcon
                                             userInfo:nil];
  [[UIApplication sharedApplication] setShortcutItems:@[ item ]];
}

// Presents an IPH bubble that points to the Lens Icon in the NTP's Fakebox.
- (void)presentNTPLensIconBubble {
  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  [HandlerForProtocol(dispatcher, NewTabPageCommands) presentLensIconBubble];
}

// Signals the different trackers that the camera view finder has been opened
// from `entrypoint`.
- (void)signalTrackerCameraOpenFromEntrypoint:(LensEntrypoint)entrypoint {
  ProfileIOS* profile = self.profile;

  [IntentDonationHelper donateIntent:IntentType::kStartLens];

  feature_engagement::Tracker* featureTracker =
      feature_engagement::TrackerFactory::GetForProfile(profile);
  CHECK(featureTracker, base::NotFatalUntil::M160);

  // Mark IPHs as completed.
  if (entrypoint == LensEntrypoint::Keyboard) {
    featureTracker->NotifyEvent(
        feature_engagement::events::kLensButtonKeyboardUsed);
  } else if (entrypoint == LensEntrypoint::Composebox) {
    featureTracker->NotifyEvent(
        feature_engagement::events::kIOSLensButtonComposeboxUsed);
  }

  TipsManagerIOS* tipsManager = TipsManagerIOSFactory::GetForProfile(profile);

  if (tipsManager) {
    tipsManager->NotifySignal(
        segmentation_platform::tips_manager::signals::kLensUsed);
  }

  // Notify Welcome Back to remove Lens from the eligible features.
  if (IsWelcomeBackEnabled()) {
    MarkWelcomeBackFeatureUsed(BestFeaturesItemType::kLensSearch);
  }

  GetApplicationContext()->GetLocalState()->SetTime(prefs::kLensLastOpened,
                                                    base::Time::Now());
}

@end
