// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/form_suggestion_controller.h"

#import <memory>

#import "base/apple/foundation_util.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/autofill/core/browser/ui/autofill_suggestion_delegate.h"
#import "components/autofill/core/common/autofill_prefs.h"
#import "components/autofill/ios/browser/autofill_java_script_feature.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/autofill/ios/browser/form_suggestion_provider.h"
#import "components/autofill/ios/form_util/form_activity_params.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/autofill/model/form_input_navigator.h"
#import "ios/chrome/browser/autofill/model/form_input_suggestions_provider.h"
#import "ios/chrome/browser/autofill/model/form_suggestion_default_icons.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/common/url_scheme_util.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/web_state.h"

// Block types for `RunSearchPipeline`.
using PipelineBlock = void (^)(void (^completion)(BOOL));
using PipelineCompletionBlock = void (^)(NSUInteger index);

namespace {

// Executes each PipelineBlock in `blocks` in order until one invokes its
// completion with YES, in which case `on_complete` will be invoked with the
// `index` of the succeeding block, or until they all invoke their completions
// with NO, in which case `on_complete` will be invoked with NSNotFound.
void RunSearchPipeline(NSArray<PipelineBlock>* blocks,
                       PipelineCompletionBlock on_complete,
                       NSUInteger from_index = 0) {
  if (from_index == [blocks count]) {
    on_complete(NSNotFound);
    return;
  }
  PipelineBlock block = blocks[from_index];
  block(^(BOOL success) {
    if (success) {
      on_complete(from_index);
    } else {
      RunSearchPipeline(blocks, on_complete, from_index + 1);
    }
  });
}

// Make a copy of suggestions with `params` and `provider` set in the copies.
NSArray<FormSuggestion*>* SetParamsAndProviderInSuggestions(
    NSArray<FormSuggestion*>* suggestions,
    const autofill::FormActivityParams& params,
    id<FormSuggestionProvider> provider) {
  NSMutableArray<FormSuggestion*>* suggestionsCopy =
      [NSMutableArray<FormSuggestion*> array];
  for (FormSuggestion* suggestion in suggestions) {
    [suggestionsCopy addObject:[FormSuggestion copy:suggestion
                                       andSetParams:params
                                           provider:provider]];
  }
  return suggestionsCopy;
}

}  // namespace

@interface FormSuggestionController () {
  // Providers for suggestions, sorted according to the order in which
  // they should be asked for suggestions, with highest priority in front.
  NSArray* _suggestionProviders;

  // Access to WebView from the CRWWebController.
  id<CRWWebViewProxy> _webViewProxy;
}

// Unique id of the last request.
@property(nonatomic, assign) NSUInteger requestIdentifier;

// Clears state in between page loads.
- (void)resetSuggestionState;

@end

@implementation FormSuggestionController {
  // The WebState this instance is observing. Will be null after
  // -webStateDestroyed: has been called.
  raw_ptr<web::WebState> _webState;

  // Bridge to observe the web state from Objective-C.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;

  // The provider for the current set of suggestions.
  __weak id<FormSuggestionProvider> _provider;
}

@synthesize formInputNavigator = _formInputNavigator;

- (instancetype)initWithWebState:(web::WebState*)webState
                       providers:(NSArray*)providers {
  self = [super init];
  if (self) {
    DCHECK(webState);
    _webState = webState;
    _webStateObserverBridge =
        std::make_unique<web::WebStateObserverBridge>(self);
    _webState->AddObserver(_webStateObserverBridge.get());
    _webViewProxy = webState->GetWebViewProxy();
    _suggestionProviders = [providers copy];
  }
  return self;
}

- (void)dealloc {
  if (_webState) {
    _webState->RemoveObserver(_webStateObserverBridge.get());
    _webStateObserverBridge.reset();
    _webState = nullptr;
  }
}

- (void)detachFromWebState {
  if (_webState) {
    _webState->RemoveObserver(_webStateObserverBridge.get());
    _webStateObserverBridge.reset();
    _webState = nullptr;
  }
}

