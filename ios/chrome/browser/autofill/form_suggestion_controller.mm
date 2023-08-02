// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/form_suggestion_controller.h"

#import <memory>

#import "base/mac/foundation_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/autofill/core/browser/ui/autofill_popup_delegate.h"
#import "components/autofill/core/browser/ui/popup_types.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/autofill/ios/browser/form_suggestion_provider.h"
#import "components/autofill/ios/form_util/form_activity_params.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/autofill/form_input_navigator.h"
#import "ios/chrome/browser/autofill/form_input_suggestions_provider.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/web/common/url_scheme_util.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/web_state.h"

using autofill::FieldRendererId;
using autofill::FormRendererId;
// Block types for `RunSearchPipeline`.
using PipelineBlock = void (^)(void (^completion)(BOOL));
using PipelineCompletionBlock = void (^)(NSUInteger index);

namespace {

// Struct that describes suggestion state.
struct AutofillSuggestionState {
  AutofillSuggestionState(const autofill::FormActivityParams& params);
  // The name of the form for autofill.
  std::string form_name;
  // The unique numeric identifier of the form for autofill.
  FormRendererId unique_form_id;
  // The identifier of the field for autofill.
  std::string field_identifier;
  // The unique numeric identifier of the field for autofill.
  FieldRendererId unique_field_id;
  // The identifier of the frame for autofill.
  std::string frame_identifier;
  // The user-typed value in the field.
  std::string typed_value;
  // The suggestions for the form field. An array of `FormSuggestion`.
  NSArray* suggestions;
};

AutofillSuggestionState::AutofillSuggestionState(
    const autofill::FormActivityParams& params)
    : form_name(params.form_name),
      unique_form_id(params.unique_form_id),
      field_identifier(params.field_identifier),
      unique_field_id(params.unique_field_id),
      frame_identifier(params.frame_id),
      typed_value(params.value) {}

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

}  // namespace

@interface FormSuggestionController () {
  // Callback to update the accessory view.
  FormSuggestionsReadyCompletion _accessoryViewUpdateBlock;

  // Autofill suggestion state.
  std::unique_ptr<AutofillSuggestionState> _suggestionState;

  // Providers for suggestions, sorted according to the order in which
  // they should be asked for suggestions, with highest priority in front.
  NSArray* _suggestionProviders;

  // Access to WebView from the CRWWebController.
  id<CRWWebViewProxy> _webViewProxy;
}

// Unique id of the last request.
@property(nonatomic, assign) NSUInteger requestIdentifier;

// Updates keyboard for `suggestionState`.
- (void)updateKeyboard:(AutofillSuggestionState*)suggestionState;

// Updates keyboard with `suggestions`.
- (void)updateKeyboardWithSuggestions:(NSArray*)suggestions;

// Clears state in between page loads.
- (void)resetSuggestionState;

@end

