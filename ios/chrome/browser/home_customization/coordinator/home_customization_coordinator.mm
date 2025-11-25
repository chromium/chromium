// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/coordinator/home_customization_coordinator.h"

#import "base/feature_list.h"
#import "components/image_fetcher/ios/ios_image_data_fetcher_wrapper.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/commerce/model/shopping_service_factory.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_visibility_browser_agent.h"
#import "ios/chrome/browser/google/model/google_logo_service_factory.h"
#import "ios/chrome/browser/home_customization/coordinator/home_customization_background_configuration_mediator.h"
#import "ios/chrome/browser/home_customization/coordinator/home_customization_background_picker_action_sheet_coordinator.h"
#import "ios/chrome/browser/home_customization/coordinator/home_customization_delegate.h"
#import "ios/chrome/browser/home_customization/coordinator/home_customization_mediator.h"
#import "ios/chrome/browser/home_customization/model/home_background_customization_service.h"
#import "ios/chrome/browser/home_customization/model/home_background_customization_service_factory.h"
#import "ios/chrome/browser/home_customization/model/user_uploaded_image_manager_factory.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_background_color_picker_view_controller.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_background_picker_presentation_delegate.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_discover_view_controller.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_magic_stack_view_controller.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_main_view_controller.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_search_engine_logo_mediator_provider.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"
#import "ios/chrome/browser/image_fetcher/model/image_fetcher_service_factory.h"
#import "ios/chrome/browser/ntp/search_engine_logo/mediator/search_engine_logo_mediator.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_palette_util.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

// The height of the menu's initial detent, which roughly represents a header
// and 3 cells.
const CGFloat kInitialDetentHeight = 350;

// The corner radius of the customization menu sheet.
CGFloat const kSheetCornerRadius = 30;

}  // namespace

@interface HomeCustomizationCoordinator () <
    UISheetPresentationControllerDelegate,
    HomeCustomizationBackgroundPickerPresentationDelegate,
    HomeCustomizationMainViewControllerDelegate,
    HomeCustomizationSearchEngineLogoMediatorProvider> {
  // Displays the background picker action sheet.
  HomeCustomizationBackgroundPickerActionSheetCoordinator*
      _backgroundPickerActionSheetCoordinator;

  // Holds strong references to all active SearchEngineLogoMediator instances.
  // This ensures that each mediator remains alive long enough to complete its
  // asynchronous fetch callbacks, preventing mediators from being deallocated
  // before their configuration requests return.
  NSMutableDictionary<NSString*, SearchEngineLogoMediator*>*
      _activeSearchEngineLogoMediator;

  // The Background customization service for getting current and recently used
  // backgrounds.
  raw_ptr<HomeBackgroundCustomizationService, DanglingUntriaged>
      _backgroundService;

  // The mediator for background configuration generation and interactions.
  HomeCustomizationBackgroundConfigurationMediator*
      _backgroundConfigurationMediator;

  // Transparent overlay view used to dim the background content when presenting
  // half sheets.
  UIView* _dimView;
}

// The main page of the customization menu.
@property(nonatomic, strong)
    HomeCustomizationMainViewController* mainViewController;

// The Magic Stack page of the customization menu.
@property(nonatomic, strong)
    HomeCustomizationMagicStackViewController* magicStackViewController;

// The Discover page of the customization menu.
@property(nonatomic, strong)
    HomeCustomizationDiscoverViewController* discoverViewController;

// The mediator for the Home customization menu.
@property(nonatomic, strong) HomeCustomizationMediator* mediator;

// This menu consists of several sheets that can be overlayed on top of each
// other, each representing a submenu.
// This property points to the view controller that is at the base of the stack.
@property(nonatomic, weak) UIViewController* firstPageViewController;

// This property points to the view controller that is at the top of the stack.
@property(nonatomic, weak) UIViewController* currentPageViewController;

@end

@implementation HomeCustomizationCoordinator

#pragma mark - ChromeCoordinator

