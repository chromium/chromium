// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/search_with/search_with_mediator.h"

#import "base/apple/foundation_util.h"
#import "base/ios/ios_util.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "components/search_engines/template_url.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/browser_container/browser_edit_menu_utils.h"
#import "ios/chrome/browser/web_selection/model/web_selection_response.h"
#import "ios/chrome/browser/web_selection/model/web_selection_tab_helper.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {
typedef void (^ProceduralBlockWithItemArray)(NSArray<UIMenuElement*>*);
typedef void (^ProceduralBlockWithBlockWithItemArray)(
    ProceduralBlockWithItemArray);

// Character limit for the search with feature.
const NSUInteger kSearchWithCharacterLimit = 200;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SearchWithContext {
  kNormalGoogle = 0,
  kNormalOther = 1,
  kIncognitoGoogle = 2,
  kIncognitoOther = 3,
  kMaxValue = kIncognitoOther
};

// Log an event when user triggers search with.
void LogTrigger(bool incognito, bool search_engine_google) {
  if (!incognito) {
    if (search_engine_google) {
      base::UmaHistogramEnumeration("IOS.SearchWith.Trigger",
                                    SearchWithContext::kNormalGoogle);
    } else {
      base::UmaHistogramEnumeration("IOS.SearchWith.Trigger",
                                    SearchWithContext::kNormalOther);
    }
  } else {
    if (search_engine_google) {
      base::UmaHistogramEnumeration("IOS.SearchWith.Trigger",
                                    SearchWithContext::kIncognitoGoogle);
    } else {
      base::UmaHistogramEnumeration("IOS.SearchWith.Trigger",
                                    SearchWithContext::kIncognitoOther);
    }
  }
}

}  // namespace

@interface SearchWithMediator ()

// Whether the mediator is handling search with for an incognito tab.
@property(nonatomic, assign) BOOL incognito;

@end

@implementation SearchWithMediator {
  // The Browser's WebStateList.
  base::WeakPtr<WebStateList> _webStateList;

  // The service to retrieve default search engine URL.
  raw_ptr<TemplateURLService> _templateURLService;
}

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                  templateURLService:(TemplateURLService*)templateURLService
                           incognito:(BOOL)incognito {
  if ((self = [super init])) {
    CHECK(webStateList);
    _webStateList = webStateList->AsWeakPtr();
    _incognito = incognito;
    _templateURLService = templateURLService;
  }
  return self;
}

- (void)shutdown {
  _templateURLService = nullptr;
}

- (WebSelectionTabHelper*)webSelectionTabHelper {
  web::WebState* webState =
      _webStateList ? _webStateList->GetActiveWebState() : nullptr;
  if (!webState) {
    return nullptr;
  }
  WebSelectionTabHelper* helper = WebSelectionTabHelper::FromWebState(webState);
  return helper;
}

- (BOOL)canPerformSearch {
  WebSelectionTabHelper* tabHelper = [self webSelectionTabHelper];
  if (!tabHelper || !tabHelper->CanRetrieveSelectedText() ||
      !self.applicationCommandHandler || !_templateURLService ||
      !_templateURLService->GetDefaultSearchProvider()) {
    return NO;
  }
  return YES;
}

- (NSString*)buttonTitle {
  if (![self canPerformSearch]) {
    return @"";
  }
  // Default value
  return l10n_util::GetNSStringF(
      IDS_IOS_SEARCH_WITH_TITLE_SEARCH_WITH,
      _templateURLService->GetDefaultSearchProvider()->short_name());
}

- (void)addItemWithCompletion:(ProceduralBlockWithItemArray)completion {
  WebSelectionTabHelper* tabHelper = [self webSelectionTabHelper];
  if (![self canPerformSearch] || !tabHelper) {
    completion(@[]);
    return;
  }

  __weak __typeof(self) weakSelf = self;
  tabHelper->GetSelectedText(base::BindOnce(^(WebSelectionResponse* response) {
    if (weakSelf) {
      [weakSelf addItemWithResponse:response completion:completion];
    } else {
      completion(@[]);
    }
  }));
}

- (void)addItemWithResponse:(WebSelectionResponse*)response
                 completion:(ProceduralBlockWithItemArray)completion {
  if (!response.valid || ![self canPerformSearch]) {
    completion(@[]);
    return;
  }
  NSString* text = response.selectedText;
  NSString* searchWithMenuTitle = [self buttonTitle];
  if ([[text
          stringByTrimmingCharactersInSet:[NSCharacterSet
                                              whitespaceAndNewlineCharacterSet]]
          length] == 0u ||
      [text length] > kSearchWithCharacterLimit ||
      [searchWithMenuTitle length] == 0) {
    completion(@[]);
    return;
  }

  NSString* searchWithMenuId = @"chromeAction.searchWith";
  __weak __typeof(self) weakSelf = self;
  UIAction* action = [UIAction
      actionWithTitle:searchWithMenuTitle
                image:DefaultSymbolWithPointSize(kMagnifyingglassCircleSymbol,
                                                 kSymbolActionPointSize)
           identifier:searchWithMenuId
              handler:^(UIAction* a) {
                [weakSelf triggerSearchForText:text];
              }];
  completion(@[ action ]);
}

- (void)triggerSearchForText:(NSString*)text {
  if (![self canPerformSearch]) {
    return;
  }
  GURL searchURL =
      _templateURLService->GenerateSearchURLForDefaultSearchProvider(
          base::SysNSStringToUTF16(text));
  if (!searchURL.is_valid()) {
    return;
  }
  const TemplateURL* defaultSearchEngine =
      _templateURLService->GetDefaultSearchProvider();
  const BOOL isDefaultSearchEngineGoogle =
      defaultSearchEngine->GetEngineType(
          _templateURLService->search_terms_data()) ==
      SearchEngineType::SEARCH_ENGINE_GOOGLE;
  LogTrigger(self.incognito, isDefaultSearchEngineGoogle);
  OpenNewTabCommand* command =
      [[OpenNewTabCommand alloc] initWithURL:searchURL
                                    referrer:web::Referrer()
                                 inIncognito:self.incognito
                                inBackground:NO
                                    appendTo:OpenPosition::kCurrentTab];
  [self.applicationCommandHandler openURLInNewTab:command];
}

#pragma mark - EditMenuProvider

- (void)buildMenuWithBuilder:(id<UIMenuBuilder>)builder {
  if (![self canPerformSearch]) {
    return;
  }

  __weak __typeof(self) weakSelf = self;
  ProceduralBlockWithBlockWithItemArray provider =
      ^(ProceduralBlockWithItemArray completion) {
        [weakSelf addItemWithCompletion:completion];
      };
  UIDeferredMenuElement* deferredMenuElement =
      [UIDeferredMenuElement elementWithProvider:provider];
  edit_menu::AddElementToChromeMenu(builder, deferredMenuElement);
}

@end