#pragma mark - CRWWebStateObserver

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  [self detachFromWebState];
}

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  DCHECK_EQ(_webState, webState);
  [self processPage:webState];
}

- (void)processPage:(web::WebState*)webState {
  [self resetSuggestionState];
}

- (void)setWebViewProxy:(id<CRWWebViewProxy>)webViewProxy {
  _webViewProxy = webViewProxy;
}

- (void)retrieveSuggestionsForForm:(const autofill::FormActivityParams&)params
                          webState:(web::WebState*)webState
          accessoryViewUpdateBlock:
              (FormSuggestionsReadyCompletion)accessoryViewUpdateBlock {
  [self processPage:webState];

  self.requestIdentifier += 1;
  NSUInteger requestIdentifier = self.requestIdentifier;

  __weak FormSuggestionController* weakSelf = self;

  FormSuggestionProviderQuery* formQuery = [[FormSuggestionProviderQuery alloc]
      initWithFormName:base::SysUTF8ToNSString(params.form_name)
        formRendererID:params.form_renderer_id
       fieldIdentifier:base::SysUTF8ToNSString(params.field_identifier)
       fieldRendererID:params.field_renderer_id
             fieldType:params.field_type
                  type:params.type
            typedValue:base::SysUTF8ToNSString(params.value)
               frameID:base::SysUTF8ToNSString(params.frame_id)
          onlyPassword:NO];

  BOOL hasUserGesture = params.has_user_gesture;

  // Build a block for each provider that will invoke its completion with YES
  // if the provider can provide suggestions for the specified form/field/type
  // and NO otherwise.
  NSMutableArray* findProviderBlocks = [[NSMutableArray alloc] init];
  for (NSUInteger i = 0; i < [_suggestionProviders count]; i++) {
    PipelineBlock block = ^(void (^completion)(BOOL success)) {
      // Access all the providers through `self` to guarantee that both
      // `self` and all the providers exist when the block is executed.
      // `_suggestionProviders` is immutable, so the subscripting is
      // always valid.
      FormSuggestionController* strongSelf = weakSelf;
      if (!strongSelf) {
        return;
      }
      id<FormSuggestionProvider> provider = strongSelf->_suggestionProviders[i];
      [provider checkIfSuggestionsAvailableForForm:formQuery
                                    hasUserGesture:hasUserGesture
                                          webState:webState
                                 completionHandler:completion];
    };
    [findProviderBlocks addObject:block];
  }

  // Push the retrieved suggestions to the caller.
  SuggestionsReadyCompletion readyCompletion = CallbackToBlock(base::BindOnce(
      [](__weak __typeof(self) weakSelf,
         const autofill::FormActivityParams& params,
         FormSuggestionsReadyCompletion accessoryViewUpdateBlock,
         NSArray<FormSuggestion*>* suggestions,
         id<FormSuggestionProvider> provider) {
        [weakSelf onSuggestionsReady:suggestions
                            provider:provider
                              params:params
                          completion:accessoryViewUpdateBlock];
      },
      weakSelf, params, accessoryViewUpdateBlock));

  // Once a provider is found, use it to retrieve suggestions.
  PipelineCompletionBlock completion = ^(NSUInteger providerIndex) {
    // Ignore outdated results.
    if (weakSelf.requestIdentifier != requestIdentifier) {
      return;
    }
    if (providerIndex == NSNotFound) {
      if (accessoryViewUpdateBlock) {
        accessoryViewUpdateBlock(@[], weakSelf);
      }
      return;
    }
    FormSuggestionController* strongSelf = weakSelf;
    if (!strongSelf) {
      return;
    }
    id<FormSuggestionProvider> provider =
        strongSelf->_suggestionProviders[providerIndex];
    [provider retrieveSuggestionsForForm:formQuery
                                webState:webState
                       completionHandler:readyCompletion];
  };

  // Run all the blocks in `findProviderBlocks` until one invokes its
  // completion with YES. The first one to do so will be passed to
  // `completion`.
  RunSearchPipeline(findProviderBlocks, completion);
}

