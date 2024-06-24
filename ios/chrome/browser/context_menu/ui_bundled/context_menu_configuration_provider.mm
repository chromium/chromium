// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/context_menu/ui_bundled/context_menu_configuration_provider.h"

#import "base/ios/ios_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "components/prefs/pref_service.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/context_menu/ui_bundled/context_menu_configuration_provider+Testing.h"
#import "ios/chrome/browser/context_menu/ui_bundled/context_menu_utils.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/photos/model/photos_availability.h"
#import "ios/chrome/browser/photos/model/photos_metrics.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/reading_list/model/reading_list_browser_agent.h"
#import "ios/chrome/browser/search_engines/model/search_engines_util.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group_utils.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/lens_commands.h"
#import "ios/chrome/browser/shared/public/commands/mini_map_commands.h"
#import "ios/chrome/browser/shared/public/commands/reading_list_add_command.h"
#import "ios/chrome/browser/shared/public/commands/search_image_with_lens_command.h"
#import "ios/chrome/browser/shared/public/commands/unit_conversion_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/image/image_copier.h"
#import "ios/chrome/browser/shared/ui/util/image/image_saver.h"
#import "ios/chrome/browser/shared/ui/util/pasteboard_util.h"
#import "ios/chrome/browser/shared/ui/util/url_with_title.h"
#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_commands.h"
#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_scene_agent.h"
#import "ios/chrome/browser/ui/lens/lens_availability.h"
#import "ios/chrome/browser/ui/lens/lens_entrypoint.h"
#import "ios/chrome/browser/ui/menu/browser_action_factory.h"
#import "ios/chrome/browser/ui/menu/menu_histograms.h"
#import "ios/chrome/browser/url_loading/model/image_search_param_generator.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/browser/web/model/image_fetch/image_fetch_tab_helper.h"
#import "ios/chrome/browser/web/model/web_navigation_util.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/context_menu/context_menu_api.h"
#import "ios/public/provider/chrome/browser/lens/lens_api.h"
#import "ios/web/common/features.h"
#import "ios/web/common/url_scheme_util.h"
#import "ios/web/public/ui/context_menu_params.h"
#import "ios/web/public/web_state.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace {

// Maximum length for a context menu title formed from a URL.
const NSUInteger kContextMenuMaxURLTitleLength = 100;
// Character to append to context menut titles that are truncated.
NSString* const kContextMenuEllipsis = @"…";
// Maximum length for a context menu title formed from an address, date or phone
// number experience.
const NSUInteger kContextMenuMaxTitleLength = 30;

}  // namespace

@interface ContextMenuConfigurationProvider ()

// Helper for saving images.
@property(nonatomic, strong) ImageSaver* imageSaver;
// Helper for copying images.
@property(nonatomic, strong) ImageCopier* imageCopier;

@property(nonatomic, assign) Browser* browser;

@property(nonatomic, weak) UIViewController* baseViewController;

@property(nonatomic, assign, readonly) web::WebState* currentWebState;

@end

@implementation ContextMenuConfigurationProvider

- (instancetype)initWithBrowser:(Browser*)browser
             baseViewController:(UIViewController*)baseViewController {
  self = [super init];
  if (self) {
    _browser = browser;
    _baseViewController = baseViewController;
    _imageSaver = [[ImageSaver alloc] initWithBrowser:self.browser];
    _imageCopier = [[ImageCopier alloc] initWithBrowser:self.browser];
  }
  return self;
}

- (void)stop {
  _browser = nil;
  _baseViewController = nil;
  [_imageSaver stop];
  _imageSaver = nil;
  [_imageCopier stop];
  _imageCopier = nil;
}

- (void)dealloc {
  CHECK(!_browser);
}

- (UIContextMenuConfiguration*)
    contextMenuConfigurationForWebState:(web::WebState*)webState
                                 params:(web::ContextMenuParams)params {
  UIContextMenuActionProvider actionProvider =
      [self contextMenuActionProviderForWebState:webState params:params];
  if (!actionProvider) {
    return nil;
  }
  return
      [UIContextMenuConfiguration configurationWithIdentifier:nil
                                              previewProvider:nil
                                               actionProvider:actionProvider];
}

#pragma mark - Properties

- (web::WebState*)currentWebState {
  return self.browser ? self.browser->GetWebStateList()->GetActiveWebState()
                      : nullptr;
}

