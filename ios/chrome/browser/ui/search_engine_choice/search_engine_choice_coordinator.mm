// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_coordinator.h"

#import "base/check_op.h"
#import "base/time/time.h"
#import "components/search_engines/search_engine_choice/search_engine_choice_utils.h"
#import "components/search_engines/search_engines_switches.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_screen_delegate.h"
#import "ios/chrome/browser/search_engine_choice/model/search_engine_choice_util.h"
#import "ios/chrome/browser/search_engines/model/search_engine_choice_service_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_constants.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_learn_more/search_engine_choice_learn_more_coordinator.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_learn_more/search_engine_choice_learn_more_view_controller.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_mediator.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_view_controller.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util_mac.h"

@interface SearchEngineChoiceCoordinator () <
    SearchEngineChoiceActionDelegate,
    SearchEngineChoiceLearnMoreCoordinatorDelegate>
@end

@implementation SearchEngineChoiceCoordinator {
  // The mediator that fetches the list of search engines.
  SearchEngineChoiceMediator* _mediator;
  // The view controller for the search engines.
  SearchEngineChoiceViewController* _viewController;
  // Coordinator for the informational popup that may be displayed to the user.
  SearchEngineChoiceLearnMoreCoordinator*
      _searchEngineChoiceLearnMoreCoordinator;
  // Whether the screen is being shown in the FRE.
  BOOL _firstRun;
  // Whether the primary account button was already tapped.
  BOOL _didTapPrimaryButton;
  // Timestamp of the previous call to `-(void)_didTapPrimaryButton`.
  base::Time _lastCallToDidTapPrimaryButtonTimestamp;
  // First run screen delegate.
  __weak id<FirstRunScreenDelegate> _firstRunDelegate;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _firstRun = NO;
    _didTapPrimaryButton = NO;
  }
  return self;
}

- (instancetype)initForFirstRunWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                                    browser:(Browser*)browser
                                           firstRunDelegate:
                                               (id<FirstRunScreenDelegate>)
                                                   delegate {
  self = [self initWithBaseViewController:navigationController browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
    _firstRun = YES;
    _firstRunDelegate = delegate;
  }
  return self;
}

- (void)start {
  [super start];
  // Make sure we use the original profile (non-incognito).
  ProfileIOS* profile = self.browser->GetProfile()->GetOriginalProfile();
  if (!ShouldDisplaySearchEngineChoiceScreen(
          *profile, _firstRun,
          /*app_started_via_external_intent=*/false)) {
    // If the search engine enterprise pocliy has been loaded, just before to
    // open the Search Engine Choice dialog, it should be skipped.
    [self dismissChoiceScreen];
    return;
  }
  _viewController =
      [[SearchEngineChoiceViewController alloc] initWithFirstRunMode:_firstRun];
  _viewController.actionDelegate = self;
  TemplateURLService* templateURLService =
      ios::TemplateURLServiceFactory::GetForProfile(profile);
  search_engines::SearchEngineChoiceService* searchEngineChoiceService =
      ios::SearchEngineChoiceServiceFactory::GetForProfile(profile);
  _mediator = [[SearchEngineChoiceMediator alloc]
      initWithTemplateURLService:templateURLService
       searchEngineChoiceService:searchEngineChoiceService];
  _mediator.consumer = _viewController;
  _viewController.mutator = _mediator;
  _viewController.modalInPresentation = YES;
  if (_firstRun) {
    BOOL animated = self.baseNavigationController.topViewController != nil;
    [self.baseNavigationController setViewControllers:@[ _viewController ]
                                             animated:animated];
    search_engines::RecordChoiceScreenEvent(
        search_engines::SearchEngineChoiceScreenEvents::
            kFreChoiceScreenWasDisplayed);
  } else {
    if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_PHONE) {
      _viewController.modalPresentationStyle = UIModalPresentationFormSheet;
      _viewController.preferredContentSize =
          CGSizeMake(kIPadSearchEngineChoiceScreenPreferredWidth,
                     kIPadSearchEngineChoiceScreenPreferredHeight);
    }
    [self.baseViewController presentViewController:_viewController
                                          animated:YES
                                        completion:nil];
    search_engines::RecordChoiceScreenEvent(
        search_engines::SearchEngineChoiceScreenEvents::
            kChoiceScreenWasDisplayed);
  }
}

- (void)stop {
  if (!_firstRun) {
    [_viewController.presentingViewController
        dismissViewControllerAnimated:YES
                           completion:nil];
  }

  [_searchEngineChoiceLearnMoreCoordinator stop];
  _searchEngineChoiceLearnMoreCoordinator = nil;
  [_mediator disconnect];
  _mediator.consumer = nil;
  _mediator = nil;
  _viewController.mutator = nil;
  _viewController = nil;
  _baseNavigationController = nil;
  _firstRunDelegate = nil;
  [super stop];
}

#pragma mark - SearchEngineChoiceViewControllerDelegate

- (void)showLearnMore {
  _searchEngineChoiceLearnMoreCoordinator =
      [[SearchEngineChoiceLearnMoreCoordinator alloc]
          initWithBaseViewController:_viewController
                             browser:self.browser];
  _searchEngineChoiceLearnMoreCoordinator.forcePresentationFormSheet =
      _firstRun;
  _searchEngineChoiceLearnMoreCoordinator.delegate = self;
  [_searchEngineChoiceLearnMoreCoordinator start];
  if (_firstRun) {
    search_engines::RecordChoiceScreenEvent(
        search_engines::SearchEngineChoiceScreenEvents::
            kFreLearnMoreWasDisplayed);
  } else {
    search_engines::RecordChoiceScreenEvent(
        search_engines::SearchEngineChoiceScreenEvents::kLearnMoreWasDisplayed);
  }
}

- (void)didTapPrimaryButton {
  if (_didTapPrimaryButton) {
    NOTREACHED(base::NotFatalUntil::M127)
        << "Double tap on primary button [_firstRun = " << _firstRun
        << " ; delay : "
        << (base::Time::Now() - _lastCallToDidTapPrimaryButtonTimestamp)
               .InMilliseconds()
        << " ms]";
    return;
  }
  _didTapPrimaryButton = YES;
  _lastCallToDidTapPrimaryButtonTimestamp = base::Time::Now();
  if (_firstRun) {
    search_engines::RecordChoiceScreenEvent(
        search_engines::SearchEngineChoiceScreenEvents::kFreDefaultWasSet);
  } else {
    search_engines::RecordChoiceScreenEvent(
        search_engines::SearchEngineChoiceScreenEvents::kDefaultWasSet);
  }
  [_mediator saveDefaultSearchEngine];
  [self dismissChoiceScreen];
}

#pragma mark - SearchEngineChoiceLearnMoreCoordinatorDelegate

- (void)learnMoreDidDismiss {
  [_searchEngineChoiceLearnMoreCoordinator stop];
  _searchEngineChoiceLearnMoreCoordinator.delegate = nil;
  _searchEngineChoiceLearnMoreCoordinator = nil;
}

#pragma mark - Private

- (void)dismissChoiceScreen {
  if (_firstRun) {
    [_firstRunDelegate screenWillFinishPresenting];
  } else {
    [self.delegate choiceScreenWillBeDismissed:self];
  }
}

@end
