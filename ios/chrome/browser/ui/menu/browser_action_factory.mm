// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/menu/browser_action_factory.h"

#import "base/strings/sys_string_conversions.h"
#import "components/open_from_clipboard/clipboard_recent_content.h"
#import "components/prefs/pref_service.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/policy/policy_util.h"
#import "ios/chrome/browser/prefs/pref_names.h"
#import "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/load_query_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/commands/qr_scanner_commands.h"
#import "ios/chrome/browser/ui/icons/symbols.h"
#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_scene_agent.h"
#import "ios/chrome/browser/ui/main/scene_state_browser_agent.h"
#import "ios/chrome/browser/ui/menu/action_factory+protected.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/pasteboard_util.h"
#import "ios/chrome/browser/url_loading/image_search_param_generator.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface BrowserActionFactory ()

// Current browser instance.
@property(nonatomic, assign) Browser* browser;

@end

@implementation BrowserActionFactory

- (instancetype)initWithBrowser:(Browser*)browser
                       scenario:(MenuScenarioHistogram)scenario {
  DCHECK(browser);
  if (self = [super initWithScenario:scenario]) {
    _browser = browser;
  }
  return self;
}

- (UIAction*)actionToOpenInNewTabWithURL:(const GURL)URL
                              completion:(ProceduralBlock)completion {
  UrlLoadParams params = UrlLoadParams::InNewTab(URL);
  UrlLoadingBrowserAgent* loadingAgent =
      UrlLoadingBrowserAgent::FromBrowser(self.browser);
  return [self actionToOpenInNewTabWithBlock:^{
    loadingAgent->Load(params);
    if (completion) {
      completion();
    }
  }];
}

- (UIAction*)actionToOpenInNewIncognitoTabWithURL:(const GURL)URL
                                       completion:(ProceduralBlock)completion {
  if (!_browser)
    return nil;

  UrlLoadParams params = UrlLoadParams::InNewTab(URL);
  params.in_incognito = YES;
  UrlLoadingBrowserAgent* loadingAgent =
      UrlLoadingBrowserAgent::FromBrowser(self.browser);
  return [self actionToOpenInNewIncognitoTabWithBlock:^{
    loadingAgent->Load(params);
    if (completion) {
      completion();
    }
  }];
}

- (UIAction*)actionToOpenInNewIncognitoTabWithBlock:(ProceduralBlock)block {
  // Wrap the block with the incognito auth check, if necessary.
  IncognitoReauthSceneAgent* reauthAgent = [IncognitoReauthSceneAgent
      agentFromScene:SceneStateBrowserAgent::FromBrowser(self.browser)
                         ->GetSceneState()];
  if (reauthAgent.authenticationRequired) {
    block = ^{
      [reauthAgent
          authenticateIncognitoContentWithCompletionBlock:^(BOOL success) {
            if (success && block != nullptr) {
              block();
            }
          }];
    };
  }

  UIImage* image = UseSymbols() ? CustomSymbolWithPointSize(
                                      kIncognitoSymbol, kSymbolActionPointSize)
                                : [UIImage imageNamed:@"open_in_incognito"];
  ProceduralBlock completionBlock =
      [self recordMobileWebContextMenuOpenTabActionWithBlock:block];

  return [self actionWithTitle:l10n_util::GetNSString(
                                   IDS_IOS_OPEN_IN_INCOGNITO_ACTION_TITLE)
                         image:image
                          type:MenuActionType::OpenInNewIncognitoTab
                         block:completionBlock];
}

- (UIAction*)actionToOpenInNewWindowWithURL:(const GURL)URL
                             activityOrigin:
                                 (WindowActivityOrigin)activityOrigin {
  id<ApplicationCommands> windowOpener = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);

  UIImage* image = UseSymbols()
                       ? DefaultSymbolWithPointSize(kNewWindowActionSymbol,
                                                    kSymbolActionPointSize)
                       : [UIImage imageNamed:@"open_new_window"];
  NSUserActivity* activity = ActivityToLoadURL(activityOrigin, URL);
  return [self actionWithTitle:l10n_util::GetNSString(
                                   IDS_IOS_CONTENT_CONTEXT_OPENINNEWWINDOW)
                         image:image
                          type:MenuActionType::OpenInNewWindow
                         block:^{
                           [windowOpener openNewWindowWithActivity:activity];
                         }];
}

- (UIAction*)actionToOpenInNewWindowWithActivity:(NSUserActivity*)activity {
  id<ApplicationCommands> windowOpener = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);

  UIImage* image = UseSymbols()
                       ? DefaultSymbolWithPointSize(kNewWindowActionSymbol,
                                                    kSymbolActionPointSize)
                       : [UIImage imageNamed:@"open_new_window"];
  return [self actionWithTitle:l10n_util::GetNSString(
                                   IDS_IOS_CONTENT_CONTEXT_OPENINNEWWINDOW)
                         image:image
                          type:MenuActionType::OpenInNewWindow
                         block:^{
                           [windowOpener openNewWindowWithActivity:activity];
                         }];
}