#pragma mark - Private

// Returns an action based contextual menu for a given web state (link, image,
// copy and intent detection actions).
- (UIContextMenuActionProvider)
    contextMenuActionProviderForWebState:(web::WebState*)webState
                                  params:(web::ContextMenuParams)params {
  // Reset the URL.
  _URLToLoad = GURL();

  // Prevent context menu from displaying for a tab which is no longer the
  // current one.
  if (webState != self.currentWebState) {
    return nil;
  }

  const GURL linkURL = params.link_url;
  const bool isLink = linkURL.is_valid();
  const GURL imageURL = params.src_url;
  const bool isImage = imageURL.is_valid();

  DCHECK(self.browser->GetBrowserState());
  const bool isOffTheRecord = self.browser->GetBrowserState()->IsOffTheRecord();

  const GURL& lastCommittedURL = webState->GetLastCommittedURL();
  web::Referrer referrer(lastCommittedURL, web::ReferrerPolicyDefault);

  NSMutableArray<UIMenuElement*>* menuElements = [[NSMutableArray alloc] init];
  // TODO(crbug.com/40823789) add scenario for not a link and not an image.
  MenuScenarioHistogram menuScenario =
      isImage && isLink ? kMenuScenarioHistogramContextMenuImageLink
      : isImage         ? kMenuScenarioHistogramContextMenuImage
                        : kMenuScenarioHistogramContextMenuLink;

  if (isLink) {
    [menuElements
        addObjectsFromArray:[self contextMenuElementsForLink:linkURL
                                                    scenario:menuScenario
                                                    referrer:referrer
                                              isOffTheRecord:isOffTheRecord
                                                      params:params]];
  }
  if (isImage) {
    [menuElements
        addObjectsFromArray:[self imageContextMenuElementsWithURL:imageURL
                                                         scenario:menuScenario
                                                         referrer:referrer
                                                   isOffTheRecord:isOffTheRecord
                                                           isLink:isLink
                                                         webState:webState
                                                           params:params]];
  }

  // Insert any provided menu items. Do after Link and/or Image to allow
  // inserting at beginning or adding to end.
  ElementsToAddToContextMenu* result =
      ios::provider::GetContextMenuElementsToAdd(
          webState, params, self.baseViewController,
          HandlerForProtocol(self.browser->GetCommandDispatcher(),
                             MiniMapCommands),
          HandlerForProtocol(self.browser->GetCommandDispatcher(),
                             UnitConversionCommands));
  NSString* menuTitle = nil;
  if (result && result.elements) {
    [menuElements addObjectsFromArray:result.elements];
    menuTitle = result.title;
    if (menuTitle.length > kContextMenuMaxTitleLength) {
      menuTitle = [[menuTitle substringToIndex:kContextMenuMaxTitleLength - 1]
          stringByAppendingString:kContextMenuEllipsis];
    }
  }

  if (menuElements.count == 0) {
    return nil;
  }

  if (isLink || isImage) {
    menuTitle = GetContextMenuTitle(params);

    // Truncate context meny titles that originate from URLs, leaving text
    // titles untruncated.
    if (!IsImageTitle(params) &&
        menuTitle.length > kContextMenuMaxURLTitleLength + 1) {
      menuTitle = [[menuTitle substringToIndex:kContextMenuMaxURLTitleLength]
          stringByAppendingString:kContextMenuEllipsis];
    }
  }

  UIMenu* menu = [UIMenu menuWithTitle:menuTitle children:menuElements];

  UIContextMenuActionProvider actionProvider =
      ^(NSArray<UIMenuElement*>* suggestedActions) {
        RecordMenuShown(menuScenario);
        return menu;
      };

  return actionProvider;
}