- (void)start {
  _activeSearchEngineLogoMediator = [NSMutableDictionary dictionary];
  _backgroundService =
      HomeBackgroundCustomizationServiceFactory::GetForProfile(self.profile);

  _mediator = [[HomeCustomizationMediator alloc]
                     initWithPrefService:self.profile->GetPrefs()
      discoverFeedVisibilityBrowserAgent:DiscoverFeedVisibilityBrowserAgent::
                                             FromBrowser(self.browser)
                         shoppingService:commerce::ShoppingServiceFactory::
                                             GetForProfile(self.profile)];
  _mediator.navigationDelegate = self;

  if (IsNTPBackgroundCustomizationEnabled() &&
      !_backgroundService->IsCustomizationDisabledOrColorManagedByPolicy()) {
    UserUploadedImageManager* userUploadedImageManager =
        UserUploadedImageManagerFactory::GetForProfile(self.profile);
    image_fetcher::ImageFetcherService* imageFetcherService =
        ImageFetcherServiceFactory::GetForProfile(self.profile);
    image_fetcher::ImageFetcher* imageFetcher =
        imageFetcherService->GetImageFetcher(
            image_fetcher::ImageFetcherConfig::kDiskCacheOnly);
    _backgroundConfigurationMediator =
        [[HomeCustomizationBackgroundConfigurationMediator alloc]
            initWithBackgroundCustomizationService:_backgroundService
                                      imageFetcher:imageFetcher
                        homeBackgroundImageService:nil
                          userUploadedImageManager:userUploadedImageManager];
  }

  // The Customization menu consists of a stack of presenting view controllers.
  // Since the `baseViewController` is at the root of this stack, it is set as
  // the first page.
  _currentPageViewController = self.baseViewController;

  [super start];
}

- (void)stop {
  [_backgroundConfigurationMediator saveCurrentTheme];

  [self.baseViewController dismissViewControllerAnimated:YES completion:nil];

  [self dismissBackgroundPickerActionSheet];

  if ([self.browser->GetCommandDispatcher()
          dispatchingForProtocol:@protocol(SnackbarCommands)]) {
    [HandlerForProtocol(self.browser->GetCommandDispatcher(), SnackbarCommands)
        dismissAllSnackbars];
  }

  _mediator = nil;
  _mainViewController = nil;
  _magicStackViewController = nil;
  _discoverViewController = nil;
  _dimView = nil;

  // Enable accessibility in the presenting view, as UIKit doesn't enable it
  // automatically.
  self.currentPageViewController.presentingViewController.view
      .accessibilityElementsHidden = NO;

  [super stop];
}

#pragma mark - Public

- (void)updateMenuData {
  if (self.mainViewController) {
    [self.mediator configureMainPageData];
    [_backgroundConfigurationMediator loadRecentlyUsedBackgroundConfigurations];
  }

  if (self.magicStackViewController) {
    [self.mediator configureMagicStackPageData];
  }

  if (self.discoverViewController) {
    [self.mediator configureDiscoverPageData];
  }
}

#pragma mark - HomeCustomizationNavigationDelegate

