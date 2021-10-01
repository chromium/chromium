// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/context_menu/context_menu_configuration_provider.h"

#include "base/ios/ios_util.h"
#include "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#include "components/search_engines/template_url_service.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#import "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/policy/policy_util.h"
#include "ios/chrome/browser/search_engines/search_engines_util.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/ui/alert_coordinator/action_sheet_coordinator.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/reading_list_add_command.h"
#import "ios/chrome/browser/ui/commands/search_image_with_lens_command.h"
#import "ios/chrome/browser/ui/context_menu/context_menu_utils.h"
#import "ios/chrome/browser/ui/context_menu/image_preview_view_controller.h"
#import "ios/chrome/browser/ui/context_menu/link_no_preview_view_controller.h"
#import "ios/chrome/browser/ui/image_util/image_copier.h"
#import "ios/chrome/browser/ui/image_util/image_saver.h"
#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_commands.h"
#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_scene_agent.h"
#import "ios/chrome/browser/ui/main/scene_state_browser_agent.h"
#import "ios/chrome/browser/ui/menu/browser_action_factory.h"
#import "ios/chrome/browser/ui/menu/menu_histograms.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/pasteboard_util.h"
#import "ios/chrome/browser/ui/util/url_with_title.h"
#import "ios/chrome/browser/url_loading/image_search_param_generator.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/browser/web/image_fetch/image_fetch_tab_helper.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/public/provider/chrome/browser/lens/lens_api.h"
#include "ios/web/common/features.h"
#import "ios/web/common/url_scheme_util.h"
#import "ios/web/public/ui/context_menu_params.h"
#import "ios/web/public/web_state.h"
#include "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

typedef NS_ENUM(NSInteger, ContextMenuHistogram) {
  // Note: these values must match the ContextMenuOptionIOS enum in enums.xml.
  ACTION_OPEN_IN_NEW_TAB = 0,
  ACTION_OPEN_IN_INCOGNITO_TAB = 1,
  ACTION_COPY_LINK_ADDRESS = 2,
  ACTION_SAVE_IMAGE = 3,
  ACTION_OPEN_IMAGE = 4,
  ACTION_OPEN_IMAGE_IN_NEW_TAB = 5,
  ACTION_COPY_IMAGE = 6,
  ACTION_SEARCH_BY_IMAGE = 7,
  ACTION_OPEN_JAVASCRIPT = 8,
  ACTION_READ_LATER = 9,
  ACTION_OPEN_IN_NEW_WINDOW = 10,
  NUM_ACTIONS = 11,
};

void Record(ContextMenuHistogram action, bool is_image, bool is_link) {
  if (is_image) {
    if (is_link) {
      UMA_HISTOGRAM_ENUMERATION("ContextMenu.SelectedOptionIOS.ImageLink",
                                action, NUM_ACTIONS);
    } else {
      UMA_HISTOGRAM_ENUMERATION("ContextMenu.SelectedOptionIOS.Image", action,
                                NUM_ACTIONS);
    }
  } else {
    UMA_HISTOGRAM_ENUMERATION("ContextMenu.SelectedOptionIOS.Link", action,
                              NUM_ACTIONS);
  }
}

// Maximum length for a context menu title formed from a URL.
const NSUInteger kContextMenuMaxURLTitleLength = 100;
// Character to append to context menut titles that are truncated.
NSString* const kContextMenuEllipsis = @"…";

// Desired width and height of favicon.
const CGFloat kFaviconWidthHeight = 24;

}  // namespace

@interface ContextMenuConfigurationProvider ()

// Helper for saving images.
@property(nonatomic, strong) ImageSaver* imageSaver;
// Helper for copying images.
@property(nonatomic, strong) ImageCopier* imageCopier;

@property(nonatomic, assign) Browser* browser;

// Handles displaying the action sheet for all form factors.
@property(nonatomic, strong)
    ActionSheetCoordinator* legacyContextMenuCoordinator;

@property(nonatomic, assign, readonly) web::WebState* currentWebState;

@end

@implementation ContextMenuConfigurationProvider