- (void)onSuggestionsReady:(NSArray<FormSuggestion*>*)suggestions
                  provider:(id<FormSuggestionProvider>)provider
                    params:(const autofill::FormActivityParams&)params
                completion:(FormSuggestionsReadyCompletion)completion {
  _provider = provider;

  if (completion) {
    NSArray<FormSuggestion*>* suggestionsCopy =
        [self copyAndAdjustSuggestions:suggestions];
    suggestionsCopy =
        SetParamsAndProviderInSuggestions(suggestionsCopy, params, provider);
    completion(suggestionsCopy, self);
  }
}

- (void)resetSuggestionState {
  _provider = nil;
}

#pragma mark - FormSuggestionClient

- (void)didSelectSuggestion:(FormSuggestion*)suggestion
                    atIndex:(NSInteger)index
                 completion:(ProceduralBlock)completion {
  // Check that there are always params attached to the suggestion when no
  // params are provided by the -didSelectSuggestion caller itself.
  CHECK(suggestion.params);
  if (!suggestion.params) {
    // Just skip if the check isn't triggered. This is to handle the absence
    // of params when the CHECK isn't fatal.
    if (completion) {
      completion();
    }
    return;
  }

  [self didSelectSuggestion:suggestion
                    atIndex:index
                     params:*suggestion.params
                 completion:completion];
}

- (void)didSelectSuggestion:(FormSuggestion*)suggestion
                    atIndex:(NSInteger)index
                     params:(const autofill::FormActivityParams&)params
                 completion:(ProceduralBlock)completion {
  id<FormSuggestionProvider> provider = suggestion.provider ?: _provider;

  // If a password related suggestion was selected, reset the credential bottom
  // sheet dismiss count to 0.
  if (provider.type == SuggestionProviderTypePassword) {
    [self resetCredentialBottomSheetDismissCount];
  }

  // Send the suggestion to the provider. Upon completion advance the cursor
  // for single-field Autofill, or close the keyboard for full-form Autofill.
  __weak FormSuggestionController* weakSelf = self;
  [provider didSelectSuggestion:suggestion
                        atIndex:index
                           form:base::SysUTF8ToNSString(params.form_name)
                 formRendererID:params.form_renderer_id
                fieldIdentifier:base::SysUTF8ToNSString(params.field_identifier)
                fieldRendererID:params.field_renderer_id
                        frameID:base::SysUTF8ToNSString(params.frame_id)
              completionHandler:^{
                [[weakSelf formInputNavigator] closeKeyboardWithoutButtonPress];
                if (completion) {
                  completion();
                }
              }];
}

#pragma mark - FormInputSuggestionsProvider

- (void)inputAccessoryViewControllerDidReset {
  [self resetSuggestionState];
}

- (SuggestionProviderType)type {
  return _provider ? _provider.type : SuggestionProviderTypeUnknown;
}

- (autofill::FillingProduct)mainFillingProduct {
  return _provider ? _provider.mainFillingProduct
                   : autofill::FillingProduct::kNone;
}

#pragma mark - Private

// Copies the incoming suggestions, making adjustments if necessary.
- (NSArray<FormSuggestion*>*)copyAndAdjustSuggestions:
    (NSArray<FormSuggestion*>*)suggestions {
  return CopyFormSuggestionsWithDefaultIcons(suggestions, _webState);
}

// Resets the credential bottom sheet dismiss count to 0.
- (void)resetCredentialBottomSheetDismissCount {
  ProfileIOS* profile =
      _webState ? ProfileIOS::FromBrowserState(_webState->GetBrowserState())
                : nullptr;
  if (profile) {
    profile->GetPrefs()->SetInteger(prefs::kIosPasswordBottomSheetDismissCount,
                                    0);
  }
}

@end