// Returns the elements of the context menu related to links.
- (NSArray<UIMenuElement*>*)
    contextMenuElementsForLink:(GURL)linkURL
                      scenario:(MenuScenarioHistogram)scenario
                      referrer:(web::Referrer)referrer
                isOffTheRecord:(BOOL)isOffTheRecord
                        params:(web::ContextMenuParams)params {
  BrowserActionFactory* actionFactory =
      [[BrowserActionFactory alloc] initWithBrowser:self.browser
                                           scenario:scenario];

  NSMutableArray<UIMenuElement*>* linkMenuElements =
      [[NSMutableArray alloc] init];

  __weak __typeof(self) weakSelf = self;

  // Array for the actions/menus used to open a link.
  NSMutableArray<UIMenuElement*>* linkOpeningElements =
      [[NSMutableArray alloc] init];

  _URLToLoad = linkURL;
  base::RecordAction(
      base::UserMetricsAction("MobileWebContextMenuLinkImpression"));
  if (web::UrlHasWebScheme(linkURL)) {
    // Open in New Tab.
    UrlLoadParams loadParams = UrlLoadParams::InNewTab(linkURL);
    loadParams.SetInBackground(YES);
    loadParams.web_params.referrer = referrer;
    loadParams.in_incognito = isOffTheRecord;
    loadParams.append_to = OpenPosition::kCurrentTab;
    loadParams.origin_point = [params.view convertPoint:params.location
                                                 toView:nil];

    UIAction* openNewTab = [actionFactory actionToOpenInNewTabWithBlock:^{
      ContextMenuConfigurationProvider* strongSelf = weakSelf;
      if (!strongSelf) {
        return;
      }
      UrlLoadingBrowserAgent::FromBrowser(strongSelf.browser)->Load(loadParams);
    }];
    [linkOpeningElements addObject:openNewTab];

    if (IsTabGroupInGridEnabled()) {
      // Open in Tab Group.
      UIMenuElement* openLinkInGroupMenu =
          [self openLinkInGroupMenuElement:linkURL
                                  scenario:scenario
                                  referrer:referrer
                            isOffTheRecord:isOffTheRecord
                                    params:params];
      [linkOpeningElements addObject:openLinkInGroupMenu];
    }

    if (!isOffTheRecord) {
      // Open in Incognito Tab.
      UIAction* openIncognitoTab;
      openIncognitoTab =
          [actionFactory actionToOpenInNewIncognitoTabWithURL:linkURL
                                                   completion:nil];
      [linkOpeningElements addObject:openIncognitoTab];
    }

    if (base::ios::IsMultipleScenesSupported()) {
      // Open in New Window.

      NSUserActivity* newWindowActivity = ActivityToLoadURL(
          WindowActivityContextMenuOrigin, linkURL, referrer, isOffTheRecord);
      UIAction* openNewWindow =
          [actionFactory actionToOpenInNewWindowWithActivity:newWindowActivity];

      [linkOpeningElements addObject:openNewWindow];
    }

    UIMenu* linkOpeningMenu = [UIMenu menuWithTitle:@""
                                              image:nil
                                         identifier:nil
                                            options:UIMenuOptionsDisplayInline
                                           children:linkOpeningElements];

    [linkMenuElements addObject:linkOpeningMenu];

    if (linkURL.SchemeIsHTTPOrHTTPS()) {
      NSString* innerText = params.text;
      if ([innerText length] > 0) {
        // Add to reading list.
        UIAction* addToReadingList =
            [actionFactory actionToAddToReadingListWithBlock:^{
              ContextMenuConfigurationProvider* strongSelf = weakSelf;
              if (!strongSelf) {
                return;
              }

              ReadingListAddCommand* command =
                  [[ReadingListAddCommand alloc] initWithURL:linkURL
                                                       title:innerText];
              ReadingListBrowserAgent* readingListBrowserAgent =
                  ReadingListBrowserAgent::FromBrowser(strongSelf.browser);
              readingListBrowserAgent->AddURLsToReadingList(command.URLs);
            }];
        [linkMenuElements addObject:addToReadingList];
      }
    }
  }

  // Copy Link.
  UIAction* copyLink =
      [actionFactory actionToCopyURL:[[CrURL alloc] initWithGURL:linkURL]];
  [linkMenuElements addObject:copyLink];

  return linkMenuElements;
}

