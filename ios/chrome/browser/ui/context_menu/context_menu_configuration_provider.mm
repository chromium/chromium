// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/context_menu/context_menu_configuration_provider.h"

#import "base/ios/ios_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "components/prefs/pref_service.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#import "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/photos/photos_availability.h"
#import "ios/chrome/browser/photos/photos_metrics.h"
#import "ios/chrome/browser/policy/policy_util.h"
#import "ios/chrome/browser/reading_list/model/reading_list_browser_agent.h"
#import "ios/chrome/browser/search_engines/model/search_engines_util.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_browser_agent.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/lens_commands.h"
#import "ios/chrome/browser/shared/public/commands/mini_map_commands.h"
#import "ios/chrome/browser/shared/public/commands/reading_list_add_command.h"
#import "ios/chrome/browser/shared/public/commands/search_image_with_lens_command.h"
#import "ios/chrome/browser/shared/public/commands/unit_conversion_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/image/image_copier.h"
#import "ios/chrome/browser/shared/ui/util/image/image_saver.h"
#import "ios/chrome/browser/shared/ui/util/pasteboard_util.h"
#import "ios/chrome/browser/shared/ui/util/url_with_title.h"
#import "ios/chrome/browser/ui/context_menu/context_menu_configuration_provider+private.h"
#import "ios/chrome/browser/ui/context_menu/context_menu_utils.h"
#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_commands.h"
#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_scene_agent.h"
#import "ios/chrome/browser/ui/lens/lens_availability.h"
#import "ios/chrome/browser/ui/lens/lens_entrypoint.h"
#import "ios/chrome/browser/ui/menu/browser_action_factory.h"
#import "ios/chrome/browser/ui/menu/menu_histograms.h"
#import "ios/chrome/browser/url_loading/model/image_search_param_generator.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/browser/web/image_fetch/image_fetch_tab_helper.h"
#import "ios/chrome/browser/web/web_navigation_util.h"
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

