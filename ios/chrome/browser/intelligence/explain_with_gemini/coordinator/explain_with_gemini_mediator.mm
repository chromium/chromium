// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/explain_with_gemini/coordinator/explain_with_gemini_mediator.h"

#import "base/apple/foundation_util.h"
#import "base/ios/ios_util.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/browser_content/ui_bundled/browser_edit_menu_utils.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_service.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_service_factory.h"
#import "ios/chrome/browser/intelligence/bwg/utils/bwg_constants.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/bwg_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/web_selection/model/web_selection_response.h"
#import "ios/chrome/browser/web_selection/model/web_selection_tab_helper.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {
typedef void (^ProceduralBlockWithItemArray)(NSArray<UIMenuElement*>*);
typedef void (^ProceduralBlockWithBlockWithItemArray)(
    ProceduralBlockWithItemArray);

}  // namespace

@implementation ExplainWithGeminiMediator {
  raw_ptr<signin::IdentityManager> _identityManager;
  raw_ptr<AuthenticationService> _authService;
}

- (instancetype)initWithIdentityManager:
                    (signin::IdentityManager*)identityManager
                            authService:(AuthenticationService*)authService {
  if ((self = [super init])) {
    _identityManager = identityManager;
    _authService = authService;
  }
  return self;
}

#pragma mark - Private

// Checks if Explain With Gemini can be performed in `webState`.
- (BOOL)canPerformExplainWithGeminiInWebState:(web::WebState*)webState {
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(webState->GetBrowserState());
  raw_ptr<BwgService> geminiService = BwgServiceFactory::GetForProfile(profile);
  const BOOL geminiAvailable =
      geminiService && geminiService->IsBwgAvailableForWebState(webState);
  if (!geminiAvailable) {
    return NO;
  }
  WebSelectionTabHelper* tabHelper =
      WebSelectionTabHelper::FromWebState(webState);
  return tabHelper && tabHelper->CanRetrieveSelectedText() && self.sceneHandler;
}

// Returns the title of button Explain With Gemini.
- (NSString*)buttonTitle {
  return l10n_util::GetNSString(IDS_IOS_EXPLAIN_GEMINI_EDIT_MENU);
}

// Fetches the selection in the web page. On success, trigger a "Explain with
// Gemini" on the selection. This is used on iOS26 where the action must be
// added before the selection is retrieved.
- (void)fetchSelectionForWebState:(base::WeakPtr<web::WebState>)weakWebState {
  if (!weakWebState) {
    return;
  }
  web::WebState* webState = weakWebState.get();
  if (![self canPerformExplainWithGeminiInWebState:webState]) {
    return;
  }
  WebSelectionTabHelper* tabHelper =
      WebSelectionTabHelper::FromWebState(webState);
  __weak __typeof(self) weakSelf = self;
  tabHelper->GetSelectedText(base::BindOnce(^(WebSelectionResponse* response) {
    if (weakSelf && response.valid && response.selectedText.length) {
      [weakSelf triggerExplainWithGeminiForText:response.selectedText
                                       webState:webState];
    }
  }));
}

// Adds Explain With Gemini item to the menu with a completion block.
- (void)addItemForWebState:(base::WeakPtr<web::WebState>)weakWebState
            withCompletion:(ProceduralBlockWithItemArray)completion {
  if (!weakWebState) {
    completion(@[]);
    return;
  }
  web::WebState* webState = weakWebState.get();
  WebSelectionTabHelper* tabHelper =
      WebSelectionTabHelper::FromWebState(webState);
  if (!tabHelper) {
    completion(@[]);
    return;
  }

  __weak __typeof(self) weakSelf = self;
  tabHelper->GetSelectedText(base::BindOnce(^(WebSelectionResponse* response) {
    if (weakSelf) {
      [weakSelf addItemWithResponse:response
                         completion:completion
                           webState:webState];
      return;
    }
    completion(@[]);
  }));
}

