// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/search_engine_choice/coordinator/search_engine_choice_coordinator.h"

#import "base/check_op.h"
#import "base/metrics/histogram_functions.h"
#import "base/time/time.h"
#import "components/regional_capabilities/regional_capabilities_service.h"
#import "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#import "components/search_engines/search_engine_choice/search_engine_choice_utils.h"
#import "components/search_engines/search_engines_switches.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/first_run/model/first_run_metrics.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_screen_delegate.h"
#import "ios/chrome/browser/regional_capabilities/model/regional_capabilities_service_factory.h"
#import "ios/chrome/browser/search_engine_choice/coordinator/search_engine_choice_mediator.h"
#import "ios/chrome/browser/search_engine_choice/model/search_engine_choice_util.h"
#import "ios/chrome/browser/search_engine_choice/search_engine_choice_learn_more/coordinator/search_engine_choice_learn_more_coordinator.h"
#import "ios/chrome/browser/search_engine_choice/ui/search_engine_choice_constants.h"
#import "ios/chrome/browser/search_engine_choice/ui/search_engine_choice_view_controller.h"
#import "ios/chrome/browser/search_engines/model/search_engine_choice_service_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util_mac.h"

using search_engines::SearchEngineChoiceScreenEvents;

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
  ProfileIOS* profile = self.profile->GetOriginalProfile();
  if (!ShouldDisplaySearchEngineChoiceScreen(
          *profile, _firstRun,
          /*app_started_via_external_intent=*/false)) {
    // If the search engine enterprise pocliy has been loaded, just before to
    // open the Search Engine Choice dialog, it should be skipped.
    [self dismissChoiceScreenIfNeeded];
    return;
  }
  _viewController =
      [[SearchEngineChoiceViewController alloc] initWithFirstRunMode:_firstRun];
  _viewController.actionDelegate = self;
  TemplateURLService* templateURLService =
      ios::TemplateURLServiceFactory::GetForProfile(profile);
  search_engines::SearchEngineChoiceService* searchEngineChoiceService =
      ios::SearchEngineChoiceServiceFactory::GetForProfile(profile);
  regional_capabilities::RegionalCapabilitiesService*
      regionalCapabilitiesService =
          ios::RegionalCapabilitiesServiceFactory::GetForProfile(profile);
  std::optional<
      regional_capabilities::RegionalCapabilitiesService::ChoiceScreenDesign>
      choiceScreenDesign = regionalCapabilitiesService->GetChoiceScreenDesign();
  // ShouldDisplaySearchEngineChoiceScreen() should return false if there is
  // no value from RegionalCapabilitiesService::GetChoiceScreenDesign().
  CHECK(choiceScreenDesign.has_value());
  _mediator = [[SearchEngineChoiceMediator alloc]
       initWithTemplateURLService:templateURLService
        searchEngineChoiceService:searchEngineChoiceService
      regionalCapabilitiesService:regionalCapabilitiesService];
  _mediator.consumer = _viewController;
  _viewController.mutator = _mediator;
  _viewController.modalInPresentation = YES;
  if (_firstRun) {
    BOOL animated = self.baseNavigationController.topViewController != nil;
    [self.baseNavigationController setViewControllers:@[ _viewController ]
                                             animated:animated];
    [self recordChoiceScreenEvent:SearchEngineChoiceScreenEvents::
                                      kFreChoiceScreenWasDisplayed];
    base::UmaHistogramEnumeration(first_run::kFirstRunStageHistogram,
                                  first_run::kSearchEngineChoiceScreenStart);
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
    [self recordChoiceScreenEvent:SearchEngineChoiceScreenEvents::
                                      kChoiceScreenWasDisplayed];
  }
}

- (void)stop {
  if (!_firstRun) {
    [self dismissChoiceScreenAnimated:YES completion:nil];
  } else {
    _viewController.mutator = nil;
    _viewController = nil;
  }
  [_searchEngineChoiceLearnMoreCoordinator stop];
  _searchEngineChoiceLearnMoreCoordinator = nil;
  [_mediator disconnect];
  _mediator.consumer = nil;
  _mediator = nil;
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
    [self recordChoiceScreenEvent:SearchEngineChoiceScreenEvents::
                                      kFreLearnMoreWasDisplayed];
  } else {
    [self recordChoiceScreenEvent:SearchEngineChoiceScreenEvents::
                                      kLearnMoreWasDisplayed];
  }
}

- (void)didTapPrimaryButton {
  if (_didTapPrimaryButton) {
    NOTREACHED() << "Double tap on primary button [_firstRun = " << _firstRun
                 << " ; delay : "
                 << (base::Time::Now() -
                     _lastCallToDidTapPrimaryButtonTimestamp)
                        .InMilliseconds()
                 << " ms]";
  }
  _didTapPrimaryButton = YES;
  _lastCallToDidTapPrimaryButtonTimestamp = base::Time::Now();
  if (_firstRun) {
    [self recordChoiceScreenEvent:SearchEngineChoiceScreenEvents::
                                      kFreDefaultWasSet];
    base::UmaHistogramEnumeration(
        first_run::kFirstRunStageHistogram,
        first_run::kSearchEngineChoiceScreenCompletionWithSelection);
  } else {
    [self
        recordChoiceScreenEvent:SearchEngineChoiceScreenEvents::kDefaultWasSet];
  }
  [_mediator saveDefaultSearchEngine];
  [self dismissChoiceScreenIfNeeded];
}

#pragma mark - SearchEngineChoiceLearnMoreCoordinatorDelegate

- (void)learnMoreDidDismiss {
  [_searchEngineChoiceLearnMoreCoordinator stop];
  _searchEngineChoiceLearnMoreCoordinator.delegate = nil;
  _searchEngineChoiceLearnMoreCoordinator = nil;
}

#pragma mark - Private

// For the first run, the view controller is not dismissed, and
// `_firstRunDelegate` is called. Otherwise, the view controller is dismissed,
// and then the delegate is called.
- (void)dismissChoiceScreenIfNeeded {
  if (_firstRun) {
    [_firstRunDelegate screenWillFinishPresenting];
  } else {
    __weak __typeof(self) weakSelf = self;
    [self dismissChoiceScreenAnimated:YES
                           completion:^{
                             [weakSelf.delegate
                                 choiceScreenWasDismissed:weakSelf];
                           }];
  }
}

// Dimisses the view controller and calls `completion`.
- (void)dismissChoiceScreenAnimated:(BOOL)animated
                         completion:(ProceduralBlock)completion {
  UIViewController* viewController = _viewController;
  _viewController.mutator = nil;
  _viewController = nil;
  [viewController.presentingViewController
      dismissViewControllerAnimated:animated
                         completion:completion];
}

- (void)recordChoiceScreenEvent:(SearchEngineChoiceScreenEvents)event {
  ProfileIOS* profile = self.profile->GetOriginalProfile();
  search_engines::SearchEngineChoiceService* searchEngineChoiceService =
      ios::SearchEngineChoiceServiceFactory::GetForProfile(profile);
  searchEngineChoiceService->RecordChoiceScreenEvent(event);
}

@end