- (UIAction*)actionOpenImageWithURL:(const GURL)URL
                         completion:(ProceduralBlock)completion {
  UrlLoadingBrowserAgent* loadingAgent =
      UrlLoadingBrowserAgent::FromBrowser(self.browser);
  UIImage* image = UseSymbols()
                       ? DefaultSymbolWithPointSize(kOpenImageActionSymbol,
                                                    kSymbolActionPointSize)
                       : [UIImage imageNamed:@"open"];
  UIAction* action = [self
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_OPENIMAGE)
                image:image
                 type:MenuActionType::OpenImageInCurrentTab
                block:^{
                  loadingAgent->Load(UrlLoadParams::InCurrentTab(URL));
                  if (completion) {
                    completion();
                  }
                }];
  return action;
}

- (UIAction*)actionOpenImageInNewTabWithUrlLoadParams:(UrlLoadParams)params
                                           completion:
                                               (ProceduralBlock)completion {
  UrlLoadingBrowserAgent* loadingAgent =
      UrlLoadingBrowserAgent::FromBrowser(self.browser);
  UIImage* image = UseSymbols()
                       ? CustomSymbolWithPointSize(kPhotoBadgePlusSymbol,
                                                   kSymbolActionPointSize)
                       : [UIImage imageNamed:@"open_image_in_new_tab"];
  UIAction* action =
      [self actionWithTitle:l10n_util::GetNSString(
                                IDS_IOS_CONTENT_CONTEXT_OPENIMAGENEWTAB)
                      image:image
                       type:MenuActionType::OpenImageInNewTab
                      block:^{
                        loadingAgent->Load(params);
                        if (completion) {
                          completion();
                        }
                      }];
  return action;
}

- (UIAction*)actionToOpenNewTab {
  id<ApplicationCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  UIAction* action =
      [self actionWithTitle:l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_NEW_TAB)
                      image:DefaultSymbolWithPointSize(kNewTabActionSymbol,
                                                       kSymbolActionPointSize)
                       type:MenuActionType::OpenNewTab
                      block:^{
                        [handler openURLInNewTab:[OpenNewTabCommand
                                                     commandWithIncognito:NO]];
                      }];
  if (IsIncognitoModeForced(self.browser->GetBrowserState()->GetPrefs())) {
    action.attributes = UIMenuElementAttributesDisabled;
  }
  return action;
}

- (UIAction*)actionToOpenNewIncognitoTab {
  id<ApplicationCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  UIAction* action =
      [self actionWithTitle:l10n_util::GetNSString(
                                IDS_IOS_TOOLS_MENU_NEW_INCOGNITO_TAB)
                      image:CustomSymbolWithPointSize(kIncognitoSymbol,
                                                      kSymbolActionPointSize)
                       type:MenuActionType::OpenNewIncognitoTab
                      block:^{
                        [handler openURLInNewTab:[OpenNewTabCommand
                                                     commandWithIncognito:YES]];
                      }];
  if (IsIncognitoModeDisabled(self.browser->GetBrowserState()->GetPrefs())) {
    action.attributes = UIMenuElementAttributesDisabled;
  }
  return action;
}

- (UIAction*)actionToCloseCurrentTab {
  __weak id<BrowserCoordinatorCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), BrowserCoordinatorCommands);
  UIAction* action =
      [self actionWithTitle:l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_CLOSE_TAB)
                      image:DefaultSymbolWithPointSize(kXMarkSymbol,
                                                       kSymbolActionPointSize)
                       type:MenuActionType::CloseCurrentTabs
                      block:^{
                        [handler closeCurrentTab];
                      }];
  action.attributes = UIMenuElementAttributesDestructive;
  return action;
}

- (UIAction*)actionToShowQRScanner {
  id<QRScannerCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), QRScannerCommands);
  return [self
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_QR_SCANNER)
                image:DefaultSymbolWithPointSize(kQRCodeFinderActionSymbol,
                                                 kSymbolActionPointSize)
                 type:MenuActionType::ShowQRScanner
                block:^{
                  [handler showQRScanner];
                }];
}

- (UIAction*)actionToStartVoiceSearch {
  id<ApplicationCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  return [self
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_VOICE_SEARCH)
                image:DefaultSymbolWithPointSize(kMicrophoneSymbol,
                                                 kSymbolActionPointSize)
                 type:MenuActionType::StartVoiceSearch
                block:^{
                  [handler startVoiceSearch];
                }];
}

