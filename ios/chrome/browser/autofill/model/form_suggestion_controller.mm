// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/form_suggestion_controller.h"

#import <memory>

#import "base/apple/foundation_util.h"
#import "base/feature_list.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/not_fatal_until.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/autofill/core/browser/ui/autofill_suggestion_delegate.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/autofill/ios/browser/form_suggestion_provider.h"
#import "components/autofill/ios/form_util/form_activity_params.h"
#import "components/plus_addresses/core/common/features.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/autofill/model/features.h"
#import "ios/chrome/browser/autofill/model/form_input_navigator.h"
#import "ios/chrome/browser/autofill/model/form_input_suggestions_provider.h"
#import "ios/chrome/browser/autofill/model/form_suggestion_controller.mm"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
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

// Point size of the SF Symbol used for default icons.
const CGFloat kSymbolPointSize = 17.0f;

// Struct that describes suggestion state.
struct AutofillSuggestionState {
  AutofillSuggestionState(const autofill::FormActivityParams& params);
  // The name of the form for autofill.
  std::string form_name;
  // The numeric identifier of the form for autofill.
  FormRendererId form_renderer_id;
  // The identifier of the field for autofill.
  std::string field_identifier;
  // The numeric identifier of the field for autofill.
  FieldRendererId field_renderer_id;
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
      form_renderer_id(params.form_renderer_id),
      field_identifier(params.field_identifier),
      field_renderer_id(params.field_renderer_id),
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

// Returns the default icon for the suggestion type.
UIImage* defaultIconForType(FormSuggestion* suggestion) {
  switch (suggestion.type) {
    case autofill::SuggestionType::kGeneratePasswordEntry:
      return MakeSymbolMulticolor(
          CustomSymbolWithPointSize(kPasswordManagerSymbol, kSymbolPointSize));
    case autofill::SuggestionType::kFillExistingPlusAddress: {
      BOOL isPlusAddressFeaturesEnabled = base::FeatureList::IsEnabled(
          plus_addresses::features::kPlusAddressesEnabled);
      return isPlusAddressFeaturesEnabled
                 ? SymbolWithPalette(DefaultSymbolWithPointSize(
                                         kShieldedEnvelope, kSymbolPointSize),
                                     @[
                                       [UIColor colorNamed:kTextPrimaryColor],
                                     ])
                 : nil;
    }
    case autofill::SuggestionType::kAddressEntry: {
      switch (suggestion.suggestionIconType) {
        case SuggestionIconType::kAccountHome:
          return SymbolWithPalette(
              DefaultSymbolWithPointSize(kHomeSymbol, kSymbolPointSize), @[
                [UIColor colorNamed:kTextPrimaryColor],
              ]);
        case SuggestionIconType::kAccountWork:
          return SymbolWithPalette(
              DefaultSymbolWithPointSize(kWorkSymbol, kSymbolPointSize), @[
                [UIColor colorNamed:kTextPrimaryColor],
              ]);
        default:
          return nil;
      }
    }
    case autofill::SuggestionType::kAutocompleteEntry:
    default:
      return nil;
  }
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

// Returns true if the form suggestion controller is stateless.
bool IsStateless() {
  return base::FeatureList::IsEnabled(kStatelessFormSuggestionController);
}

// Returns true if deduping requests is allowed.
bool IsRequestDedupingAllowed() {
  return !IsStateless() ||
         base::FeatureList::IsEnabled(
             kStatelessFormSuggestionControllerWithRequestDeduping);
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
  _suggestionState = std::make_unique<AutofillSuggestionState>(params);
  _accessoryViewUpdateBlock = [accessoryViewUpdateBlock copy];

  self.requestIdentifier += 1;
  NSUInteger requestIdentifier = self.requestIdentifier;

  __weak FormSuggestionController* weakSelf = self;

  FormSuggestionProviderQuery* formQuery = [[FormSuggestionProviderQuery alloc]
      initWithFormName:base::SysUTF8ToNSString(params.form_name)
        formRendererID:params.form_renderer_id
       fieldIdentifier:base::SysUTF8ToNSString(params.field_identifier)
       fieldRendererID:params.field_renderer_id
             fieldType:base::SysUTF8ToNSString(params.field_type)
                  type:base::SysUTF8ToNSString(params.type)
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
    // Ignore outdated results if allowed.
    if (weakSelf.requestIdentifier != requestIdentifier &&
        IsRequestDedupingAllowed()) {
      return;
    }
    if (providerIndex == NSNotFound) {
      if (IsStateless() && accessoryViewUpdateBlock) {
        accessoryViewUpdateBlock(@[], weakSelf);
      } else if (!IsStateless()) {
        [weakSelf onNoSuggestionsAvailable];
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

- (void)onNoSuggestionsAvailable {
  // Do not call this no suggestions handler when stateless.
  CHECK(!IsStateless());

  // Check the update block hasn't been reset while waiting for suggestions.
  if (!_accessoryViewUpdateBlock) {
    return;
  }
  _accessoryViewUpdateBlock(@[], self);
}

- (void)onSuggestionsReady:(NSArray<FormSuggestion*>*)suggestions
                  provider:(id<FormSuggestionProvider>)provider
                    params:(const autofill::FormActivityParams&)params
                completion:(FormSuggestionsReadyCompletion)completion {
  // TODO(ios): crbug.com/249916. If we can also pass in the form/field for
  // which `suggestions` are, we should check here if `suggestions` are for
  // the current active element. If not, reset `_suggestionState`.
  if (!_suggestionState && !IsStateless()) {
    // The suggestion state was reset in between the call to Autofill API (e.g.
    // OnAskForValuesToFill) and this method being called back. Results are
    // therefore no longer relevant.
    return;
  }

  NSArray<FormSuggestion*>* suggestionsCopy =
      [self copyAndAdjustSuggestions:suggestions];

  // Cache the provider and state for the stateful controller. Those should not
  // be used when the provider is stateless.
  _provider = provider;
  if (_suggestionState) {
    // This case is only reached when using the stateless suggestion controller
    // where the `_suggestionState` isn't required, so updating the suggestions
    // can be skipped when no `_suggestionState`.
    _suggestionState->suggestions = [self copyAndAdjustSuggestions:suggestions];
  }

  if (IsStateless()) {
    // Use the stateless way for passing suggestions when the feature is
    // enabled.
    if (completion) {
      suggestionsCopy =
          SetParamsAndProviderInSuggestions(suggestionsCopy, params, provider);
      completion(suggestionsCopy, self);
    }
  } else {
    // Call the accessory view update block.
    [self updateKeyboard:_suggestionState.get()];
  }
}

- (void)resetSuggestionState {
  _provider = nil;
  _suggestionState.reset();
}

- (void)updateKeyboard:(AutofillSuggestionState*)suggestionState {
  CHECK(!IsStateless());
  if (!suggestionState) {
    if (_accessoryViewUpdateBlock) {
      _accessoryViewUpdateBlock(nil, self);
    }
  } else {
    [self updateKeyboardWithSuggestions:suggestionState->suggestions];
  }
}

- (void)updateKeyboardWithSuggestions:(NSArray<FormSuggestion*>*)suggestions {
  CHECK(!IsStateless());
  if (_accessoryViewUpdateBlock) {
    _accessoryViewUpdateBlock(suggestions, self);
  }
}

#pragma mark - FormSuggestionClient

- (void)didSelectSuggestion:(FormSuggestion*)suggestion
                    atIndex:(NSInteger)index {
  if (IsStateless()) {
    // Check that there are always params attached to the suggestion when no
    // params are provided by the -didSelectSuggestion caller itself.
    CHECK(suggestion.params);
    if (!suggestion.params) {
      // Just skip if the check isn't triggered. This is to handle the absence
      // of params when the CHECK isn't fatal.
      return;
    }

    [self didSelectSuggestion:suggestion
                      atIndex:index
                        state:AutofillSuggestionState(*suggestion.params)];
  } else if (_suggestionState) {
    [self didSelectSuggestion:suggestion
                      atIndex:index
                        state:(*_suggestionState)];
  }
}

- (void)didSelectSuggestion:(FormSuggestion*)suggestion
                    atIndex:(NSInteger)index
                     params:(const autofill::FormActivityParams&)params {
  AutofillSuggestionState suggestionState(params);
  [self didSelectSuggestion:suggestion atIndex:index state:suggestionState];
}

#pragma mark - FormInputSuggestionsProvider

- (void)inputAccessoryViewControllerDidReset {
  _accessoryViewUpdateBlock = nil;
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
  NSMutableArray<FormSuggestion*>* suggestionsCopy = [NSMutableArray array];
  for (FormSuggestion* suggestion : suggestions) {
    UIImage* defaultIcon = defaultIconForType(suggestion);

    // If there are no icons, but we have a default icon for this suggestion,
    // copy the suggestion and add the default icon, otherwise, update the icon
    // for this suggestion.
    BOOL shouldUpdateIcon = !suggestion.icon && defaultIcon;

    if (shouldUpdateIcon) {
      // If we ever get suggestions with metadata here, we'll need to use a
      // different [FormSuggestion suggestionWithValue:...] to perform the copy.
      CHECK(!suggestion.metadata.is_single_username_form);

      FormSuggestion* suggestionCopy = [FormSuggestion
                  suggestionWithValue:suggestion.value
                           minorValue:suggestion.minorValue
                   displayDescription:suggestion.displayDescription
                                 icon:defaultIcon
                                 type:suggestion.type
                              payload:suggestion.payload
          fieldByFieldFillingTypeUsed:suggestion.fieldByFieldFillingTypeUsed
                       requiresReauth:suggestion.requiresReauth
           acceptanceA11yAnnouncement:suggestion.acceptanceA11yAnnouncement];
      // TODO(crbug.com/452315148): Include `featureForIPH` in the
      // `FormSuggestion` constructor.
      suggestionCopy.featureForIPH = suggestion.featureForIPH;
      [suggestionsCopy addObject:suggestionCopy];
    } else {
      [suggestionsCopy addObject:suggestion];
    }
  }
  return suggestionsCopy;
}

// Performs the selection of the suggestion at the provided `index` based on the
// provided `suggestionState`.
- (void)didSelectSuggestion:(FormSuggestion*)suggestion
                    atIndex:(NSInteger)index
                      state:(const AutofillSuggestionState&)suggestionState {
  id<FormSuggestionProvider> provider = suggestion.provider ?: _provider;

  // If a password related suggestion was selected, reset the credential bottom
  // sheet dismiss count to 0.
  if (provider.type == SuggestionProviderTypePassword) {
    [self resetCredentialBottomSheetDismissCount];
  }

  // Send the suggestion to the provider. Upon completion advance the cursor
  // for single-field Autofill, or close the keyboard for full-form Autofill.
  __weak FormSuggestionController* weakSelf = self;
  [provider
      didSelectSuggestion:suggestion
                  atIndex:index
                     form:base::SysUTF8ToNSString(suggestionState.form_name)
           formRendererID:suggestionState.form_renderer_id
          fieldIdentifier:base::SysUTF8ToNSString(
                              suggestionState.field_identifier)
          fieldRendererID:suggestionState.field_renderer_id
                  frameID:base::SysUTF8ToNSString(
                              suggestionState.frame_identifier)
        completionHandler:^{
          [[weakSelf formInputNavigator] closeKeyboardWithoutButtonPress];
        }];
}

// Resets the credential bottom sheet dismiss count to 0.
- (void)resetCredentialBottomSheetDismissCount {
  ProfileIOS* profile =
      _webState ? ProfileIOS::FromBrowserState(_webState->GetBrowserState())
                : nullptr;
  if (profile) {
    int dismissCount = profile->GetPrefs()->GetInteger(
        prefs::kIosPasswordBottomSheetDismissCount);
    profile->GetPrefs()->SetInteger(prefs::kIosPasswordBottomSheetDismissCount,
                                    0);
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
