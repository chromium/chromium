// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/ui_bundled/password_suggestion_coordinator.h"

#import "base/check.h"
#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/common/password_generation_util.h"
#import "components/password_manager/core/browser/features/password_features.h"
#import "components/password_manager/ios/constants.h"
#import "components/password_manager/ios/password_manager_java_script_feature.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_tab_helper.h"
#import "ios/chrome/browser/autofill/model/form_input_accessory_view_handler.h"
#import "ios/chrome/browser/passwords/ui_bundled/password_suggestion_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"
#import "ui/base/device_form_factor.h"

namespace {
constexpr CGFloat preferredCornerRadius = 20;
}  // namespace

@interface PasswordSuggestionCoordinator () <
    ConfirmationAlertActionHandler,
    UIAdaptivePresentationControllerDelegate>

// Main view controller for this coordinator.
@property(nonatomic, strong) PasswordSuggestionViewController* viewController;

// The suggested strong password.
@property(nonatomic, copy) NSString* passwordSuggestion;

// The suggest password decision handler.
@property(nonatomic, copy) void (^decisionHandler)(BOOL accept);

@end

@implementation PasswordSuggestionCoordinator {
  // YES when the bottom sheet is proactive where it is triggered upon focus.
  BOOL _proactive;
}

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                        passwordSuggestion:(NSString*)passwordSuggestion
                           decisionHandler:
                               (void (^)(BOOL accept))decisionHandler
                                 proactive:(BOOL)proactive {
  self = [super initWithBaseViewController:baseViewController browser:browser];

  if (self) {
    _passwordSuggestion = passwordSuggestion;
    _decisionHandler = decisionHandler;
    _proactive = proactive;
  }

  return self;
}

- (void)start {
  self.viewController = [[PasswordSuggestionViewController alloc]
      initWithPasswordSuggestion:self.passwordSuggestion
                       userEmail:[self userEmail]
                       proactive:_proactive];
  self.viewController.presentationController.delegate = self;
  self.viewController.actionHandler = self;

  NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
  [defaultCenter addObserver:self
                    selector:@selector(applicationDidEnterBackground:)
                        name:UIApplicationDidEnterBackgroundNotification
                      object:nil];

  self.viewController.modalPresentationStyle = UIModalPresentationPageSheet;
  UISheetPresentationController* presentationController =
      self.viewController.sheetPresentationController;
  presentationController.detents = [self detents];
  presentationController.prefersEdgeAttachedInCompactHeight =
      [self isEdgeAttachedInCompactHeight];

  presentationController.preferredCornerRadius = preferredCornerRadius;

  // Immediately dismiss the keyboard (only on tablet) because the
  // PasswordSuggestion view controller is incorrectly being displayed behind
  // the keyboard. This issue does not happen on mobile devices.
  // For more information, please see: https://www.crbug.com/1307759.
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    [self closeKeyboard];
  }

  [self.baseViewController presentViewController:self.viewController
                                        animated:YES
                                      completion:nil];

  // Dismiss right away if the presentation failed to avoid having a zombie
  // coordinator. This is the best proxy we have to know whether the view
  // controller for the bottom sheet could really be presented as the completion
  // block is only called when presentation really happens, and we can't get any
  // error message or signal. Based on what we could test, we know that
  // presentingViewController is only set if the view controller can be
  // presented, where it is left to nil if the presentation is rejected for
  // various reasons (having another view controller already presented is one of
  // them). One should not think they can know all the reasons why the
  // presentation fails.
  //
  // Keep this line at the end of -start because the
  // delegate will likely -stop the coordinator when closing suggestions, so the
  // coordinator should be in the most up to date state where it can be safely
  // stopped.
  if (!self.viewController.presentingViewController) {
    [self.delegate closePasswordSuggestion];
  }
}