// Returns the elements of the context menu related to image links.
- (NSArray<UIMenuElement*>*)
    imageContextMenuElementsWithURL:(GURL)imageURL
                           scenario:(MenuScenarioHistogram)scenario
                           referrer:(web::Referrer)referrer
                     isOffTheRecord:(BOOL)isOffTheRecord
                             isLink:(BOOL)isLink
                           webState:(web::WebState*)webState
                             params:(web::ContextMenuParams)params {
  BrowserActionFactory* actionFactory =
      [[BrowserActionFactory alloc] initWithBrowser:self.browser
                                           scenario:scenario];

  NSMutableArray<UIMenuElement*>* imageMenuElements =
      [[NSMutableArray alloc] init];
  base::RecordAction(
      base::UserMetricsAction("MobileWebContextMenuImageImpression"));

  __weak __typeof(self) weakSelf = self;

  // Image saving.
  NSArray<UIMenuElement*>* imageSavingElements =
      [self imageSavingElementsWithURL:imageURL
                              scenario:scenario
                              referrer:referrer
                              webState:webState];
  [imageMenuElements addObjectsFromArray:imageSavingElements];

  // Copy Image.
  UIAction* copyImage = [actionFactory actionCopyImageWithBlock:^{
    ContextMenuConfigurationProvider* strongSelf = weakSelf;
    if (!strongSelf || !strongSelf.baseViewController) {
      return;
    }
    [strongSelf.imageCopier copyImageAtURL:imageURL
                                  referrer:referrer
                                  webState:strongSelf.currentWebState
                        baseViewController:strongSelf.baseViewController];
  }];
  [imageMenuElements addObject:copyImage];

  // Image opening.
  NSArray<UIMenuElement*>* imageOpeningElements =
      [self imageOpeningElementsWithURL:imageURL
                               scenario:scenario
                               referrer:referrer
                         isOffTheRecord:isOffTheRecord
                                 isLink:isLink
                                 params:params];
  [imageMenuElements addObjectsFromArray:imageOpeningElements];

  // Image searching.
  NSArray<UIMenuElement*>* imageSearchingElements =
      [self imageSearchingElementsWithURL:imageURL
                                 scenario:scenario
                                 referrer:referrer];
  [imageMenuElements addObjectsFromArray:imageSearchingElements];

  return imageMenuElements;
}

// Searches an image with the given `imageURL` and `referrer`, optionally using
// Lens.
- (void)searchImageWithURL:(GURL)imageURL
                 usingLens:(BOOL)usingLens
                  referrer:(web::Referrer)referrer {
  ImageFetchTabHelper* imageFetcher =
      ImageFetchTabHelper::FromWebState(self.currentWebState);
  DCHECK(imageFetcher);
  __weak ContextMenuConfigurationProvider* weakSelf = self;
  imageFetcher->GetImageData(imageURL, referrer, ^(NSData* data) {
    if (usingLens) {
      [weakSelf searchImageUsingLensWithData:data];
    } else {
      [weakSelf searchByImageData:data imageURL:imageURL];
    }
  });
}

// Starts a reverse image search based on `imageData` and `imageURL` in a new
// tab.
- (void)searchByImageData:(NSData*)imageData imageURL:(const GURL&)URL {
  web::NavigationManager::WebLoadParams webParams =
      ImageSearchParamGenerator::LoadParamsForImageData(
          imageData, URL,
          ios::TemplateURLServiceFactory::GetForBrowserState(
              self.browser->GetBrowserState()));
  const BOOL isIncognito = self.browser->GetBrowserState()->IsOffTheRecord();

  // Apply variation header data to the params.
  NSMutableDictionary<NSString*, NSString*>* combinedExtraHeaders =
      [web_navigation_util::VariationHeadersForURL(webParams.url, isIncognito)
          mutableCopy];
  [combinedExtraHeaders addEntriesFromDictionary:webParams.extra_headers];
  webParams.extra_headers = [combinedExtraHeaders copy];

  UrlLoadParams params = UrlLoadParams::InNewTab(webParams);
  params.in_incognito = isIncognito;
  UrlLoadingBrowserAgent::FromBrowser(self.browser)->Load(params);
}

// Searches an image with Lens using the given `imageData`.
- (void)searchImageUsingLensWithData:(NSData*)imageData {
  id<LensCommands> handler =
      HandlerForProtocol(_browser->GetCommandDispatcher(), LensCommands);
  UIImage* image = [UIImage imageWithData:imageData];
  SearchImageWithLensCommand* command = [[SearchImageWithLensCommand alloc]
      initWithImage:image
         entryPoint:LensEntrypoint::ContextMenu];
  [handler searchImageWithLens:command];
}