- (void)presentCustomizationMenuPage:(CustomizationMenuPage)page {
  UIViewController* menuPage = [self createMenuPage:page];

  // True if this is the first page being presented in the half sheet hierarchy.
  BOOL isFirstPagePresentation =
      self.baseViewController == self.currentPageViewController;

  // If this is the first page being presented, set a reference to it in
  // `firstPageViewController`.
  if (isFirstPagePresentation) {
    self.firstPageViewController = menuPage;
    _dimView = [[UIView alloc] init];
    _dimView.translatesAutoresizingMaskIntoConstraints = NO;
    _dimView.backgroundColor = UIColor.clearColor;

    // Add a tap gesture recognizer to the dim view so that tapping outside the
    // presented half sheet (on the dimmed view) can trigger dismissal.
    UIGestureRecognizer* tapGesture = [[UITapGestureRecognizer alloc]
        initWithTarget:self
                action:@selector(handleDimViewTap:)];
    [_dimView addGestureRecognizer:tapGesture];
  }

  [self.currentPageViewController presentViewController:menuPage
                                               animated:YES
                                             completion:nil];

  id<UIViewControllerTransitionCoordinator> transitionCoordinator =
      self.baseViewController.transitionCoordinator;

  __weak UIView* weakDimView = _dimView;

  // Add the dim view alongside the half sheet presentation.
  // Using `animateAlongsideTransition` ensures we have access to
  // `context.containerView` so the dim view can be inserted correctly
  // into the presentation hierarchy.
  [transitionCoordinator
      animateAlongsideTransition:^(
          id<UIViewControllerTransitionCoordinatorContext> context) {
        if (!weakDimView) {
          return;
        }
        [context.containerView insertSubview:weakDimView atIndex:0];
        AddSameConstraints(weakDimView, context.containerView);
      }
                      completion:nil];

  self.currentPageViewController = menuPage;

  // Set the currently presented modal as the interactable one for voiceover.
  self.currentPageViewController.view.accessibilityViewIsModal = YES;

  // Disable accessibility in the presenting view, as UIKit doesn't disable it
  // automatically.
  menuPage.presentingViewController.view.accessibilityElementsHidden = YES;
}

- (void)dismissMenuPage {
  [self dismissCurrentPageBySwipe:NO presentationController:nil];
}

- (void)navigateToURL:(GURL)URL {
  UrlLoadingBrowserAgent::FromBrowser(self.browser)
      ->Load(UrlLoadParams::InCurrentTab(URL));
  [self.delegate dismissCustomizationMenu];
}

#pragma mark - UISheetPresentationControllerDelegate

- (void)presentationControllerWillDismiss:
    (UIPresentationController*)presentationController {
  [HandlerForProtocol(self.browser->GetCommandDispatcher(), SnackbarCommands)
      dismissAllSnackbars];
}

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self dismissCurrentPageBySwipe:YES
           presentationController:presentationController];
  [self dismissBackgroundPickerActionSheet];
}

#pragma mark - Private