- (instancetype)initWithBrowser:(Browser*)browser {
  self = [super init];
  if (self) {
    _browser = browser;
    _imageSaver = [[ImageSaver alloc] initWithBrowser:self.browser];
    _imageCopier = [[ImageCopier alloc] initWithBrowser:self.browser];
  }
  return self;
}

- (UIContextMenuConfiguration*)
    contextMenuConfigurationForWebState:(web::WebState*)webState
                                 params:(const web::ContextMenuParams&)params
                     baseViewController:(UIViewController*)baseViewController {
  // Prevent context menu from displaying for a tab which is no longer the
  // current one.
  if (webState != self.currentWebState) {
    return nil;
  }

  // Copy the link_url and src_url to allow the block to safely
  // capture them (capturing references would lead to UAF).
  const GURL link = params.link_url;
  const bool isLink = link.is_valid();
  const GURL imageUrl = params.src_url;
  const bool isImage = imageUrl.is_valid();

  BOOL isOffTheRecord = self.browser->GetBrowserState()->IsOffTheRecord();
  __weak UIViewController* weakBaseViewController = baseViewController;

  // Presents a custom menu only if there is a valid url
  // or a valid image.
  if (!isLink && !isImage)
    return nil;

  DCHECK(self.browser->GetBrowserState());

  __weak __typeof(self) weakSelf = self;

  const GURL& lastCommittedURL = webState->GetLastCommittedURL();
  web::Referrer referrer(lastCommittedURL, web::ReferrerPolicyDefault);

  NSMutableArray<UIMenuElement*>* menuElements = [[NSMutableArray alloc] init];
  MenuScenario menuScenario = isImage && isLink
                                  ? MenuScenario::kContextMenuImageLink
                                  : isImage ? MenuScenario::kContextMenuImage
                                            : MenuScenario::kContextMenuLink;

  BrowserActionFactory* actionFactory =
      [[BrowserActionFactory alloc] initWithBrowser:self.browser
                                           scenario:menuScenario];

  if (isLink) {
    base::RecordAction(
        base::UserMetricsAction("MobileWebContextMenuLinkImpression"));
    if (web::UrlHasWebScheme(link)) {
      // Open in New Tab.
      UrlLoadParams loadParams = UrlLoadParams::InNewTab(link);
      loadParams.SetInBackground(YES);
      loadParams.in_incognito = isOffTheRecord;
      loadParams.append_to = kCurrentTab;
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
        UIAction* openIncognitoTab =
            [actionFactory actionToOpenInNewIncognitoTabWithURL:link
                                                     completion:nil];
        [menuElements addObject:openIncognitoTab];
      }

      if (base::ios::IsMultipleScenesSupported()) {
        // Open in New Window.

        NSUserActivity* newWindowActivity = ActivityToLoadURL(
            WindowActivityContextMenuOrigin, link, referrer, isOffTheRecord);
        UIAction* openNewWindow = [actionFactory
            actionToOpenInNewWindowWithActivity:newWindowActivity];

        [menuElements addObject:openNewWindow];
      }

      if (link.SchemeIsHTTPOrHTTPS()) {
        NSString* innerText = params.link_text;
        if ([innerText length] > 0) {
          // Add to reading list.
          UIAction* addToReadingList =
              [actionFactory actionToAddToReadingListWithBlock:^{
                ContextMenuConfigurationProvider* strongSelf = weakSelf;
                if (!strongSelf)
                  return;

                id<BrowserCommands> handler = static_cast<id<BrowserCommands>>(
                    strongSelf.browser->GetCommandDispatcher());
                [handler addToReadingList:[[ReadingListAddCommand alloc]
                                              initWithURL:link
                                                    title:innerText]];
              }];
          [menuElements addObject:addToReadingList];
        }
      }
    }

    // Copy Link.
    UIAction* copyLink = [actionFactory actionToCopyURL:link];
    [menuElements addObject:copyLink];
  }

  if (isImage) {
    base::RecordAction(
        base::UserMetricsAction("MobileWebContextMenuImageImpression"));
    // Save Image.
    UIAction* saveImage = [actionFactory actionSaveImageWithBlock:^{
      if (!weakSelf || !weakBaseViewController)
        return;
      [weakSelf.imageSaver saveImageAtURL:imageUrl
                                 referrer:referrer
                                 webState:weakSelf.currentWebState
                       baseViewController:weakBaseViewController];
    }];
    [menuElements addObject:saveImage];

    // Copy Image.
    UIAction* copyImage = [actionFactory actionCopyImageWithBlock:^{
      if (!weakSelf || !weakBaseViewController)
        return;
      [weakSelf.imageCopier copyImageAtURL:imageUrl
                                  referrer:referrer
                                  webState:weakSelf.currentWebState
                        baseViewController:weakBaseViewController];
    }];
    [menuElements addObject:copyImage];

    // Open Image.
    UIAction* openImage = [actionFactory actionOpenImageWithURL:imageUrl
                                                     completion:nil];
    [menuElements addObject:openImage];

    // Open Image in new tab.
    UrlLoadParams loadParams = UrlLoadParams::InNewTab(imageUrl);
    loadParams.SetInBackground(YES);
    loadParams.web_params.referrer = referrer;
    loadParams.in_incognito = isOffTheRecord;
    loadParams.append_to = kCurrentTab;
    loadParams.origin_point = [params.view convertPoint:params.location
                                                 toView:nil];
    UIAction* openImageInNewTab =
        [actionFactory actionOpenImageInNewTabWithUrlLoadParams:loadParams
                                                     completion:nil];
    [menuElements addObject:openImageInNewTab];

    // Search by image or Search image with Lens.
    TemplateURLService* service =
        ios::TemplateURLServiceFactory::GetForBrowserState(
            self.browser->GetBrowserState());
    __weak ContextMenuConfigurationProvider* weakSelf = self;
    if (ios::provider::IsLensSupported() &&
        base::FeatureList::IsEnabled(kUseLensToSearchForImage) &&
        search_engines::SupportsSearchImageWithLens(service)) {
      UIAction* searchImageWithLensAction =
          [actionFactory actionToSearchImageUsingLensWithBlock:^{
            [weakSelf searchImageWithURL:imageUrl
                               usingLens:YES
                                referrer:referrer];
          }];
      [menuElements addObject:searchImageWithLensAction];
    } else if (search_engines::SupportsSearchByImage(service)) {
      const TemplateURL* defaultURL = service->GetDefaultSearchProvider();
      NSString* title =
          IsContextMenuActionsRefreshEnabled()
              ? l10n_util::GetNSString(IDS_IOS_CONTEXT_MENU_SEARCHFORIMAGE)
              : l10n_util::GetNSStringF(IDS_IOS_CONTEXT_MENU_SEARCHWEBFORIMAGE,
                                        defaultURL->short_name());
      UIAction* searchByImage = [actionFactory
          actionSearchImageWithTitle:title
                               Block:^{
                                 [weakSelf searchImageWithURL:imageUrl
                                                    usingLens:NO
                                                     referrer:referrer];
                               }];
      [menuElements addObject:searchByImage];
    }
  }

  NSString* menuTitle = nil;
  if (!base::FeatureList::IsEnabled(
          web::features::kWebViewNativeContextMenuPhase2)) {
    menuTitle = GetContextMenuTitle(params);

    // Truncate context meny titles that originate from URLs, leaving text
    // titles untruncated.
    if (!IsImageTitle(params) &&
        menuTitle.length > kContextMenuMaxURLTitleLength + 1) {
      menuTitle = [[menuTitle substringToIndex:kContextMenuMaxURLTitleLength]
          stringByAppendingString:kContextMenuEllipsis];
    }
  } else if (!isLink) {
    menuTitle = GetContextMenuTitle(params);
  }

  UIContextMenuActionProvider actionProvider =
      ^(NSArray<UIMenuElement*>* suggestedActions) {
        RecordMenuShown(menuScenario);
        return [UIMenu menuWithTitle:menuTitle children:menuElements];
      };

  UIContextMenuContentPreviewProvider previewProvider = ^UIViewController* {
    if (!base::FeatureList::IsEnabled(
            web::features::kWebViewNativeContextMenuPhase2)) {
      return nil;
    }
    if (isLink) {
      NSString* title = GetContextMenuTitle(params);
      NSString* subtitle = GetContextMenuSubtitle(params);
      LinkNoPreviewViewController* previewViewController =
          [[LinkNoPreviewViewController alloc] initWithTitle:title
                                                    subtitle:subtitle];

      __weak LinkNoPreviewViewController* weakPreview = previewViewController;
      FaviconLoader* faviconLoader =
          IOSChromeFaviconLoaderFactory::GetForBrowserState(
              self.browser->GetBrowserState());
      faviconLoader->FaviconForPageUrl(
          params.link_url, kFaviconWidthHeight, kFaviconWidthHeight,
          /*fallback_to_google_server=*/false,
          ^(FaviconAttributes* attributes) {
            [weakPreview configureFaviconWithAttributes:attributes];
          });
      return previewViewController;
    }
    DCHECK(isImage);
    ImagePreviewViewController* preview =
        [[ImagePreviewViewController alloc] init];
    __weak ImagePreviewViewController* weakPreview = preview;

    ImageFetchTabHelper* imageFetcher =
        ImageFetchTabHelper::FromWebState(self.currentWebState);
    DCHECK(imageFetcher);
    imageFetcher->GetImageData(imageUrl, referrer, ^(NSData* data) {
      [weakPreview updateImageData:data];
    });

    return preview;
  };
  return
      [UIContextMenuConfiguration configurationWithIdentifier:nil
                                              previewProvider:previewProvider
                                               actionProvider:actionProvider];
}