// Returns the context menu element to open a URL (image or link) in a group.
- (UIMenuElement*)openLinkInGroupMenuElement:(GURL)linkURL
                                    scenario:(MenuScenarioHistogram)scenario
                                    referrer:(web::Referrer)referrer
                              isOffTheRecord:(BOOL)isOffTheRecord
                                      params:(web::ContextMenuParams)params {
  BrowserActionFactory* actionFactory =
      [[BrowserActionFactory alloc] initWithBrowser:self.browser
                                           scenario:scenario];
  __weak __typeof(self) weakSelf = self;

  std::set<const TabGroup*> groups =
      GetAllGroupsForBrowserState(self.browser->GetBrowserState());

  auto actionResult = ^(const TabGroup* group) {
    ContextMenuConfigurationProvider* strongSelf = weakSelf;
    if (!strongSelf) {
      return;
    }
    UrlLoadParams groupLoadParams = UrlLoadParams::InNewTab(linkURL);
    groupLoadParams.SetInBackground(YES);
    groupLoadParams.web_params.referrer = referrer;
    groupLoadParams.in_incognito = isOffTheRecord;
    groupLoadParams.append_to = OpenPosition::kCurrentTab;
    groupLoadParams.origin_point = [params.view convertPoint:params.location
                                                      toView:nil];
    groupLoadParams.load_in_group = true;
    if (group) {
      groupLoadParams.tab_group = group->GetWeakPtr();
    }

    UrlLoadingBrowserAgent::FromBrowser(strongSelf.browser)
        ->Load(groupLoadParams);
  };

  return [actionFactory menuToOpenLinkInGroupWithGroups:groups
                                                  block:actionResult];
}

// Returns the context menu elements for image opening.
- (NSArray<UIMenuElement*>*)
    imageOpeningElementsWithURL:(GURL)imageURL
                       scenario:(MenuScenarioHistogram)scenario
                       referrer:(web::Referrer)referrer
                 isOffTheRecord:(BOOL)isOffTheRecord
                         isLink:(BOOL)isLink
                         params:(web::ContextMenuParams)params {
  BrowserActionFactory* actionFactory =
      [[BrowserActionFactory alloc] initWithBrowser:self.browser
                                           scenario:scenario];

  NSMutableArray<UIMenuElement*>* imageOpeningMenuElements =
      [[NSMutableArray alloc] init];

  // Open Image.
  UIAction* openImage = [actionFactory actionOpenImageWithURL:imageURL
                                                   completion:nil];
  [imageOpeningMenuElements addObject:openImage];

  // Open Image in new tab.
  UrlLoadParams loadParams = UrlLoadParams::InNewTab(imageURL);
  loadParams.SetInBackground(YES);
  loadParams.web_params.referrer = referrer;
  loadParams.in_incognito = isOffTheRecord;
  loadParams.append_to = OpenPosition::kCurrentTab;
  loadParams.origin_point = [params.view convertPoint:params.location
                                               toView:nil];

  UIAction* openImageInNewTab =
      [actionFactory actionOpenImageInNewTabWithUrlLoadParams:loadParams
                                                   completion:nil];

  // Check if the URL was a valid link to avoid having the `Open in Tab Group`
  // option twice.
  if (!IsTabGroupInGridEnabled() || isLink) {
    [imageOpeningMenuElements addObject:openImageInNewTab];
  } else {
    // Array for the actions/menus used to open an image in a new tab.
    NSMutableArray<UIMenuElement*>* imageOpeningElements =
        [[NSMutableArray alloc] init];

    [imageOpeningElements addObject:openImageInNewTab];

    // Open in Tab Group.
    UIMenuElement* openLinkInGroupMenu =
        [self openLinkInGroupMenuElement:imageURL
                                scenario:scenario
                                referrer:referrer
                          isOffTheRecord:isOffTheRecord
                                  params:params];
    [imageOpeningElements addObject:openLinkInGroupMenu];

    UIMenu* imageOpeningMenu = [UIMenu menuWithTitle:@""
                                               image:nil
                                          identifier:nil
                                             options:UIMenuOptionsDisplayInline
                                            children:imageOpeningElements];

    [imageOpeningMenuElements addObject:imageOpeningMenu];
  }

  return imageOpeningMenuElements;
}