// Creates a view controller for a page in the menu.
- (UIViewController*)createMenuPage:(CustomizationMenuPage)page {
  auto detentResolver = ^CGFloat(
      id<UISheetPresentationControllerDetentResolutionContext> context) {
    return kInitialDetentHeight;
  };
  UISheetPresentationControllerDetent* initialDetent =
      [UISheetPresentationControllerDetent
          customDetentWithIdentifier:kBottomSheetDetentIdentifier
                            resolver:detentResolver];
  NSMutableArray<UISheetPresentationControllerDetent*>* detents = [@[
    initialDetent,
  ] mutableCopy];

  UIViewController* menuPage;

  // Create view controller for the `page` and configure it with the mediator.
  switch (page) {
    case CustomizationMenuPage::kMain: {
      self.mainViewController =
          [[HomeCustomizationMainViewController alloc] init];
      self.mainViewController.snackbarCommandHandler = HandlerForProtocol(
          self.browser->GetCommandDispatcher(), SnackbarCommands);
      self.mainViewController.delegate = self;
      self.mainViewController.backgroundPickerPresentationDelegate = self;
      self.mainViewController.mutator = _mediator;
      self.mainViewController.customizationMutator =
          _backgroundConfigurationMediator;
      self.mainViewController.searchEngineLogoMediatorProvider = self;
      self.mainViewController.customizationDisabledByPolicy =
          _backgroundService->IsCustomizationDisabledOrColorManagedByPolicy();
      self.mediator.mainPageConsumer = self.mainViewController;
      _backgroundConfigurationMediator.consumer = self.mainViewController;
      [self.mediator configureMainPageData];
      [_backgroundConfigurationMediator
          loadRecentlyUsedBackgroundConfigurations];
      menuPage = self.mainViewController;

      __weak __typeof(self) weakSelf = self;
      auto expandedDetentResolver = ^CGFloat(
          id<UISheetPresentationControllerDetentResolutionContext> context) {
        return [weakSelf detentHeightForMainViewControllerExpanded];
      };

      UISheetPresentationControllerDetent* expandedDetent =
          [UISheetPresentationControllerDetent
              customDetentWithIdentifier:kBottomSheetExpandedDetentIdentifier
                                resolver:expandedDetentResolver];
      [detents addObject:expandedDetent];

      break;
    }
    case CustomizationMenuPage::kMagicStack: {
      self.magicStackViewController =
          [[HomeCustomizationMagicStackViewController alloc] init];
      self.magicStackViewController.mutator = _mediator;
      self.mediator.magicStackPageConsumer = self.magicStackViewController;
      [self.mediator configureMagicStackPageData];
      menuPage = self.magicStackViewController;
      break;
    }
    case CustomizationMenuPage::kDiscover: {
      self.discoverViewController =
          [[HomeCustomizationDiscoverViewController alloc] init];
      self.discoverViewController.mutator = _mediator;
      self.mediator.discoverPageConsumer = self.discoverViewController;
      [self.mediator configureDiscoverPageData];
      menuPage = self.discoverViewController;
      break;
    }
    case CustomizationMenuPage::kUnknown:
      NOTREACHED();
  }

  // Configure the navigation controller.
  UINavigationController* navigationController =
      [[UINavigationController alloc] initWithRootViewController:menuPage];

  if (@available(iOS 26, *)) {
    menuPage.view.backgroundColor = [UIColor clearColor];
  } else {
    menuPage.view.backgroundColor =
        [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
  }

  navigationController.modalPresentationStyle = UIModalPresentationFormSheet;

  // Configure the presentation controller with a custom initial detent.
  UISheetPresentationController* presentationController =
      navigationController.sheetPresentationController;
  presentationController.prefersEdgeAttachedInCompactHeight = YES;
  presentationController.preferredCornerRadius = kSheetCornerRadius;
  presentationController.delegate = self;

  presentationController.detents = detents;
  presentationController.prefersScrollingExpandsWhenScrolledToEdge = NO;
  presentationController.selectedDetentIdentifier =
      kBottomSheetDetentIdentifier;
  presentationController.largestUndimmedDetentIdentifier =
      [self currentLargestUndimmedDetentIdentifier];

  return navigationController;
}

// Dismisses the background picker action sheet and clears its reference.
- (void)dismissBackgroundPickerActionSheet {
  [_backgroundPickerActionSheetCoordinator stop];
  _backgroundPickerActionSheetCoordinator = nil;
}

// Handles the dismissal of the current menu page, either explicitly for a tap
// on the dismiss button or implicitly for a swipe to dismiss gesture.
- (void)dismissCurrentPageBySwipe:(BOOL)bySwipe
           presentationController:
               (UIPresentationController*)presentationController {
  // If the page being dismissed is the first page of the stack, then the entire
  // menu should be dismissed. Otherwise, dismiss the topmost page and update
  // the currently visible page.
  if (self.currentPageViewController == self.firstPageViewController) {
    [self.delegate dismissCustomizationMenu];
  } else {
    // If the dismissal was not triggered natively (e.g., a swipe gesture), the
    // view controller should be dismissed programmatically.
    if (!bySwipe) {
      [self.currentPageViewController dismissViewControllerAnimated:YES
                                                         completion:nil];
      self.currentPageViewController =
          self.currentPageViewController.presentingViewController;
    } else {
      self.currentPageViewController =
          presentationController.presentingViewController;
    }

    // The presenting page should become interactable for voiceover.
    self.currentPageViewController.view.accessibilityViewIsModal = YES;
  }
}

- (CGFloat)detentHeightForMainViewControllerExpanded {
  CGFloat height = self.mainViewController.viewContentHeight;
  return (height < kInitialDetentHeight)
             ? UISheetPresentationControllerDetentInactive
             : height;
}

// Returns the identifier of the detent that should currently be the largest
// undimmed detent. This is required because if the largest undimmed detent
// currently has an inactive height, UIKit dims everything. So when that detent
// is inactive, the largest undimmed detent must change.
- (NSString*)currentLargestUndimmedDetentIdentifier {
  return ([self detentHeightForMainViewControllerExpanded] ==
          UISheetPresentationControllerDetentInactive)
             ? kBottomSheetDetentIdentifier
             : kBottomSheetExpandedDetentIdentifier;
}

// Called when the user taps on the dim view to dismiss the presented half
// sheet.
- (void)handleDimViewTap:(UITapGestureRecognizer*)gesture {
  [self.delegate dismissCustomizationMenu];
}

#pragma mark - HomeCustomizationBackgroundPickerPresentationDelegate

- (void)showBackgroundPickerOptionsFromSourceView:(UIView*)sourceView {
  _backgroundPickerActionSheetCoordinator =
      [[HomeCustomizationBackgroundPickerActionSheetCoordinator alloc]
          initWithBaseViewController:self.mainViewController
                             browser:self.browser
                          sourceView:sourceView];
  _backgroundPickerActionSheetCoordinator.presentationDelegate = self;
  _backgroundPickerActionSheetCoordinator.searchEngineLogoMediatorProvider =
      self;

  [_backgroundPickerActionSheetCoordinator start];
  // Disable customization interactions while the background picker views are
  // open so the user can't choose a new background from the main menu while in
  // the process of dismissing the picker views.
  self.mainViewController.backgroundCustomizationUserInteractionEnabled = NO;
  self.currentPageViewController.view.accessibilityElementsHidden = YES;
}

- (void)dismissBackgroundPicker {
  [self.delegate dismissCustomizationMenu];
}

- (void)cancelBackgroundPicker {
  // Reenable interaction when the picker is canceled, as the main menu is now
  // active again.
  self.mainViewController.backgroundCustomizationUserInteractionEnabled = YES;
  self.currentPageViewController.view.accessibilityElementsHidden = NO;

  [self dismissBackgroundPickerActionSheet];
}

#pragma mark - HomeCustomizationSearchEngineLogoMediator

- (SearchEngineLogoMediator*)provideSearchEngineLogoMediatorForKey:
    (NSString*)key {
  SearchEngineLogoMediator* searchEngineLogoMediator =
      _activeSearchEngineLogoMediator[key];
  if (!searchEngineLogoMediator) {
    ProfileIOS* profile = self.browser->GetProfile();
    web::WebState* webState =
        self.browser->GetWebStateList()->GetActiveWebState();
    TemplateURLService* templateURLService =
        ios::TemplateURLServiceFactory::GetForProfile(profile);
    GoogleLogoService* logoService =
        GoogleLogoServiceFactory::GetForProfile(profile);
    UrlLoadingBrowserAgent* URLLoadingBrowserAgent =
        UrlLoadingBrowserAgent::FromBrowser(self.browser);
    scoped_refptr<network::SharedURLLoaderFactory> sharedURLLoaderFactory =
        profile->GetSharedURLLoaderFactory();
    BOOL offTheRecord = profile->IsOffTheRecord();
    searchEngineLogoMediator = [[SearchEngineLogoMediator alloc]
              initWithWebState:webState
            templateURLService:templateURLService
                   logoService:logoService
        URLLoadingBrowserAgent:URLLoadingBrowserAgent
        sharedURLLoaderFactory:sharedURLLoaderFactory
                  offTheRecord:offTheRecord];
    _activeSearchEngineLogoMediator[key] = searchEngineLogoMediator;
  }

  return searchEngineLogoMediator;
}

#pragma mark - HomeCustomizationMainViewControllerDelegate

- (void)viewContentHeightChangedInHomeCustomizationViewController:
    (HomeCustomizationMainViewController*)viewController {
  [viewController.sheetPresentationController invalidateDetents];
  // Make sure to update the largest undimmed detent identifier because the old
  // largest could have changed activation state.
  viewController.sheetPresentationController.largestUndimmedDetentIdentifier =
      [self currentLargestUndimmedDetentIdentifier];
}

@end