- (void)showLegacyContextMenuForWebState:(web::WebState*)webState
                                  params:(const web::ContextMenuParams&)params
                      baseViewController:(UIViewController*)baseViewController {
  DCHECK(!web::features::UseWebViewNativeContextMenuWeb() &&
         !web::features::UseWebViewNativeContextMenuSystem());
  // Prevent context menu from displaying for a tab which is no longer the
  // current one.
  if (webState != self.currentWebState) {
    return;
  }

  // No custom context menu if no valid url is available in |params|.
  if (!params.link_url.is_valid() && !params.src_url.is_valid()) {
    return;
  }

  DCHECK(self.browser->GetBrowserState());

  BOOL isOffTheRecord = self.browser->GetBrowserState()->IsOffTheRecord();
  __weak UIViewController* weakBaseViewController = baseViewController;

  // Truncate context meny titles that originate from URLs, leaving text titles
  // untruncated.
  NSString* menuTitle = GetContextMenuTitle(params);
  if (!IsImageTitle(params) &&
      menuTitle.length > kContextMenuMaxURLTitleLength + 1) {
    menuTitle = [[menuTitle substringToIndex:kContextMenuMaxURLTitleLength]
        stringByAppendingString:kContextMenuEllipsis];
  }

  self.legacyContextMenuCoordinator = [[ActionSheetCoordinator alloc]
      initWithBaseViewController:baseViewController
                         browser:self.browser
                           title:menuTitle
                         message:nil
                            rect:CGRectMake(params.location.x,
                                            params.location.y, 1.0, 1.0)
                            view:params.view];

  NSString* title = nil;
  ProceduralBlock action = nil;

  __weak __typeof(self) weakSelf = self;
  GURL link = params.link_url;
  bool isLink = link.is_valid();
  GURL imageUrl = params.src_url;
  bool isImage = imageUrl.is_valid();
  const GURL& lastCommittedURL = webState->GetLastCommittedURL();
  CGPoint originPoint = [params.view convertPoint:params.location toView:nil];

  if (isLink) {
    base::RecordAction(
        base::UserMetricsAction("MobileWebContextMenuLinkImpression"));
    if (web::UrlHasWebScheme(link)) {
      web::Referrer referrer(lastCommittedURL, params.referrer_policy);

      // Open in New Tab.
      title = l10n_util::GetNSStringWithFixup(
          IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB);
      action = ^{
        base::RecordAction(
            base::UserMetricsAction("MobileWebContextMenuOpenInNewTab"));
        Record(ACTION_OPEN_IN_NEW_TAB, isImage, isLink);
        // The "New Tab" item in the context menu opens a new tab in the current
        // browser state. |isOffTheRecord| indicates whether or not the current
        // browser state is incognito.
        ContextMenuConfigurationProvider* strongSelf = weakSelf;
        if (!strongSelf)
          return;

        UrlLoadParams params = UrlLoadParams::InNewTab(link);
        params.SetInBackground(YES);
        params.web_params.referrer = referrer;
        params.in_incognito = isOffTheRecord;
        params.append_to = kCurrentTab;
        params.origin_point = originPoint;
        UrlLoadingBrowserAgent::FromBrowser(self.browser)->Load(params);
      };
      [self.legacyContextMenuCoordinator
          addItemWithTitle:title
                    action:action
                     style:UIAlertActionStyleDefault];

      if (base::ios::IsMultipleScenesSupported()) {
        // Open in New Window.
        title = l10n_util::GetNSStringWithFixup(
            IDS_IOS_CONTENT_CONTEXT_OPENINNEWWINDOW);
        action = ^{
          base::RecordAction(
              base::UserMetricsAction("MobileWebContextMenuOpenInNewWindow"));
          Record(ACTION_OPEN_IN_NEW_WINDOW, isImage, isLink);
          // The "Open In New Window" item in the context menu opens a new tab
          // in a new window. This will be (according to |isOffTheRecord|)
          // incognito if the originating browser is incognito.
          ContextMenuConfigurationProvider* strongSelf = weakSelf;
          if (!strongSelf)
            return;

          NSUserActivity* loadURLActivity = ActivityToLoadURL(
              WindowActivityContextMenuOrigin, link, referrer, isOffTheRecord);
          id<ApplicationCommands> handler = HandlerForProtocol(
              strongSelf.browser->GetCommandDispatcher(), ApplicationCommands);

          [handler openNewWindowWithActivity:loadURLActivity];
        };
        [self.legacyContextMenuCoordinator
            addItemWithTitle:title
                      action:action
                       style:UIAlertActionStyleDefault];
      }
      if (!isOffTheRecord) {
        // Open in Incognito Tab.
        title = l10n_util::GetNSStringWithFixup(
            IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWINCOGNITOTAB);
        action = ^{
          base::RecordAction(base::UserMetricsAction(
              "MobileWebContextMenuOpenInIncognitoTab"));
          ContextMenuConfigurationProvider* strongSelf = weakSelf;
          if (!strongSelf)
            return;

          Record(ACTION_OPEN_IN_INCOGNITO_TAB, isImage, isLink);

          UrlLoadParams params = UrlLoadParams::InNewTab(link);
          params.web_params.referrer = referrer;
          params.in_incognito = YES;
          params.append_to = kCurrentTab;
          UrlLoadingBrowserAgent::FromBrowser(self.browser)->Load(params);
        };

        IncognitoReauthSceneAgent* reauthAgent = [IncognitoReauthSceneAgent
            agentFromScene:SceneStateBrowserAgent::FromBrowser(self.browser)
                               ->GetSceneState()];
        // Wrap the action inside of an auth check block.
        ProceduralBlock wrappedAction = action;
        action = ^{
          if (reauthAgent.authenticationRequired) {
            [reauthAgent authenticateIncognitoContentWithCompletionBlock:^(
                             BOOL success) {
              if (success) {
                wrappedAction();
              }
            }];
          } else {
            wrappedAction();
          }
        };

        [self.legacyContextMenuCoordinator
            addItemWithTitle:title
                      action:action
                       style:UIAlertActionStyleDefault
                     enabled:!IsIncognitoModeDisabled(
                                 self.browser->GetBrowserState()->GetPrefs())];
      }
    }
    if (link.SchemeIsHTTPOrHTTPS()) {
      NSString* innerText = params.link_text;
      if ([innerText length] > 0) {
        // Add to reading list.
        title = l10n_util::GetNSStringWithFixup(
            IDS_IOS_CONTENT_CONTEXT_ADDTOREADINGLIST);
        action = ^{
          ContextMenuConfigurationProvider* strongSelf = weakSelf;
          if (!strongSelf)
            return;

          base::RecordAction(
              base::UserMetricsAction("MobileWebContextMenuReadLater"));
          Record(ACTION_READ_LATER, isImage, isLink);
          id<BrowserCommands> handler = static_cast<id<BrowserCommands>>(
              strongSelf.browser->GetCommandDispatcher());
          [handler addToReadingList:[[ReadingListAddCommand alloc]
                                        initWithURL:link
                                              title:innerText]];
        };
        [self.legacyContextMenuCoordinator
            addItemWithTitle:title
                      action:action
                       style:UIAlertActionStyleDefault];
      }
    }
    // Copy Link.
    title = l10n_util::GetNSStringWithFixup(IDS_IOS_CONTENT_CONTEXT_COPY);
    action = ^{
      base::RecordAction(
          base::UserMetricsAction("MobileWebContextMenuCopyLink"));
      Record(ACTION_COPY_LINK_ADDRESS, isImage, isLink);
      StoreURLInPasteboard(link);
    };
    [self.legacyContextMenuCoordinator
        addItemWithTitle:title
                  action:action
                   style:UIAlertActionStyleDefault];
  }
  if (isImage) {
    base::RecordAction(
        base::UserMetricsAction("MobileWebContextMenuImageImpression"));
    web::Referrer referrer(lastCommittedURL, params.referrer_policy);
    // Save Image.
    title = l10n_util::GetNSStringWithFixup(IDS_IOS_CONTENT_CONTEXT_SAVEIMAGE);
    action = ^{
      base::RecordAction(
          base::UserMetricsAction("MobileWebContextMenuSaveImage"));
      Record(ACTION_SAVE_IMAGE, isImage, isLink);
      if (!weakSelf || !weakBaseViewController)
        return;

      [weakSelf.imageSaver saveImageAtURL:imageUrl
                                 referrer:referrer
                                 webState:weakSelf.currentWebState
                       baseViewController:weakBaseViewController];
    };
    [self.legacyContextMenuCoordinator
        addItemWithTitle:title
                  action:action
                   style:UIAlertActionStyleDefault];
    // Copy Image.
    title = l10n_util::GetNSStringWithFixup(IDS_IOS_CONTENT_CONTEXT_COPYIMAGE);
    action = ^{
      base::RecordAction(
          base::UserMetricsAction("MobileWebContextMenuCopyImage"));
      Record(ACTION_COPY_IMAGE, isImage, isLink);
      DCHECK(imageUrl.is_valid());

      if (!weakSelf || !weakBaseViewController)
        return;

      [weakSelf.imageCopier copyImageAtURL:imageUrl
                                  referrer:referrer
                                  webState:weakSelf.currentWebState
                        baseViewController:weakBaseViewController];
    };
    [self.legacyContextMenuCoordinator
        addItemWithTitle:title
                  action:action
                   style:UIAlertActionStyleDefault];
    // Open Image.
    title = l10n_util::GetNSStringWithFixup(IDS_IOS_CONTENT_CONTEXT_OPENIMAGE);
    action = ^{
      base::RecordAction(
          base::UserMetricsAction("MobileWebContextMenuOpenImage"));
      ContextMenuConfigurationProvider* strongSelf = weakSelf;
      if (!strongSelf)
        return;

      Record(ACTION_OPEN_IMAGE, isImage, isLink);
      UrlLoadingBrowserAgent::FromBrowser(self.browser)
          ->Load(UrlLoadParams::InCurrentTab(imageUrl));
    };
    [self.legacyContextMenuCoordinator
        addItemWithTitle:title
                  action:action
                   style:UIAlertActionStyleDefault];
    // Open Image In New Tab.
    title = l10n_util::GetNSStringWithFixup(
        IDS_IOS_CONTENT_CONTEXT_OPENIMAGENEWTAB);
    action = ^{
      base::RecordAction(
          base::UserMetricsAction("MobileWebContextMenuOpenImageInNewTab"));
      Record(ACTION_OPEN_IMAGE_IN_NEW_TAB, isImage, isLink);
      ContextMenuConfigurationProvider* strongSelf = weakSelf;
      if (!strongSelf)
        return;

      UrlLoadParams params = UrlLoadParams::InNewTab(imageUrl);
      params.SetInBackground(YES);
      params.web_params.referrer = referrer;
      params.in_incognito = isOffTheRecord;
      params.append_to = kCurrentTab;
      params.origin_point = originPoint;
      UrlLoadingBrowserAgent::FromBrowser(self.browser)->Load(params);
    };
    [self.legacyContextMenuCoordinator
        addItemWithTitle:title
                  action:action
                   style:UIAlertActionStyleDefault];

    TemplateURLService* service =
        ios::TemplateURLServiceFactory::GetForBrowserState(
            self.browser->GetBrowserState());
    if (search_engines::SupportsSearchByImage(service)) {
      const TemplateURL* defaultURL = service->GetDefaultSearchProvider();
      title = l10n_util::GetNSStringF(IDS_IOS_CONTEXT_MENU_SEARCHWEBFORIMAGE,
                                      defaultURL->short_name());
      action = ^{
        base::RecordAction(
            base::UserMetricsAction("MobileWebContextMenuSearchByImage"));
        Record(ACTION_SEARCH_BY_IMAGE, isImage, isLink);
        ImageFetchTabHelper* imageFetcher =
            ImageFetchTabHelper::FromWebState(self.currentWebState);
        DCHECK(imageFetcher);
        imageFetcher->GetImageData(imageUrl, referrer, ^(NSData* data) {
          [weakSelf searchByImageData:data imageURL:imageUrl];
        });
      };
      [self.legacyContextMenuCoordinator
          addItemWithTitle:title
                    action:action
                     style:UIAlertActionStyleDefault];
    }
  }

  [self.legacyContextMenuCoordinator start];
}

