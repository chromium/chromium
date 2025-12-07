// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/search_with/ui_bundled/search_with_mediator.h"

#import "base/apple/foundation_util.h"
#import "base/ios/ios_util.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "components/search_engines/template_url.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/browser_container/ui_bundled/browser_edit_menu_utils.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
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
  SearchWithContext context;
  if (incognito) {
    context = search_engine_google ? SearchWithContext::kIncognitoGoogle
                                   : SearchWithContext::kIncognitoOther;
  } else {
    context = search_engine_google ? SearchWithContext::kNormalGoogle
                                   : SearchWithContext::kNormalOther;
  }
  base::UmaHistogramEnumeration("IOS.SearchWith.Trigger", context);
}

// Log the number of characters selected.
void LogSelectedNumberChar(NSUInteger textLength) {
  base::UmaHistogramCounts1000("IOS.SearchWith.CharSelected", textLength);
}

}  // namespace

@interface SearchWithMediator ()

// Whether the mediator is handling search with for an incognito tab.
@property(nonatomic, assign) BOOL incognito;

@end

@implementation SearchWithMediator {
  // The service to retrieve default search engine URL.
  raw_ptr<TemplateURLService, DanglingUntriaged> _templateURLService;
}

- (instancetype)initWithTemplateURLService:
                    (TemplateURLService*)templateURLService
                                 incognito:(BOOL)incognito {
  if ((self = [super init])) {
    _incognito = incognito;
    _templateURLService = templateURLService;
  }
  return self;
}

- (void)shutdown {
  _templateURLService = nullptr;
}

#pragma mark - Private

// Whether a search can be performed on the current page presented in
// `webState`.
- (BOOL)canPerformSearchInWebState:(web::WebState*)webState {
  if (!webState) {
    return NO;
  }
  WebSelectionTabHelper* tabHelper =
      WebSelectionTabHelper::FromWebState(webState);
  if (!tabHelper || !tabHelper->CanRetrieveSelectedText() ||
      !self.applicationCommandHandler || !_templateURLService ||
      !_templateURLService->GetDefaultSearchProvider()) {
    return NO;
  }
  return YES;
}

// The title for the `Search with` button.
- (NSString*)buttonTitle {
  // Default value
  return l10n_util::GetNSStringF(
      IDS_IOS_SEARCH_WITH_TITLE_SEARCH_WITH,
      _templateURLService->GetDefaultSearchProvider()->short_name());
}

// Fetches the selection in the web page. On success, trigger a search on the
// selection. This is used on iOS26 where the action must be added before the
// selection is retrieved.
- (void)fetchSelectionForWebState:(base::WeakPtr<web::WebState>)weakWebState {
  if (!weakWebState) {
    return;
  }
  web::WebState* webState = weakWebState.get();
  if (![self canPerformSearchInWebState:webState]) {
    return;
  }
  WebSelectionTabHelper* tabHelper =
      WebSelectionTabHelper::FromWebState(webState);
  __weak __typeof(self) weakSelf = self;
  tabHelper->GetSelectedText(base::BindOnce(^(WebSelectionResponse* response) {
    if (weakSelf && response.valid && response.selectedText.length) {
      [weakSelf triggerSearchForText:response.selectedText];
    }
  }));
}

// Fetches the selection in the web page. On success, add the action in the menu
// to trigger a search.
- (void)addItemForWebState:(base::WeakPtr<web::WebState>)weakWebState
            withCompletion:(ProceduralBlockWithItemArray)completion {
  if (!weakWebState) {
    completion(@[]);
    return;
  }
  web::WebState* webState = weakWebState.get();
  if (![self canPerformSearchInWebState:webState]) {
    completion(@[]);
    return;
  }
  WebSelectionTabHelper* tabHelper =
      WebSelectionTabHelper::FromWebState(webState);

  __weak __typeof(self) weakSelf = self;
  tabHelper->GetSelectedText(base::BindOnce(^(WebSelectionResponse* response) {
    if (weakSelf) {
      [weakSelf addItemWithResponse:response completion:completion];
    } else {
      completion(@[]);
    }
  }));
}

// Adds the search button if the selection is valid.
- (void)addItemWithResponse:(WebSelectionResponse*)response
                 completion:(ProceduralBlockWithItemArray)completion {
  if (!response.valid) {
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

  __weak __typeof(self) weakSelf = self;
  UIAction* action = [self actionWithHandler:^(UIAction* a) {
    [weakSelf triggerSearchForText:text];
  }];
  completion(@[ action ]);
}

// Triggeres a search for `text`.
- (void)triggerSearchForText:(NSString*)text {
  if (!_templateURLService ||
      !_templateURLService->GetDefaultSearchProvider()) {
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
  LogSelectedNumberChar([text length]);
  OpenNewTabCommand* command =
      [[OpenNewTabCommand alloc] initWithURL:searchURL
                                    referrer:web::Referrer()
                                 inIncognito:self.incognito
                                inBackground:NO
                                    appendTo:OpenPosition::kCurrentTab];
  [self.applicationCommandHandler openURLInNewTab:command];
}

// Returns the action to trigger the search with feature. Calls `handler` on
// activation.
- (UIAction*)actionWithHandler:(void (^)(UIAction*))handler {
  return [UIAction
      actionWithTitle:[self buttonTitle]
                image:DefaultSymbolWithPointSize(kMagnifyingglassCircleSymbol,
                                                 kSymbolActionPointSize)
           identifier:@"chromeAction.searchWith"
              handler:handler];
}

#pragma mark - EditMenuBuilder

- (void)buildEditMenuWithBuilder:(id<UIMenuBuilder>)builder
                      inWebState:(web::WebState*)webState {
  if (!webState) {
    return;
  }
  if (![self canPerformSearchInWebState:webState]) {
    return;
  }

  __weak __typeof(self) weakSelf = self;
  base::WeakPtr<web::WebState> weakWebState = webState->GetWeakPtr();
  if (ShouldShowEditMenuItemsSynchronously()) {
    UIAction* action = [self actionWithHandler:^(UIAction* a) {
      [weakSelf fetchSelectionForWebState:weakWebState];
    }];
    edit_menu::AddElementToChromeMenu(builder, action,
                                      /*primary*/ YES);
  } else {
    ProceduralBlockWithBlockWithItemArray provider =
        ^(ProceduralBlockWithItemArray completion) {
          [weakSelf addItemForWebState:weakWebState withCompletion:completion];
        };
    UIDeferredMenuElement* deferredMenuElement =
        [UIDeferredMenuElement elementWithProvider:provider];
    edit_menu::AddElementToChromeMenu(builder, deferredMenuElement,
                                      /*primary*/ YES);
  }
}

@end