- (UIAction*)actionToStartNewSearch {
  id<ApplicationCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  UIAction* action = [self
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_NEW_SEARCH)
                image:DefaultSymbolWithPointSize(kSearchSymbol,
                                                 kSymbolActionPointSize)
                 type:MenuActionType::StartNewSearch
                block:^{
                  OpenNewTabCommand* command =
                      [OpenNewTabCommand commandWithIncognito:NO];
                  command.shouldFocusOmnibox = YES;
                  [handler openURLInNewTab:command];
                }];

  if (IsIncognitoModeForced(self.browser->GetBrowserState()->GetPrefs())) {
    action.attributes = UIMenuElementAttributesDisabled;
  }

  return action;
}

- (UIAction*)actionToStartNewIncognitoSearch {
  id<ApplicationCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  UIAction* action =
      [self actionWithTitle:l10n_util::GetNSString(
                                IDS_IOS_TOOLS_MENU_NEW_INCOGNITO_SEARCH)
                      image:CustomSymbolWithPointSize(kIncognitoSymbol,
                                                      kSymbolActionPointSize)
                       type:MenuActionType::StartNewIcognitoSearch
                      block:^{
                        OpenNewTabCommand* command =
                            [OpenNewTabCommand commandWithIncognito:YES];
                        command.shouldFocusOmnibox = YES;
                        [handler openURLInNewTab:command];
                      }];

  if (IsIncognitoModeDisabled(self.browser->GetBrowserState()->GetPrefs())) {
    action.attributes = UIMenuElementAttributesDisabled;
  }

  return action;
}

- (UIAction*)actionToSearchCopiedImage {
  __weak __typeof(self) weakSelf = self;

  void (^clipboardAction)(absl::optional<gfx::Image>) =
      ^(absl::optional<gfx::Image> optionalImage) {
        if (!optionalImage || !weakSelf) {
          return;
        }
        __typeof(weakSelf) strongSelf = weakSelf;

        TemplateURLService* templateURLService =
            ios::TemplateURLServiceFactory::GetForBrowserState(
                strongSelf.browser->GetBrowserState());

        UIImage* image = [optionalImage.value().ToUIImage() copy];

        web::NavigationManager::WebLoadParams webParams =
            ImageSearchParamGenerator::LoadParamsForImage(image,
                                                          templateURLService);
        UrlLoadParams params = UrlLoadParams::InCurrentTab(webParams);

        UrlLoadingBrowserAgent::FromBrowser(strongSelf.browser)->Load(params);
      };

  return [self actionWithTitle:l10n_util::GetNSString(
                                   IDS_IOS_TOOLS_MENU_SEARCH_COPIED_IMAGE)
                         image:DefaultSymbolWithPointSize(
                                   kClipboardActionSymbol,
                                   kSymbolActionPointSize)
                          type:MenuActionType::SearchCopiedImage
                         block:^{
                           ClipboardRecentContent::GetInstance()
                               ->GetRecentImageFromClipboard(
                                   base::BindOnce(clipboardAction));
                         }];
}

- (UIAction*)actionToSearchCopiedURL {
  id<LoadQueryCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), LoadQueryCommands);

  void (^clipboardAction)(absl::optional<GURL>) =
      ^(absl::optional<GURL> optionalURL) {
        if (!optionalURL) {
          return;
        }
        NSString* URL = base::SysUTF8ToNSString(optionalURL.value().spec());
        dispatch_async(dispatch_get_main_queue(), ^{
          [handler loadQuery:URL immediately:YES];
        });
      };

  return [self actionWithTitle:l10n_util::GetNSString(
                                   IDS_IOS_TOOLS_MENU_VISIT_COPIED_LINK)
                         image:DefaultSymbolWithPointSize(
                                   kClipboardActionSymbol,
                                   kSymbolActionPointSize)
                          type:MenuActionType::VisitCopiedLink
                         block:^{
                           ClipboardRecentContent::GetInstance()
                               ->GetRecentURLFromClipboard(
                                   base::BindOnce(clipboardAction));
                         }];
}

- (UIAction*)actionToSearchCopiedText {
  id<LoadQueryCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), LoadQueryCommands);

  void (^clipboardAction)(absl::optional<std::u16string>) =
      ^(absl::optional<std::u16string> optionalText) {
        if (!optionalText) {
          return;
        }
        NSString* query = base::SysUTF16ToNSString(optionalText.value());
        dispatch_async(dispatch_get_main_queue(), ^{
          [handler loadQuery:query immediately:YES];
        });
      };

  return [self actionWithTitle:l10n_util::GetNSString(
                                   IDS_IOS_TOOLS_MENU_SEARCH_COPIED_TEXT)
                         image:DefaultSymbolWithPointSize(
                                   kClipboardActionSymbol,
                                   kSymbolActionPointSize)
                          type:MenuActionType::SearchCopiedText
                         block:^{
                           ClipboardRecentContent::GetInstance()
                               ->GetRecentTextFromClipboard(
                                   base::BindOnce(clipboardAction));
                         }];
}

@end