- (void)dismissLegacyContextMenu {
  [self.legacyContextMenuCoordinator stop];
}

#pragma mark - Properties

- (web::WebState*)currentWebState {
  return self.browser ? self.browser->GetWebStateList()->GetActiveWebState()
                      : nullptr;
}

#pragma mark - Private

// Searches an image with the given |imageURL| and |referrer|, optionally using
// Lens.
- (void)searchImageWithURL:(GURL)imageUrl
                 usingLens:(BOOL)usingLens
                  referrer:(web::Referrer)referrer {
  ImageFetchTabHelper* imageFetcher =
      ImageFetchTabHelper::FromWebState(self.currentWebState);
  DCHECK(imageFetcher);
  __weak ContextMenuConfigurationProvider* weakSelf = self;
  imageFetcher->GetImageData(imageUrl, referrer, ^(NSData* data) {
    if (usingLens) {
      [weakSelf searchImageUsingLensWithData:data];
    } else {
      [weakSelf searchByImageData:data imageURL:imageUrl];
    }
  });
}

// Starts a reverse image search based on |imageData| and |imageURL| in a new
// tab.
- (void)searchByImageData:(NSData*)imageData imageURL:(const GURL&)URL {
  web::NavigationManager::WebLoadParams webParams =
      ImageSearchParamGenerator::LoadParamsForImageData(
          imageData, URL,
          ios::TemplateURLServiceFactory::GetForBrowserState(
              self.browser->GetBrowserState()));

  UrlLoadParams params = UrlLoadParams::InNewTab(webParams);
  params.in_incognito = self.browser->GetBrowserState()->IsOffTheRecord();
  UrlLoadingBrowserAgent::FromBrowser(self.browser)->Load(params);
}

// Searches an image with Lens using the given |imageData|.
- (void)searchImageUsingLensWithData:(NSData*)imageData {
  id<BrowserCommands> handler =
      static_cast<id<BrowserCommands>>(_browser->GetCommandDispatcher());
  UIImage* image = [UIImage imageWithData:imageData];
  SearchImageWithLensCommand* command =
      [[SearchImageWithLensCommand alloc] initWithImage:image];
  [handler searchImageWithLens:command];
}

@end
