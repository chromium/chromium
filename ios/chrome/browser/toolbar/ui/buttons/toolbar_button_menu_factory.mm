// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button_menu_factory.h"

#import <optional>
#import <set>

#import "base/check.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/open_from_clipboard/clipboard_recent_content.h"
#import "components/search/search.h"
#import "ios/chrome/browser/lens/ui_bundled/lens_availability.h"
#import "ios/chrome/browser/menu/ui_bundled/browser_action_factory.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_util.h"
#import "ios/chrome/browser/search_engines/model/search_engines_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/tab_grid_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button_menu_factory_delegate.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/lens/lens_api.h"
#import "ios/web/public/favicon/favicon_status.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/gfx/image/image.h"

@implementation ToolbarButtonMenuFactory {
  raw_ptr<TemplateURLService> _templateURLService;
  raw_ptr<WebStateList> _webStateList;
  BrowserActionFactory* _actionFactory;
  // The current state of the tab grid. Owned by this factory's `_delegate`.
  __weak TabGridState* _tabGridState;
  BOOL _incognito;
}

- (instancetype)initForToolbarWithIncognito:(BOOL)incognito
                               webStateList:(WebStateList*)webStateList
                              actionFactory:
                                  (BrowserActionFactory*)actionFactory {
  if ((self = [super init])) {
    CHECK(actionFactory);
    _incognito = incognito;
    _webStateList = webStateList;
    _actionFactory = actionFactory;
  }
  return self;
}

- (instancetype)initForAppBarWithIncognito:(BOOL)incognito
                              webStateList:(WebStateList*)webStateList
                             actionFactory:(BrowserActionFactory*)actionFactory
                        templateURLService:
                            (TemplateURLService*)templateURLService
                              tabGridState:(TabGridState*)tabGridState {
  if ((self = [super init])) {
    CHECK(actionFactory);
    CHECK(templateURLService);
    CHECK(tabGridState);
    _incognito = incognito;
    _webStateList = webStateList;
    _actionFactory = actionFactory;
    _templateURLService = templateURLService;
    _tabGridState = tabGridState;
  }
  return self;
}

#pragma mark - Public

- (UIMenu*)menuForNavigationButton:
    (const std::vector<web::NavigationItem*>)navigationItems {
  NSMutableArray<UIMenuElement*>* actions = [NSMutableArray array];

  for (web::NavigationItem* navigationItem : navigationItems) {
    NSString* title;
    UIImage* image;

    if ([self shouldUseIncognitoNTPResourcesForURL:navigationItem
                                                       ->GetVirtualURL()]) {
      title = l10n_util::GetNSStringWithFixup(IDS_IOS_NEW_INCOGNITO_TAB);
      image = SymbolWithPalette(
          CustomSymbolWithPointSize(kIncognitoSymbol, kInfobarSymbolPointSize),
          @[ UIColor.whiteColor ]);
    } else {
      title = base::SysUTF16ToNSString(navigationItem->GetTitleForDisplay());
      const gfx::Image& gfxImage = navigationItem->GetFaviconStatus().image;
      if (!gfxImage.IsEmpty()) {
        image = gfxImage.ToUIImage();
      } else {
        image = DefaultSymbolWithPointSize(kDocSymbol, kInfobarSymbolPointSize);
      }
    }

    __weak __typeof(self) weakSelf = self;
    UIAction* navigateToPageAction = [UIAction
        actionWithTitle:title
                  image:image
             identifier:nil
                handler:^(UIAction* uiAction) {
                  [weakSelf.delegate navigateToPageForItem:navigationItem];
                }];
    [actions addObject:navigateToPageAction];
  }
  return [UIMenu menuWithTitle:@"" children:actions];
}

- (UIMenu*)menuForAssistantButton {
  return nil;
}