// TODO(crbug.com/1318432): rafactor long method.
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
  const bool saveToPhotosAvailable =
      IsSaveToPhotosAvailable(self.browser->GetBrowserState());

  DCHECK(self.browser->GetBrowserState());
  const bool isOffTheRecord = self.browser->GetBrowserState()->IsOffTheRecord();

  const GURL& lastCommittedURL = webState->GetLastCommittedURL();
  web::Referrer referrer(lastCommittedURL, web::ReferrerPolicyDefault);

  NSMutableArray<UIMenuElement*>* menuElements = [[NSMutableArray alloc] init];
  // TODO(crbug.com/1299758) add scenario for not a link and not an image.
  MenuScenarioHistogram menuScenario =
      isImage && isLink ? MenuScenarioHistogram::kContextMenuImageLink
      : isImage         ? MenuScenarioHistogram::kContextMenuImage
                        : MenuScenarioHistogram::kContextMenuLink;

  BrowserActionFactory* actionFactory =
      [[BrowserActionFactory alloc] initWithBrowser:self.browser
                                           scenario:menuScenario];

  __weak __typeof(self) weakSelf = self;

  if (isLink) {
    _URLToLoad = linkURL;
    base::RecordAction(
        base::UserMetricsAction("MobileWebContextMenuLinkImpression"));
    if (web::UrlHasWebScheme(linkURL)) {
      // Open in New Tab.
      UrlLoadParams loadParams = UrlLoadParams::InNewTab(linkURL);
      loadParams.SetInBackground(YES);
      loadParams.in_incognito = isOffTheRecord;
      loadParams.append_to = OpenPosition::kCurrentTab;
      loadParams.web_params.referrer = referrer;
      loadParams.origin_point = [params.view convertPoint:params.location
                                                   toView:nil];
      UIAction* openNewTab = [actionFactory actionToOpenInNewTabWithBlock:^{
        ContextMenuConfigurationProvider* strongSelf = weakSelf;
        if (!strongSelf)
          return;
        UrlLoadingBrowserAgent::FromBrowser(strongSelf.browser)
            ->Load(loadParams);
      }];
      [menuElements addObject:openNewTab];

      if (!isOffTheRecord) {
        // Open in Incognito Tab.
        UIAction* openIncognitoTab;
        openIncognitoTab =
            [actionFactory actionToOpenInNewIncognitoTabWithURL:linkURL
                                                     completion:nil];
        [menuElements addObject:openIncognitoTab];
      }

      if (base::ios::IsMultipleScenesSupported()) {
        // Open in New Window.

        NSUserActivity* newWindowActivity = ActivityToLoadURL(
            WindowActivityContextMenuOrigin, linkURL, referrer, isOffTheRecord);
        UIAction* openNewWindow = [actionFactory
            actionToOpenInNewWindowWithActivity:newWindowActivity];

        [menuElements addObject:openNewWindow];
      }

      if (linkURL.SchemeIsHTTPOrHTTPS()) {
        NSString* innerText = params.text;
        if ([innerText length] > 0) {
          // Add to reading list.
          UIAction* addToReadingList =
              [actionFactory actionToAddToReadingListWithBlock:^{
                ContextMenuConfigurationProvider* strongSelf = weakSelf;
                if (!strongSelf)
                  return;

                ReadingListAddCommand* command =
                    [[ReadingListAddCommand alloc] initWithURL:linkURL
                                                         title:innerText];
                ReadingListBrowserAgent* readingListBrowserAgent =
                    ReadingListBrowserAgent::FromBrowser(self.browser);
                readingListBrowserAgent->AddURLsToReadingList(command.URLs);
              }];
          [menuElements addObject:addToReadingList];
        }
      }
    }

    // Copy Link.
    UIAction* copyLink = [actionFactory actionToCopyURL:linkURL];
    [menuElements addObject:copyLink];
  }

  if (isImage) {
    base::RecordAction(
        base::UserMetricsAction("MobileWebContextMenuImageImpression"));

    __weak UIViewController* weakBaseViewController = self.baseViewController;

    // Save Image.
    UIAction* saveImage = [actionFactory actionSaveImageWithBlock:^{
      if (!weakSelf || !weakBaseViewController)
        return;
      [weakSelf.imageSaver saveImageAtURL:imageURL
                                 referrer:referrer
                                 webState:weakSelf.currentWebState
                       baseViewController:weakBaseViewController];
      base::UmaHistogramEnumeration(
          kSaveToPhotosContextMenuActionsHistogram,
          saveToPhotosAvailable
              ? SaveToPhotosContextMenuActions::kAvailableDidSaveImageLocally
              : SaveToPhotosContextMenuActions::
                    kUnavailableDidSaveImageLocally);
    }];
    [menuElements addObject:saveImage];

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
      [menuElements addObject:saveImageToPhotosAction];
    }

    // Copy Image.
    UIAction* copyImage = [actionFactory actionCopyImageWithBlock:^{
      if (!weakSelf || !weakBaseViewController)
        return;
      [weakSelf.imageCopier copyImageAtURL:imageURL
                                  referrer:referrer
                                  webState:weakSelf.currentWebState
                        baseViewController:weakBaseViewController];
    }];
    [menuElements addObject:copyImage];

    // Open Image.
    UIAction* openImage = [actionFactory actionOpenImageWithURL:imageURL
                                                     completion:nil];
    [menuElements addObject:openImage];

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
    [menuElements addObject:openImageInNewTab];

    // Search the image using Lens if Lens is enabled and available. Otherwise
    // fall back to a standard search by image experience.
    TemplateURLService* service =
        ios::TemplateURLServiceFactory::GetForBrowserState(
            self.browser->GetBrowserState());

    const BOOL useLens =
        lens_availability::CheckAndLogAvailabilityForLensEntryPoint(
            LensEntrypoint::ContextMenu,
            search_engines::SupportsSearchImageWithLens(service));
    if (useLens) {
      UIAction* searchImageWithLensAction =
          [actionFactory actionToSearchImageUsingLensWithBlock:^{
            [weakSelf searchImageWithURL:imageURL
                               usingLens:YES
                                referrer:referrer];
          }];
      [menuElements addObject:searchImageWithLensAction];
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
      [menuElements addObject:searchByImage];
    }
  }

  NSString* menuTitle;

  // Insert any provided menu items. Do after Link and/or Image to allow
  // inserting at beginning or adding to end.
  ElementsToAddToContextMenu* result =
      ios::provider::GetContextMenuElementsToAdd(
          webState, params, self.baseViewController,
          HandlerForProtocol(self.browser->GetCommandDispatcher(),
                             MiniMapCommands),
          HandlerForProtocol(self.browser->GetCommandDispatcher(),
                             UnitConversionCommands));
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

@end