// Adds Explain With Gemini item to the menu with a web selection response.
- (void)addItemWithResponse:(WebSelectionResponse*)response
                 completion:(ProceduralBlockWithItemArray)completion
                   webState:(web::WebState*)webState {
  if (!response.valid ||
      ![self canPerformExplainWithGeminiInWebState:webState]) {
    completion(@[]);
    return;
  }
  NSString* text = response.selectedText;
  if ([[text
          stringByTrimmingCharactersInSet:[NSCharacterSet
                                              whitespaceAndNewlineCharacterSet]]
          length] == 0) {
    completion(@[]);
    return;
  }

  __weak __typeof(self) weakSelf = self;
  UIAction* action = [self actionWithHandler:^(UIAction* a) {
    [weakSelf triggerExplainWithGeminiForText:text webState:webState];
  }];
  completion(@[ action ]);
}

// Triggers Explain with Gemini action for the selected text.
- (void)triggerExplainWithGeminiForText:(NSString*)text
                               webState:(web::WebState*)webState {
  CHECK(ExplainGeminiEditMenuPosition() !=
        PositionForExplainGeminiEditMenu::kDisabled);
  if (![self canPerformExplainWithGeminiInWebState:webState]) {
    return;
  }

  NSString* prepopulatedPrompt =
      [NSString stringWithFormat:@"%@ : %@",
                                 l10n_util::GetNSString(
                                     IDS_IOS_EXPLAIN_GEMINI_PROMPT_PREFIX),
                                 text];

  // TODO(crbug.com/483004001): Add metrics logging.
  GeminiStartupState* startupState = [[GeminiStartupState alloc]
      initWithEntryPoint:gemini::EntryPoint::EditMenu];
  startupState.prepopulatedPrompt = prepopulatedPrompt;
  [self.BWGHandler startGeminiFlowWithStartupState:startupState];
}

// Returns the action to trigger the search with feature. Calls `handler` on
// activation.
- (UIAction*)actionWithHandler:(void (^)(UIAction*))handler {
  NSString* explainWithGeminiMenuId = @"chromeAction.explainGemini";
  NSString* explainWithGeminiMenuTitle = [self buttonTitle];
  return [UIAction actionWithTitle:explainWithGeminiMenuTitle
                             image:nil
                        identifier:explainWithGeminiMenuId
                           handler:handler];
}

#pragma mark - EditMenuBuilder

- (void)buildEditMenuWithBuilder:(id<UIMenuBuilder>)builder
                      inWebState:(web::WebState*)webState {
  if (!webState) {
    return;
  }

  if (![self canPerformExplainWithGeminiInWebState:webState]) {
    return;
  }

  base::WeakPtr<web::WebState> weakWebState = webState->GetWeakPtr();
  __weak __typeof(self) weakSelf = self;
  UIMenuElement* menuElement = nil;
  if (ShouldShowEditMenuItemsSynchronously()) {
    menuElement = [self actionWithHandler:^(UIAction* a) {
      [weakSelf fetchSelectionForWebState:weakWebState];
    }];
  } else {
    ProceduralBlockWithBlockWithItemArray provider =
        ^(ProceduralBlockWithItemArray completion) {
          [weakSelf addItemForWebState:weakWebState withCompletion:completion];
        };
    menuElement = [UIDeferredMenuElement elementWithProvider:provider];
  }

  if (ExplainGeminiEditMenuPosition() ==
      PositionForExplainGeminiEditMenu::kAfterSearch) {
    edit_menu::AddElementToChromeMenu(builder, menuElement,
                                      /*primary*/ YES);
    return;
  }
  if (ExplainGeminiEditMenuPosition() ==
      PositionForExplainGeminiEditMenu::kAfterEdit) {
    edit_menu::AddElementToChromeMenu(builder, menuElement,
                                      /*primary*/ NO);
    return;
  }
  NOTREACHED();
}

@end