- (UIMenu*)menuForNewTabButton {
  BOOL isTabGroupsPageVisible =
      _tabGridState.currentPage == TabGridPageTabGroups;
  BOOL isTabGroupVisible =
      _tabGridState.tabGridVisible && _tabGridState.visibleTabGroup;

  __weak __typeof(self) weakSelf = self;
  ProceduralBlock createNewTabBlock = ^{
    [weakSelf.delegate createNewTabFromView:nil];
  };

  // Context menu for when a tab group is open in the tab grid.
  if (isTabGroupVisible) {
    UIAction* newTabInCurrentGroupAction =
        [_actionFactory actionToAddNewTabInGroupWithBlock:^{
          [weakSelf.delegate addNewTabInCurrentTabGroup];
        }];

    UIAction* newTabAction =
        _incognito
            ? [_actionFactory
                  actionToOpenNewIncognitoTabWithBlock:createNewTabBlock]
            : [_actionFactory actionToOpenNewTabWithBlock:createNewTabBlock];
    if (!_incognito) {
      newTabAction.image =
          DefaultSymbolWithPointSize(kPlusSymbol, kSymbolActionPointSize);
    }
    return
        [UIMenu menuWithChildren:@[ newTabAction, newTabInCurrentGroupAction ]];
  }

  // Context menu for when the tab groups page is visible in the tab grid.
  if (isTabGroupsPageVisible) {
    UIAction* newTabGroupAction =
        [_actionFactory actionToCreateEmptyTabGroupWithBlock:^{
          [weakSelf.delegate createNewTabGroupFromView:nil];
        }];
    newTabGroupAction.title =
        l10n_util::GetNSString(IDS_IOS_APP_BAR_CONTEXT_MENU_NEW_TAB_GROUP);

    UIAction* newTabAction =
        [_actionFactory actionToOpenNewTabWithBlock:createNewTabBlock];
    newTabAction.image =
        DefaultSymbolWithPointSize(kPlusSymbol, kSymbolActionPointSize);

    return [UIMenu menuWithChildren:@[ newTabGroupAction, newTabAction ]];
  }

  // The New Tab button should not have a context menu while viewing the regular
  // or incognito tab pages (unless looking inside a tab group).
  if (_tabGridState.tabGridVisible) {
    return nil;
  }

  // iPad does not have a New Tab button while browsing.
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    return nil;
  }

  // Context menu for while browsing.
  CHECK(_templateURLService);
  const bool useLens =
      lens_availability::CheckAndLogAvailabilityForLensEntryPoint(
          LensEntrypoint::PlusButton,
          search::DefaultSearchProviderIsGoogle(_templateURLService));

  UIAction* newSearchAction = [_actionFactory actionToStartNewSearch];
  UIAction* newIncognitoSearchAction =
      [_actionFactory actionToStartNewIncognitoSearch];
  UIAction* voiceSearchAction = [_actionFactory actionToStartVoiceSearch];
  UIAction* cameraSearchAction =
      useLens
          ? [_actionFactory
                actionToSearchWithLensWithEntryPoint:LensEntrypoint::PlusButton]
          : [_actionFactory actionToShowQRScanner];

  NSMutableArray* staticActions = [NSMutableArray arrayWithArray:@[
    cameraSearchAction, voiceSearchAction, newIncognitoSearchAction,
    newSearchAction
  ]];
  if (experimental_flags::EnableAIPrototypingMenu()) {
    UIAction* openAIMenu = [_actionFactory actionToOpenAIMenu];
    [staticActions addObject:openAIMenu];
  }

  UIMenuElement* clipboardAction = [self createMenuElementForPasteboard];
  if (clipboardAction) {
    UIMenu* staticMenu = [UIMenu menuWithTitle:@""
                                         image:nil
                                    identifier:nil
                                       options:UIMenuOptionsDisplayInline
                                      children:staticActions];
    return [UIMenu menuWithChildren:@[ clipboardAction, staticMenu ]];
  }

  return [UIMenu menuWithTitle:@""
                         image:nil
                    identifier:nil
                       options:UIMenuOptionsDisplayInline
                      children:staticActions];
}

- (UIMenu*)menuForTabGridButton {
  // If the tab grid is showing, the context menu should be disabled.
  if (_tabGridState.tabGridVisible) {
    return nil;
  }

  NSMutableArray* staticActions = [[NSMutableArray alloc] init];

  [staticActions addObject:[_actionFactory actionToOpenNewTab]];
  [staticActions addObject:[_actionFactory actionToOpenNewIncognitoTab]];

  UIAction* closeCurrentTabAction = [_actionFactory actionToCloseCurrentTab];
  [staticActions addObject:closeCurrentTabAction];

  return [UIMenu menuWithTitle:@""
                         image:nil
                    identifier:nil
                       options:UIMenuOptionsDisplayInline
                      children:staticActions];
}

#pragma mark - Private

// Returns the UIMenuElement for the content of the pasteboard. Can return
// `nil`.
- (UIMenuElement*)createMenuElementForPasteboard {
  std::optional<std::set<ClipboardContentType>> clipboardContentType =
      ClipboardRecentContent::GetInstance()->GetCachedClipboardContentTypes();

  if (clipboardContentType.has_value()) {
    std::set<ClipboardContentType> clipboardContentTypeValues =
        clipboardContentType.value();

    if (clipboardContentTypeValues.contains(ClipboardContentType::Image)) {
      if (base::FeatureList::IsEnabled(kEnableLensInOmniboxCopiedImage)) {
        if (search_engines::SupportsSearchImageWithLens(_templateURLService) &&
            ios::provider::IsLensSupported()) {
          return [_actionFactory actionToLensCopiedImage];
        }
      } else {
        if (search_engines::SupportsSearchByImage(_templateURLService)) {
          return [_actionFactory actionToSearchCopiedImage];
        }
      }
    } else if (clipboardContentTypeValues.contains(ClipboardContentType::URL)) {
      return [_actionFactory actionToSearchCopiedURL];
    } else if (clipboardContentTypeValues.contains(
                   ClipboardContentType::Text)) {
      return [_actionFactory actionToSearchCopiedText];
    }
  }
  return nil;
}

// Helper for `-menuForNavigationButton:`. Returns YES if incognito NTP title
// and image should be used for back/forward item associated with `url`.
- (BOOL)shouldUseIncognitoNTPResourcesForURL:(const GURL&)url {
  return IsURLNewTabPage(url) && _incognito;
}

@end