- (void)stop {
  [self.viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  self.viewController = nil;
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertSecondaryAction {
  [self handleDecision:NO];
  [self incrementDismissCount];
  [self disableBottomSheet];
  [self.delegate closePasswordSuggestion];
}

- (void)confirmationAlertPrimaryAction {
  [self handleDecision:YES];
  [self.delegate closePasswordSuggestion];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self handleDecision:NO];
  [self incrementDismissCount];
  [self disableBottomSheet];
  [self.delegate closePasswordSuggestion];
}

#pragma mark - Notification callback

- (void)applicationDidEnterBackground:(NSNotification*)notification {
  [self confirmationAlertSecondaryAction];
}

#pragma mark - Private methods

// Returns the user email.
- (NSString*)userEmail {
  ProfileIOS* profile = self.browser->GetProfile();
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForProfile(profile);
  id<SystemIdentity> authenticatedIdentity =
      authService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);

  return authenticatedIdentity.userEmail;
}

- (void)handleDecision:(BOOL)accept {
  if (accept) {
    [self resetPasswordGenerationBottomSheetDismissCount];
  }
  if (self.decisionHandler) {
    self.decisionHandler(accept);
  }
}

// Closes the keyboard.
- (void)closeKeyboard {
  NSString* activeWebStateIdentifier = self.browser->GetWebStateList()
                                           ->GetActiveWebState()
                                           ->GetStableIdentifier();
  [self onCloseKeyboardWithIdentifier:activeWebStateIdentifier];
}

- (web::WebState*)activeWebState {
  if (!self.browser) {
    return nullptr;
  }
  web::WebState* activeWebState =
      self.browser->GetWebStateList()->GetActiveWebState();
  if (!activeWebState) {
    return nullptr;
  }
  return activeWebState;
}

// Helper method which closes the keyboard.
- (void)onCloseKeyboardWithIdentifier:(NSString*)identifier {
  web::WebState* webState = [self activeWebState];
  if (!webState) {
    return;
  }
  // Note that it may have changed between the moment the
  // block was created and its invocation. So check whether
  // the WebState identifier is the same.
  NSString* webStateIdentifier = webState->GetStableIdentifier();
  if (![webStateIdentifier isEqualToString:identifier])
    return;
  password_manager::PasswordManagerJavaScriptFeature* feature =
      password_manager::PasswordManagerJavaScriptFeature::GetInstance();
  web::WebFrame* mainFrame =
      feature->GetWebFramesManager(webState)->GetMainWebFrame();
  if (!mainFrame) {
    return;
  }

  FormInputAccessoryViewHandler* handler =
      [[FormInputAccessoryViewHandler alloc] init];
  handler.webState = webState;
  NSString* mainFrameID = base::SysUTF8ToNSString(mainFrame->GetFrameId());
  [handler setLastFocusFormActivityWebFrameID:mainFrameID];
  [handler closeKeyboardWithoutButtonPress];
}

// Increments the password generation bottom sheet dismiss count
// preference.
- (void)incrementDismissCount {
  if (!base::FeatureList::IsEnabled(
          password_manager::features::
              kIOSProactivePasswordGenerationBottomSheet)) {
    return;
  }
  ProfileIOS* profile = self.browser->GetProfile();
  if (!profile) {
    return;
  }
  PrefService* prefService = profile->GetPrefs();
  if (prefService) {
    const int newDismissCount =
        prefService->GetInteger(
            prefs::kIosPasswordGenerationBottomSheetDismissCount) +
        1;
    prefService->SetInteger(
        prefs::kIosPasswordGenerationBottomSheetDismissCount, newDismissCount);
    if (newDismissCount == AutofillBottomSheetTabHelper::
                               kPasswordGenerationBottomSheetMaxDismissCount) {
      base::UmaHistogramEnumeration(
          "PasswordGeneration.BottomSheetStateTransitionPasswordGeneration.iOS."
          "ProactiveBottomSheetStateTransition",
          PasswordGenerationBottomSheetStateTransitionType::kSilenced);
    }
  }
}

