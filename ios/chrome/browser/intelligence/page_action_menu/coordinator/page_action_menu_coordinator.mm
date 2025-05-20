// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/page_action_menu/coordinator/page_action_menu_coordinator.h"

#import "base/functional/callback_helpers.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/intelligence/glic/coordinator/glic_coordinator.h"
#import "ios/chrome/browser/intelligence/glic/model/glic_service.h"
#import "ios/chrome/browser/intelligence/glic/model/glic_service_factory.h"
#import "ios/chrome/browser/intelligence/page_action_menu/coordinator/page_action_menu_mediator.h"
#import "ios/chrome/browser/intelligence/page_action_menu/ui/page_action_menu_mutator.h"
#import "ios/chrome/browser/intelligence/page_action_menu/ui/page_action_menu_view_controller.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/page_action_menu_commands.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"

@implementation PageActionMenuCoordinator {
  PageActionMenuViewController* _viewController;
  PageActionMenuMediator* _mediator;

  // The PageContext wrapper used to provide context about a page.
  PageContextWrapper* _pageContextWrapper;

  // The coordinator for the glic consent flow.
  GLICCoordinator* _glicCoordinator;
}

#pragma mark - ChromeCoordinator

- (void)start {
  _viewController = [[PageActionMenuViewController alloc] init];
  _mediator = [[PageActionMenuMediator alloc] init];

  _viewController.mutator = _mediator;

  // TODO(crbug.com/408006823): Have the view controller call this when its
  // button is pressed.
  [self handleEntryPointPressed];
  [super start];
}

- (void)stop {
  _viewController = nil;
  _mediator = nil;

  [super stop];
}

#pragma mark - Private

// TODO(crbug.com/408006823): Rename this function.
- (void)handleEntryPointPressed {
  if (![self checkCapabilities]) {
    return;
  }

  if ([self shouldShowGLICConsent]) {
    [self showGLICConsent];
    return;
  }

  [self prepareGLICOverlay];
}

// Decides whether GLIC consent should be shown.
- (BOOL)shouldShowGLICConsent {
  PrefService* prefService = self.profile->GetPrefs();
  CHECK(prefService);
  return !prefService->GetBoolean(prefs::kIOSGLICConsent);
}

// Checks capabilities for GLIC feature.
- (BOOL)checkCapabilities {
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(self.profile);

  if (!identityManager) {
    return NO;
  }

  AccountCapabilities capabilities =
      identityManager
          ->FindExtendedAccountInfo(identityManager->GetPrimaryAccountInfo(
              signin::ConsentLevel::kSignin))
          .capabilities;

  return capabilities.can_use_model_execution_features() ==
         signin::Tribool::kTrue;
}

// Shows GLIC consent.
- (void)showGLICConsent {
  _glicCoordinator = [[GLICCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser];

  [_glicCoordinator start];
}

// Prepares GLIC overlay.
- (void)prepareGLICOverlay {
  // Cancel any ongoing page context operation.
  if (_pageContextWrapper) {
    _pageContextWrapper = nil;
  }

  // Configure the callback to be executed once the page context is ready.
  __weak __typeof(self) weakSelf = self;
  base::OnceCallback<void(
      std::unique_ptr<optimization_guide::proto::PageContext>)>
      page_context_completion_callback = base::BindOnce(
          ^void(std::unique_ptr<optimization_guide::proto::PageContext>
                    page_context) {
            [weakSelf openGLICOverlayForPage:std::move(page_context)];
          });

  // Collect the PageContext and execute the callback once it's ready.
  _pageContextWrapper = [[PageContextWrapper alloc]
        initWithWebState:self.browser->GetWebStateList()->GetActiveWebState()
      completionCallback:std::move(page_context_completion_callback)];
  [_pageContextWrapper setShouldGetInnerText:YES];
  [_pageContextWrapper setShouldGetSnapshot:YES];
  [_pageContextWrapper populatePageContextFieldsAsync];
}

// Opens the GLIC overlay with a given page context.
- (void)openGLICOverlayForPage:
    (std::unique_ptr<optimization_guide::proto::PageContext>)pageContext {
  GlicService* glicService = GlicServiceFactory::GetForProfile(self.profile);
  glicService->PresentOverlayOnViewController(self.baseViewController,
                                              std::move(pageContext));
  _pageContextWrapper = nil;

  [self.pageActionMenuHandler dismissPageActionMenu];
}

@end
