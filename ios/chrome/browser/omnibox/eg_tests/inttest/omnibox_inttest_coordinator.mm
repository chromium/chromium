// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/eg_tests/inttest/omnibox_inttest_coordinator.h"

#import <memory>

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/autocomplete/model/autocomplete_browser_agent.h"
#import "ios/chrome/browser/autocomplete/test/fake_suggestions_autocomplete_controller.h"
#import "ios/chrome/browser/autocomplete/test/fake_suggestions_builder.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/omnibox/coordinator/omnibox_coordinator+Testing.h"
#import "ios/chrome/browser/omnibox/coordinator/omnibox_coordinator.h"
#import "ios/chrome/browser/omnibox/eg_tests/inttest/fake_omnibox_client.h"
#import "ios/chrome/browser/omnibox/eg_tests/inttest/omnibox_inttest_view_controller.h"
#import "ios/chrome/browser/omnibox/eg_tests/inttest/omnibox_inttest_view_controller_delegate.h"
#import "ios/chrome/browser/omnibox/model/chrome_omnibox_client_ios.h"
#import "ios/chrome/browser/omnibox/public/omnibox_presentation_context.h"
#import "ios/chrome/browser/omnibox/ui/omnibox_focus_delegate.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "url/gurl.h"

@interface OmniboxInttestCoordinator () <OmniboxInttestViewControllerDelegate>
@end

@implementation OmniboxInttestCoordinator {
  OmniboxInttestViewController* _viewController;
  raw_ptr<FakeOmniboxClient> _fakeOmniboxClient;
}

- (void)start {
  ProfileIOS* profile = self.profile;
  Browser* browser = self.browser;

  // View Controller to layout the OmniboxView, OmniboxPopupView and a cancel
  // button.
  _viewController = [[OmniboxInttestViewController alloc] init];
  _viewController.delegate = self;
  [self.baseViewController presentViewController:_viewController
                                        animated:NO
                                      completion:nil];

  // Omnibox Coordinator.
  auto omniboxClient = std::make_unique<FakeOmniboxClient>(profile);
  _fakeOmniboxClient = omniboxClient.get();

  OmniboxCoordinator* omniboxCoordinator = [[OmniboxCoordinator alloc]
      initWithBaseViewController:nil
                         browser:browser
                   omniboxClient:std::move(omniboxClient)
             presentationContext:OmniboxPresentationContext::kLensOverlay];

  omniboxCoordinator.presenterDelegate = _viewController;
  omniboxCoordinator.searchOnlyUI = YES;
  [omniboxCoordinator start];

  [omniboxCoordinator.managedViewController
      willMoveToParentViewController:_viewController];
  [_viewController
      addChildViewController:omniboxCoordinator.managedViewController];
  [_viewController setEditView:omniboxCoordinator.editView];
  [omniboxCoordinator.managedViewController
      didMoveToParentViewController:_viewController];

  [omniboxCoordinator updateOmniboxState];

  self.omniboxCoordinator = omniboxCoordinator;
  [self simulateNTP];
}

- (void)stop {
  _fakeOmniboxClient = nullptr;
  [self.omniboxCoordinator stop];
  self.omniboxCoordinator = nil;

  _viewController.delegate = nil;
  [_viewController.presentingViewController dismissViewControllerAnimated:NO
                                                               completion:nil];
  _viewController = nil;
  AutocompleteBrowserAgent::FromBrowser(self.browser)->RemoveServices();
}

- (void)simulateNTP {
  _fakeOmniboxClient->set_current_page_exists(true);
  _fakeOmniboxClient->set_url(GURL(kChromeUINewTabURL));
  _fakeOmniboxClient->set_page_classification(
      metrics::OmniboxEventProto::INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS);
}

- (GURL)lastURLLoaded {
  return _fakeOmniboxClient->get_on_autocomplete_accept_destination_url();
}

- (void)resetLastURLLoaded {
  _fakeOmniboxClient->set_on_autocomplete_accept_destination_url(GURL());
}

#pragma mark - OmniboxInttestViewControllerDelegate

- (void)viewControllerDidTapCancelButton:
    (OmniboxInttestViewController*)viewController {
  [self.omniboxCoordinator endEditing];
}

@end