// Disables the proactive password generation bottom sheet for the current tab
// session by detaching the listeners.
- (void)disableBottomSheet {
  if (!base::FeatureList::IsEnabled(
          password_manager::features::
              kIOSProactivePasswordGenerationBottomSheet)) {
    return;
  }

  web::WebState* webState = [self activeWebState];
  if (!webState) {
    return;
  }
  AutofillBottomSheetTabHelper* tabHelper =
      AutofillBottomSheetTabHelper::FromWebState(webState);
  if (!tabHelper) {
    return;
  }

  tabHelper->DetachPasswordGenerationListenersForAllFrames();
}

// Resets the proactive password generation bottom sheet dismiss count to 0 when
// a generated password suggestion is accepted.
- (void)resetPasswordGenerationBottomSheetDismissCount {
  if (!base::FeatureList::IsEnabled(
          password_manager::features::
              kIOSProactivePasswordGenerationBottomSheet)) {
    return;
  }
  web::WebState* webState = [self activeWebState];
  if (!webState) {
    return;
  }
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(webState->GetBrowserState());
  if (!profile) {
    return;
  }
  PrefService* prefService = profile->GetPrefs();
  if (prefService) {
    const int currentDismissCount = prefService->GetInteger(
        prefs::kIosPasswordGenerationBottomSheetDismissCount);
    if (currentDismissCount ==
        AutofillBottomSheetTabHelper::
            kPasswordGenerationBottomSheetMaxDismissCount) {
      base::UmaHistogramEnumeration(
          "PasswordGeneration.iOS.ProactiveBottomSheetStateTransition",
          PasswordGenerationBottomSheetStateTransitionType::kUnsilenced);
    }
    prefService->SetInteger(
        prefs::kIosPasswordGenerationBottomSheetDismissCount, 0);
  }
}

// Returns the minimum detent height such that the entire content can be
// shown, constrained between the medium and large detent heights. If
// the content does not fit entirely in the largest height, the content
// is scrollable.
- (UISheetPresentationControllerDetent*)preferredHeightDetent {
  auto resolver = ^CGFloat(
      id<UISheetPresentationControllerDetentResolutionContext> context) {
    CGFloat height = [self.viewController preferredHeightForContent];
    CGFloat largeDetentHeight = [UISheetPresentationControllerDetent.largeDetent
        resolvedValueInContext:context];
    height = MIN(height, largeDetentHeight);
    CGFloat mediumDetentHeight =
        [UISheetPresentationControllerDetent.mediumDetent
            resolvedValueInContext:context];
    return MAX(height, mediumDetentHeight);
  };
  return [UISheetPresentationControllerDetent
      customDetentWithIdentifier:@"preferred_height"
                        resolver:resolver];
}

- (NSArray<UISheetPresentationControllerDetent*>*)detents {
  // Custom sized detents for modals are available from iOS 16.
  if (@available(iOS 18, *)) {
    if (UIDevice.currentDevice.userInterfaceIdiom == UIUserInterfaceIdiomPad) {
      // As of iOS 18, the modal on iPad no longer appears near the bottom
      // edge and should not be expandable (i.e. large detent should not
      // be an option).
      return @[ [self preferredHeightDetent] ];
    }
  }
  // Having the large detent as an option makes the modal expandable to
  // the maximum size.
  return @[
    [self preferredHeightDetent],
    UISheetPresentationControllerDetent.largeDetent
  ];
}

- (BOOL)isEdgeAttachedInCompactHeight {
  if (@available(iOS 18, *)) {
    // This specifically affects the iPad mini format, so the bottom
    // sheet does not attach to the bottom edge like it does on iPhone.
    if (UIDevice.currentDevice.userInterfaceIdiom == UIUserInterfaceIdiomPad) {
      return NO;
    }
  }
  return YES;
}

@end