@implementation FormSuggestionController {
  // The WebState this instance is observing. Will be null after
  // -webStateDestroyed: has been called.
  web::WebState* _webState;

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
                          webState:(web::WebState*)webState {
  self.requestIdentifier += 1;
  NSUInteger requestIdentifier = self.requestIdentifier;

  __weak FormSuggestionController* weakSelf = self;

  FormSuggestionProviderQuery* formQuery = [[FormSuggestionProviderQuery alloc]
      initWithFormName:base::SysUTF8ToNSString(params.form_name)
          uniqueFormID:params.unique_form_id
       fieldIdentifier:base::SysUTF8ToNSString(params.field_identifier)
         uniqueFieldID:params.unique_field_id
             fieldType:base::SysUTF8ToNSString(params.field_type)
                  type:base::SysUTF8ToNSString(params.type)
            typedValue:base::SysUTF8ToNSString(
                           _suggestionState.get()->typed_value)
               frameID:base::SysUTF8ToNSString(params.frame_id)];

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
      if (!strongSelf)
        return;
      id<FormSuggestionProvider> provider = strongSelf->_suggestionProviders[i];
      [provider checkIfSuggestionsAvailableForForm:formQuery
                                    hasUserGesture:hasUserGesture
                                          webState:webState
                                 completionHandler:completion];
    };
    [findProviderBlocks addObject:block];
  }

  // Once the suggestions are retrieved, update the suggestions UI.
  SuggestionsReadyCompletion readyCompletion =
      ^(NSArray<FormSuggestion*>* suggestions,
        id<FormSuggestionProvider> provider) {
        [weakSelf onSuggestionsReady:suggestions provider:provider];
      };

  // Once a provider is found, use it to retrieve suggestions.
  PipelineCompletionBlock completion = ^(NSUInteger providerIndex) {
    // Ignore outdated results.
    if (weakSelf.requestIdentifier != requestIdentifier) {
      return;
    }
    if (providerIndex == NSNotFound) {
      [weakSelf onNoSuggestionsAvailable];
      return;
    }
    FormSuggestionController* strongSelf = weakSelf;
    if (!strongSelf)
      return;
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

- (void)onNoSuggestionsAvailable {
  // Check the update block hasn't been reset while waiting for suggestions.
  if (!_accessoryViewUpdateBlock) {
    return;
  }
  _accessoryViewUpdateBlock(@[], self);
}

- (void)onSuggestionsReady:(NSArray<FormSuggestion*>*)suggestions
                  provider:(id<FormSuggestionProvider>)provider {
  // TODO(ios): crbug.com/249916. If we can also pass in the form/field for
  // which `suggestions` are, we should check here if `suggestions` are for
  // the current active element. If not, reset `_suggestionState`.
  if (!_suggestionState) {
    // The suggestion state was reset in between the call to Autofill API (e.g.
    // OnAskForValuesToFill) and this method being called back. Results are
    // therefore no longer relevant.
    return;
  }

  _provider = provider;
  _suggestionState->suggestions = [suggestions copy];
  [self updateKeyboard:_suggestionState.get()];
}

- (void)resetSuggestionState {
  _provider = nil;
  _suggestionState.reset();
}

- (void)clearSuggestions {
  // Note that other parts of the suggestionsState are not reset.
  if (!_suggestionState.get())
    return;
  _suggestionState->suggestions = [[NSArray alloc] init];
  [self updateKeyboard:_suggestionState.get()];
}

- (void)updateKeyboard:(AutofillSuggestionState*)suggestionState {
  if (!suggestionState) {
    if (_accessoryViewUpdateBlock)
      _accessoryViewUpdateBlock(nil, self);
  } else {
    [self updateKeyboardWithSuggestions:suggestionState->suggestions];
  }
}

- (void)updateKeyboardWithSuggestions:(NSArray<FormSuggestion*>*)suggestions {
  if (_accessoryViewUpdateBlock) {
    _accessoryViewUpdateBlock(suggestions, self);
  }
}

#pragma mark - FormSuggestionClient

- (void)didSelectSuggestion:(FormSuggestion*)suggestion {
  const AutofillSuggestionState* suggestionState = _suggestionState.get();
  if (suggestionState) {
    [self didSelectSuggestion:suggestion state:(*suggestionState)];
  }
}

- (void)didSelectSuggestion:(FormSuggestion*)suggestion
                     params:(const autofill::FormActivityParams&)params {
  AutofillSuggestionState suggestionState(params);
  [self didSelectSuggestion:suggestion state:suggestionState];
}

#pragma mark - FormInputSuggestionsProvider

- (void)retrieveSuggestionsForForm:(const autofill::FormActivityParams&)params
                          webState:(web::WebState*)webState
          accessoryViewUpdateBlock:
              (FormSuggestionsReadyCompletion)accessoryViewUpdateBlock {
  [self processPage:webState];
  _suggestionState.reset(new AutofillSuggestionState(params));
  _accessoryViewUpdateBlock = [accessoryViewUpdateBlock copy];
  [self retrieveSuggestionsForForm:params webState:webState];
}

- (void)inputAccessoryViewControllerDidReset {
  _accessoryViewUpdateBlock = nil;
  [self resetSuggestionState];
}

- (SuggestionProviderType)type {
  return _provider ? _provider.type : SuggestionProviderTypeUnknown;
}

- (autofill::PopupType)suggestionType {
  return _provider ? _provider.suggestionType
                   : autofill::PopupType::kUnspecified;
}

#pragma mark - Private

// Performs the suggestion selection based on the provided suggestion state.
- (void)didSelectSuggestion:(FormSuggestion*)suggestion
                      state:(const AutofillSuggestionState&)suggestionState {
  // If a suggestion was selected, reset the password bottom sheet dismiss count
  // to 0.
  [self resetPasswordBottomSheetDismissCount];

  // Send the suggestion to the provider. Upon completion advance the cursor
  // for single-field Autofill, or close the keyboard for full-form Autofill.
  __weak FormSuggestionController* weakSelf = self;
  [_provider
      didSelectSuggestion:suggestion
                     form:base::SysUTF8ToNSString(suggestionState.form_name)
             uniqueFormID:suggestionState.unique_form_id
          fieldIdentifier:base::SysUTF8ToNSString(
                              suggestionState.field_identifier)
            uniqueFieldID:suggestionState.unique_field_id
                  frameID:base::SysUTF8ToNSString(
                              suggestionState.frame_identifier)
        completionHandler:^{
          [[weakSelf formInputNavigator] closeKeyboardWithoutButtonPress];
        }];
}

// Resets the password bottom sheet dismiss count to 0.
- (void)resetPasswordBottomSheetDismissCount {
  ChromeBrowserState* browserState =
      _webState
          ? ChromeBrowserState::FromBrowserState(_webState->GetBrowserState())
          : nullptr;
  if (browserState) {
    int dismissCount = browserState->GetPrefs()->GetInteger(
        prefs::kIosPasswordBottomSheetDismissCount);
    browserState->GetPrefs()->SetInteger(
        prefs::kIosPasswordBottomSheetDismissCount, 0);
    if (dismissCount > 0) {
      // Log how many times the bottom sheet had been dismissed before being
      // re-enabled.
      static constexpr int kHistogramMin = 1;
      static constexpr int kHistogramMax = 4;
      static constexpr size_t kHistogramBuckets = 3;
      base::UmaHistogramCustomCounts(
          "IOS.ResetDismissCount.Password.BottomSheet", dismissCount,
          kHistogramMin, kHistogramMax, kHistogramBuckets);
    }
  }
}

@end