// Returns the context menu elements for image saving.
- (NSArray<UIMenuElement*>*)
    imageSavingElementsWithURL:(GURL)imageURL
                      scenario:(MenuScenarioHistogram)scenario
                      referrer:(web::Referrer)referrer
                      webState:(web::WebState*)webState {
  const bool saveToPhotosAvailable =
      IsSaveToPhotosAvailable(self.browser->GetBrowserState());

  BrowserActionFactory* actionFactory =
      [[BrowserActionFactory alloc] initWithBrowser:self.browser
                                           scenario:scenario];

  NSMutableArray<UIMenuElement*>* imageSavingElements =
      [[NSMutableArray alloc] init];

  __weak __typeof(self) weakSelf = self;

  // Save Image.
  UIAction* saveImage = [actionFactory actionSaveImageWithBlock:^{
    ContextMenuConfigurationProvider* strongSelf = weakSelf;
    if (!strongSelf || !strongSelf.baseViewController) {
      return;
    }
    [strongSelf.imageSaver saveImageAtURL:imageURL
                                 referrer:referrer
                                 webState:strongSelf.currentWebState
                       baseViewController:strongSelf.baseViewController];
    base::UmaHistogramEnumeration(
        kSaveToPhotosContextMenuActionsHistogram,
        saveToPhotosAvailable
            ? SaveToPhotosContextMenuActions::kAvailableDidSaveImageLocally
            : SaveToPhotosContextMenuActions::kUnavailableDidSaveImageLocally);
  }];
  [imageSavingElements addObject:saveImage];

  // Save Image to Photos.
  if (saveToPhotosAvailable) {
    base::RecordAction(base::UserMetricsAction(
        "MobileWebContextMenuImageWithSaveToPhotosImpression"));
    UIAction* saveImageToPhotosAction = [actionFactory
        actionToSaveToPhotosWithImageURL:imageURL
                                referrer:referrer
                                webState:webState
                                   block:^{
                                     base::UmaHistogramEnumeration(
                                         kSaveToPhotosContextMenuActionsHistogram,
                                         SaveToPhotosContextMenuActions::
                                             kAvailableDidSaveImageToGooglePhotos);
                                   }];
    [imageSavingElements addObject:saveImageToPhotosAction];
  }

  if (IsSaveToPhotosActionImprovementEnabled() && saveToPhotosAvailable) {
    UIMenu* saveImageInMenu = [UIMenu
        menuWithTitle:l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_SAVE_IMAGE_IN)
                image:DefaultSymbolWithPointSize(kPhotoBadgeArrowDownSymbol,
                                                 kSymbolActionPointSize)
           identifier:nil
              options:UIMenuOptionsSingleSelection
             children:imageSavingElements];
    return @[ saveImageInMenu ];
  }

  return imageSavingElements;
}

// Returns the context menu elements for image searching.
- (NSArray<UIMenuElement*>*)
    imageSearchingElementsWithURL:(GURL)imageURL
                         scenario:(MenuScenarioHistogram)scenario
                         referrer:(web::Referrer)referrer {
  BrowserActionFactory* actionFactory =
      [[BrowserActionFactory alloc] initWithBrowser:self.browser
                                           scenario:scenario];

  __weak __typeof(self) weakSelf = self;

  // Search the image using Lens if Lens is enabled and available. Otherwise
  // fall back to a standard search by image experience.
  TemplateURLService* service =
      ios::TemplateURLServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());

  const BOOL useLens =
      lens_availability::CheckAndLogAvailabilityForLensEntryPoint(
          LensEntrypoint::ContextMenu,
          search_engines::SupportsSearchImageWithLens(service));

  NSMutableArray<UIMenuElement*>* imageSearchingMenuElements =
      [[NSMutableArray alloc] init];
  if (useLens) {
    UIAction* searchImageWithLensAction =
        [actionFactory actionToSearchImageUsingLensWithBlock:^{
          [weakSelf searchImageWithURL:imageURL
                             usingLens:YES
                              referrer:referrer];
        }];
    [imageSearchingMenuElements addObject:searchImageWithLensAction];
  }

  if (!useLens && search_engines::SupportsSearchByImage(service)) {
    const TemplateURL* defaultURL = service->GetDefaultSearchProvider();
    NSString* title = l10n_util::GetNSStringF(
        IDS_IOS_CONTEXT_MENU_SEARCHWEBFORIMAGE, defaultURL->short_name());
    UIAction* searchByImage = [actionFactory
        actionSearchImageWithTitle:title
                             Block:^{
                               [weakSelf searchImageWithURL:imageURL
                                                  usingLens:NO
                                                   referrer:referrer];
                             }];
    [imageSearchingMenuElements addObject:searchByImage];
  }

  return imageSearchingMenuElements;
}

@end
