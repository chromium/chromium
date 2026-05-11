// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/coordinator/composebox_input_state_manager.h"

#import <algorithm>
#import <ranges>
#import <unordered_map>

#import "base/debug/dump_without_crashing.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/contextual_search/contextual_search_metrics_recorder.h"
#import "components/contextual_search/contextual_search_session_handle.h"
#import "components/omnibox/browser/aim_eligibility_service.h"
#import "components/prefs/pref_service.h"
#import "components/search/search.h"
#import "components/search_engines/template_url_service.h"
#import "components/search_engines/util.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_constants.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_mode_holder.h"
#import "ios/chrome/browser/composebox/public/features.h"
#import "ios/chrome/browser/composebox/ui/composebox_input_item.h"
#import "ios/chrome/browser/composebox/ui/composebox_input_item_collection.h"
#import "ios/chrome/browser/composebox/ui/composebox_strings.h"
#import "ios/chrome/browser/composebox/ui/composebox_ui_input_state.h"
#import "ios/chrome/browser/search_engines/model/search_engine_observer_bridge.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/common/NSString+Chromium.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_id.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Returns the model option required by the given model mode.
ComposeboxModelOption ModelOptionForModelMode(omnibox::ModelMode model_mode) {
  using enum ComposeboxModelOption;
  switch (model_mode) {
    case omnibox::ModelMode::MODEL_MODE_GEMINI_PRO_AUTOROUTE:
      return kAuto;
    case omnibox::ModelMode::MODEL_MODE_GEMINI_PRO:
      return kThinking;
    case omnibox::ModelMode::MODEL_MODE_GEMINI_PRO_NO_GEN_UI:
      return kThinkingNoGenUI;
    case omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR:
    case omnibox::ModelMode::MODEL_MODE_UNSPECIFIED:
      return kRegular;
    default:
      base::debug::DumpWithoutCrashing();
      return kRegular;
  }
}

// Returns the model mode for the given model option.
omnibox::ModelMode ModelModeForModelOption(
    ComposeboxModelOption modelOption,
    const contextual_search::InputState& input_state) {
  using enum ComposeboxModelOption;
  switch (modelOption) {
    case kNone:
      return input_state.GetDefaultModel();
    case kRegular:
      return omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR;
    case kAuto:
      return omnibox::ModelMode::MODEL_MODE_GEMINI_PRO_AUTOROUTE;
    case kThinking:
      return omnibox::ModelMode::MODEL_MODE_GEMINI_PRO;
    case kThinkingNoGenUI:
      return omnibox::ModelMode::MODEL_MODE_GEMINI_PRO_NO_GEN_UI;
  }
}

// Returns the input plate control for the given tool mode.
std::optional<ComposeboxMode> ModeForToolMode(omnibox::ToolMode tool_mode) {
  using enum ComposeboxMode;
  switch (tool_mode) {
    case omnibox::ToolMode::TOOL_MODE_CANVAS:
      return kCanvas;
    case omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH:
      return kDeepSearch;
    case omnibox::ToolMode::TOOL_MODE_IMAGE_GEN:
    case omnibox::ToolMode::TOOL_MODE_IMAGE_GEN_UPLOAD:
    case omnibox::ToolMode::TOOL_MODE_IMAGE_GEN_SELFIE:
      return kImageGeneration;
    case omnibox::ToolMode::TOOL_MODE_AIM:
      return kAIM;
    default:
      return std::nullopt;
  }
}

// Returns the tool mode for the given composebox mode.
omnibox::ToolMode ToolModeForComposeboxMode(ComposeboxMode mode,
                                            BOOL hasImage) {
  switch (mode) {
    case ComposeboxMode::kCanvas:
      return omnibox::ToolMode::TOOL_MODE_CANVAS;
    case ComposeboxMode::kDeepSearch:
      return omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH;
    case ComposeboxMode::kImageGeneration:
      return hasImage ? omnibox::ToolMode::TOOL_MODE_IMAGE_GEN_UPLOAD
                      : omnibox::ToolMode::TOOL_MODE_IMAGE_GEN;
    case ComposeboxMode::kAIM:
    case ComposeboxMode::kRegularSearch:
      return omnibox::ToolMode::TOOL_MODE_UNSPECIFIED;
  }
}

// Returns the server strings object from a given input state.
ComposeboxStrings* ServerStringsFromInputState(
    const contextual_search::InputState& input_state) {
  std::unordered_map<ComposeboxMode, ComposeboxStringBundle*> tool_mapping;
  for (const omnibox::ToolConfig& tool_config : input_state.tool_configs) {
    NSString* menuLabel = base::SysUTF8ToNSString(tool_config.menu_label());
    NSString* chipLabel = base::SysUTF8ToNSString(tool_config.chip_label());
    NSString* hintText = base::SysUTF8ToNSString(tool_config.hint_text());
    std::optional<ComposeboxMode> mode = ModeForToolMode(tool_config.tool());
    if (mode) {
      tool_mapping[*mode] =
          [[ComposeboxStringBundle alloc] initWithMenuLabel:menuLabel
                                                  chipLabel:chipLabel
                                                   hintText:hintText];
    }
  }

  std::unordered_map<ComposeboxModelOption, ComposeboxStringBundle*>
      model_mapping;
  for (const omnibox::ModelConfig& model_config : input_state.model_configs) {
    NSString* menuLabel = base::SysUTF8ToNSString(model_config.menu_label());
    NSString* hintText = base::SysUTF8ToNSString(model_config.hint_text());
    model_mapping[ModelOptionForModelMode(model_config.model())] =
        [[ComposeboxStringBundle alloc] initWithMenuLabel:menuLabel
                                                chipLabel:nil
                                                 hintText:hintText];
  }

  NSString* modelSectionHeader = @"";
  NSString* toolsSectionHeader = @"";

  if (input_state.model_section_config) {
    modelSectionHeader =
        base::SysUTF8ToNSString(input_state.model_section_config->header());
  }

  if (input_state.tools_section_config) {
    toolsSectionHeader =
        base::SysUTF8ToNSString(input_state.tools_section_config->header());
  }

  return [[ComposeboxStrings alloc] initWithToolMapping:tool_mapping
                                           modelMapping:model_mapping
                                     modelSectionHeader:modelSectionHeader
                                     toolsSectionHeader:toolsSectionHeader];
}

}  // namespace

@interface ComposeboxInputStateManager () <ComposeboxModeObserver,
                                           SearchEngineObserving>
@end

@implementation ComposeboxInputStateManager {
  // The entrypoint from which the composebox was opened.
  ComposeboxEntrypoint _entrypoint;
  // The mode holder to read the active mode.
  ComposeboxModeHolder* _modeHolder;
  // The active model. This is different from omnibox::ModelMode that doesn't
  // support unselected state.
  ComposeboxModelOption _activeModel;

  // The web state list of the browser.
  raw_ptr<WebStateList> _webStateList;
  // Pref service for user preferences.
  raw_ptr<PrefService> _prefService;
  // Service to check AIM eligibility.
  raw_ptr<AimEligibilityService> _aimEligibilityService;
  // Identity manager for checking account status.
  raw_ptr<signin::IdentityManager> _identityManager;
  // Template URL service for checking default search engine.
  raw_ptr<TemplateURLService> _templateURLService;
  // Whether the current session is incognito.
  BOOL _isIncognito;
  // Handle to the current contextual search session, used for metrics recording
  // and state initialization.
  base::WeakPtr<contextual_search::ContextualSearchSessionHandle>
      _sessionHandle;

  // The underlying C++ model that manages the state.
  std::unique_ptr<contextual_search::InputStateModel> _inputStateModel;
  // The cached current input state.
  std::optional<contextual_search::InputState> _inputState;
  // Subscription for state updates from the model.
  base::CallbackListSubscription _inputStateSubscription;
  // Cached server strings.
  ComposeboxStrings* _cachedStrings;
  // Observer for the TemplateURLService.
  std::unique_ptr<SearchEngineObserverBridge> _searchEngineObserver;
  // Subscription for eligibility changes.
  base::CallbackListSubscription _aimEligibilitySubscription;
  // Cached DSE status.
  BOOL _isDSEGoogle;
}

#pragma mark - Public

- (instancetype)
     initWithWebStateList:(WebStateList*)webStateList
               modeHolder:(ComposeboxModeHolder*)modeHolder
              prefService:(PrefService*)prefService
    aimEligibilityService:(AimEligibilityService*)aimEligibilityService
          identityManager:(signin::IdentityManager*)identityManager
       templateURLService:(TemplateURLService*)templateURLService
            sessionHandle:
                (contextual_search::ContextualSearchSessionHandle*)sessionHandle
               entrypoint:(ComposeboxEntrypoint)entrypoint
              isIncognito:(BOOL)isIncognito {
  self = [super init];
  if (self) {
    // Initialize with local fallback strings. These will be overwritten
    // when server-side strings become available via the input state model.
    _cachedStrings = [ComposeboxStrings localFallbackStrings];
    _webStateList = webStateList;
    _modeHolder = modeHolder;
    [_modeHolder addObserver:self];
    _prefService = prefService;
    _aimEligibilityService = aimEligibilityService;
    _identityManager = identityManager;
    _templateURLService = templateURLService;
    // sessionHandle can be nil only in tests.
    if (sessionHandle) {
      _sessionHandle = sessionHandle->AsWeakPtr();
    }
    _entrypoint = entrypoint;
    _isIncognito = isIncognito;
    _activeModel = ComposeboxModelOption::kNone;

    if (_templateURLService) {
      _isDSEGoogle = search::DefaultSearchProviderIsGoogle(_templateURLService);
      _searchEngineObserver = std::make_unique<SearchEngineObserverBridge>(
          self, _templateURLService);
    }

    __weak __typeof(self) weakSelf = self;
    _aimEligibilitySubscription =
        _aimEligibilityService->RegisterEligibilityChangedCallback(
            base::BindRepeating(^{
              [weakSelf onAimEligibilityChanged];
            }));

    [self updateSearchboxConfig];
  }
  return self;
}

- (void)disconnect {
  self.items = nil;
  _inputStateSubscription = {};
  _inputStateModel.reset();
  _inputState.reset();
  _cachedStrings = nil;
  _webStateList = nullptr;
  _prefService = nullptr;
  _aimEligibilityService = nullptr;
  _identityManager = nullptr;
  _templateURLService = nullptr;
  _sessionHandle.reset();
  [_modeHolder removeObserver:self];
  _modeHolder = nil;
  _searchEngineObserver.reset();
  _aimEligibilitySubscription = {};
}

- (ComposeboxModelOption)activeModel {
  if ([self activeMode] == ComposeboxMode::kRegularSearch ||
      !_inputStateModel) {
    return ComposeboxModelOption::kNone;
  }
  return ModelOptionForModelMode(
      _inputStateModel->GetInputState().active_model);
}

- (void)setActiveModel:(ComposeboxModelOption)modelOption
    explicitUserAction:(BOOL)explicitUserAction {
  if (!_inputStateModel) {
    return;
  }

  if (![self canSelectModel:modelOption]) {
    return;
  }

  if (modelOption == _activeModel) {
    return;
  }
  _activeModel = modelOption;

  [self setActiveModelInInputState:modelOption
                explicitUserAction:explicitUserAction];

  [self.delegate inputStateManagerDidUpdateUIState:self];

  // Update mode on model change.

  // Only when the user explicitly picked the advanced model in regular mode
  // do the switch to AIM.
  BOOL switchToAIM = explicitUserAction && _modeHolder.isRegularSearch;
  if (switchToAIM) {
    _modeHolder.mode = ComposeboxMode::kAIM;
    return;
  }
}

- (std::map<std::string, std::string>)additionalQueryParams {
  if (_inputStateModel) {
    return _inputStateModel->GetAdditionalQueryParams();
  }
  return {};
}

- (void)onContextChanged {
  if (_inputStateModel) {
    _inputStateModel->OnContextChanged();
  }
}

- (void)onItemsUpdated {
  // Tool Mode has a different value depending on `items.hasImage`. Update if
  // needed.
  ComposeboxMode activeMode = [self activeMode];
  if (activeMode == ComposeboxMode::kImageGeneration) {
    [self setActiveToolInInputState:ToolModeForComposeboxMode(
                                        activeMode, self.items.hasImage)];
  }
}

- (void)recordInputStateOnSubmission {
  contextual_search::ContextualSearchMetricsRecorder* recorder =
      _sessionHandle ? _sessionHandle->GetMetricsRecorder() : nullptr;
  if (!recorder || !_inputStateModel || !_inputState.has_value()) {
    return;
  }
  std::vector<omnibox::InputType> active_input_types =
      contextual_search::InputStateModel::GetCurrentInputTypes(
          _sessionHandle.get());
  recorder->RecordModesOnSubmission(
      _inputState->active_tool, _inputState->active_model, active_input_types);
}

- (void)
    recordInputStateOnSessionEndWithNavigation:(BOOL)inNavigation
                                     mimeTypes:
                                         (const std::vector<lens::MimeType>&)
                                             mimeTypes {
  contextual_search::ContextualSearchMetricsRecorder* recorder =
      _sessionHandle ? _sessionHandle->GetMetricsRecorder() : nullptr;
  if (!recorder) {
    return;
  }
  recorder->RecordFileTypesOnSessionEnd(mimeTypes, inNavigation);

  if (_inputStateModel && _inputState.has_value()) {
    recorder->RecordActiveModesOnSessionEnd(
        _inputState->active_tool, _inputState->active_model, inNavigation);
  }

  recorder->RecordNavigationResult(inNavigation);
}

- (BOOL)isEligibleToAIM {
  if (IsAssistantContainerDebugEnabled()) {
    return YES;
  }
  if (experimental_flags::ShouldForceDisableComposeboxAIM()) {
    return NO;
  }
  if (IsComposeboxAIMDisabled()) {
    return NO;
  }
  if (!_aimEligibilityService) {
    return NO;
  }
  return _aimEligibilityService->IsFuseboxEligible();
}

- (BOOL)isDSEGoogle {
  if (!_templateURLService) {
    return NO;
  }
  return search::DefaultSearchProviderIsGoogle(_templateURLService);
}

- (BOOL)canSelectTool:(ComposeboxMode)mode {
  return [self isToolAllowed:mode] && ![self isToolDisabled:mode];
}

- (BOOL)canSelectModel:(ComposeboxModelOption)modelOption {
  return
      [self isModelAllowed:modelOption] && ![self isModelDisabled:modelOption];
}

- (BOOL)canAddMoreAttachments {
  return [self remainingAttachmentCapacity] > 0;
}

- (NSUInteger)remainingAttachmentCapacity {
  NSUInteger availableSlots = 0;
  NSUInteger totalLimit = [self totalAttachmentLimit];
  NSArray<ComposeboxInputItem*>* items = self.items.containedItems;
  if (items.count < totalLimit) {
    availableSlots = totalLimit - items.count;
  }

  switch ([self activeMode]) {
    case ComposeboxMode::kImageGeneration: {
      // For ImageGeneration, allow 1 image if no images are present, otherwise
      // 0.
      return self.items.hasImage
                 ? 0
                 : MIN(availableSlots, kAttachmentLimitForImageGeneration);
    }
    default:
      return availableSlots;
  }
}

- (NSUInteger)remainingNumberOfImagesAllowed {
  NSUInteger remainingAttachmentCapacity = [self remainingAttachmentCapacity];
  if (!EnableComposeboxServerSideState()) {
    return remainingAttachmentCapacity;
  }
  if (!_inputState.has_value()) {
    return remainingAttachmentCapacity;
  }
  auto limits = _inputState->max_inputs_by_type;
  auto type = omnibox::InputType::INPUT_TYPE_LENS_IMAGE;
  if (limits.count(type)) {
    int serverLimit = limits[type];
    NSUInteger remainingSlots =
        std::max(0, serverLimit - static_cast<int>(self.items.imagesCount));
    return MIN(remainingSlots, remainingAttachmentCapacity);
  }
  return remainingAttachmentCapacity;
}

- (BOOL)canAttachActiveTabWithAttachedWebStateIDs:
    (const std::set<web::WebStateID>&)attachedWebStateIDs {
  web::WebState* webState = _webStateList->GetActiveWebState();
  if (!webState) {
    return NO;
  }

  BOOL isNTP = IsUrlNtp(webState->GetVisibleURL());
  BOOL alreadyProcessed =
      attachedWebStateIDs.contains(webState->GetUniqueIdentifier());

  return !isNTP && !alreadyProcessed && [self isContentSharingEnabled];
}

- (NSUInteger)maxTabAttachmentCount {
  NSUInteger remainingAttachmentCapacity = [self remainingAttachmentCapacity];
  NSUInteger capacityForTabs =
      remainingAttachmentCapacity + self.items.tabsCount;

  if (!EnableComposeboxServerSideState()) {
    return capacityForTabs;
  }
  if (!_inputState.has_value()) {
    return capacityForTabs;
  }
  auto limits = _inputState->max_inputs_by_type;
  auto type = omnibox::InputType::INPUT_TYPE_BROWSER_TAB;
  if (limits.count(type)) {
    NSUInteger serverLimit = static_cast<NSUInteger>(limits[type]);
    return MIN(serverLimit, capacityForTabs);
  }

  return capacityForTabs;
}

- (ComposeboxUIInputState*)
    computeUIInputStateWithFavicon:(UIImage*)currentTabFavicon
               attachedWebStateIDs:
                   (const std::set<web::WebStateID>&)attachedWebStateIDs {
  ComposeboxUIInputState* state = [[ComposeboxUIInputState alloc] init];

  state.currentTabFavicon = currentTabFavicon;
  state.remainingAttachmentCapacity = [self remainingAttachmentCapacity];
  state.remainingNumberOfImagesAllowed = [self remainingNumberOfImagesAllowed];
  state.maxTabAttachmentCount = [self maxTabAttachmentCount];
  state.allowModelPicker = ShowComposeboxAdditionalAdvancedTools();

  state.activeTool = [self activeMode];
  state.activeModel = self.activeModel;

  state.strings = _cachedStrings;

  // Populate allowed/disabled attachments
  std::unordered_set<ComposeboxAttachmentOption> allowedAttachments;
  std::unordered_set<ComposeboxAttachmentOption> disabledAttachments;

  for (ComposeboxAttachmentOption attachmentOption :
       ComposeboxAttachmentOptionSet::All()) {
    if (attachmentOption == ComposeboxAttachmentOption::kCurrentTab) {
      if ([self
              canAttachActiveTabWithAttachedWebStateIDs:attachedWebStateIDs] &&
          [self isAttachmentAllowed:attachmentOption]) {
        allowedAttachments.insert(attachmentOption);
      }
    } else {
      if ([self isAttachmentAllowed:attachmentOption]) {
        allowedAttachments.insert(attachmentOption);
      }
    }
    if ([self isAttachmentDisabled:attachmentOption]) {
      disabledAttachments.insert(attachmentOption);
    }
  }

  state.allowedAttachments = allowedAttachments;
  state.disabledAttachments = disabledAttachments;

  // Populate allowed/disabled tools
  std::unordered_set<ComposeboxMode> allowedTools;
  std::unordered_set<ComposeboxMode> disabledTools;

  // The default/implicit tool is not displayed as an option.
  ComposeboxMode defaultMode = [self defaultTool];
  for (ComposeboxMode mode : ComposeboxModeSet::All()) {
    if (mode == defaultMode) {
      continue;
    }
    if ([self isToolAllowed:mode]) {
      allowedTools.insert(mode);
    }
    if ([self isToolDisabled:mode]) {
      disabledTools.insert(mode);
    }
  }

  state.allowedTools = allowedTools;
  state.disabledTools = disabledTools;

  std::unordered_set<ComposeboxModelOption> allowedModels;
  std::unordered_set<ComposeboxModelOption> disabledModels;

  for (ComposeboxModelOption modelOption : ComposeboxModelOptionSet::All()) {
    // Skip ComposeboxModelOption::kNone in the available models.
    if (modelOption == ComposeboxModelOption::kNone) {
      continue;
    }
    if ([self isModelAllowed:modelOption]) {
      allowedModels.insert(modelOption);
    }
    if ([self isModelDisabled:modelOption]) {
      disabledModels.insert(modelOption);
    }
  }

  state.allowedModels = allowedModels;
  state.disabledModels = disabledModels;

  return state;
}

#pragma mark - Private

#pragma mark - SearchEngineObserving

- (void)searchEngineChanged {
  BOOL isDSEGoogle = search::DefaultSearchProviderIsGoogle(_templateURLService);
  if (isDSEGoogle != _isDSEGoogle) {
    _isDSEGoogle = isDSEGoogle;
    [self.delegate inputStateManagerDidUpdateUIState:self];
  }
}

- (void)templateURLServiceShuttingDown:(TemplateURLService*)urlService {
  _searchEngineObserver.reset();
  _templateURLService = nullptr;
}

#pragma mark - ComposeboxModeObserver

- (void)composeboxModeDidChange:(ComposeboxMode)mode {
  if (![self canSelectTool:mode]) {
    _modeHolder.mode = [self defaultTool];
    // Return early as setting mode triggers a new call.
    return;
  }

  // Set active tool mode triggers input state update.
  omnibox::ToolMode toolMode =
      ToolModeForComposeboxMode(mode, self.items.hasImage);
  [self setActiveToolInInputState:toolMode];

  [self updateModelOnModeChange];

  // Invalidate items/attachments for the current mode.
  NSArray<ComposeboxInputItem*>* invalidatedItems =
      [self invalidItemsInActiveMode];

  [self.delegate inputStateManager:self
                     didChangeMode:mode
            invalidatedAttachments:invalidatedItems];
  [self.delegate inputStateManagerDidUpdateUIState:self];
}

/// Updates the active model on mode change.
- (void)updateModelOnModeChange {
  if ([_modeHolder isRegularSearch]) {
    // Reset model to kNone when switching to regular search.
    [self setActiveModel:ComposeboxModelOption::kNone explicitUserAction:NO];
    return;
  } else if (_activeModel == ComposeboxModelOption::kNone) {
    // In AI tools, set the default model when none is selected.
    [self setActiveModel:[self defaultModel] explicitUserAction:NO];
  }
}

// Returns items (attachments) that are invalid (incompatible with the current
// mode) and pending deletion.
- (NSArray<ComposeboxInputItem*>*)invalidItemsInActiveMode {
  NSMutableArray<ComposeboxInputItem*>* invalidatedItems =
      [NSMutableArray array];

  switch ([self activeMode]) {
    case ComposeboxMode::kImageGeneration: {
      // Image generation only accepts one image, invalidate the other items.
      ComposeboxInputItem* imageToKeep = nil;
      for (ComposeboxInputItem* item in self.items.containedItems) {
        BOOL hasImageType =
            item.type == ComposeboxInputItemType::kComposeboxInputItemTypeImage;
        BOOL shouldReuseItem = hasImageType && !imageToKeep;
        if (shouldReuseItem) {
          imageToKeep = item;
          continue;
        }

        [invalidatedItems addObject:item];
      }
      break;
    }
    case ComposeboxMode::kRegularSearch:
      // Invalidate all items.
      if (self.items.containedItems) {
        [invalidatedItems addObjectsFromArray:self.items.containedItems];
      }
      break;
    default:
      break;
  }

  return invalidatedItems;
}

#pragma mark AIMEligibilityService Observation

- (void)onAimEligibilityChanged {
  [self updateSearchboxConfig];
}

- (void)updateSearchboxConfig {
  if (!_aimEligibilityService) {
    return;
  }

  const omnibox::SearchboxConfig* searchboxConfig =
      _aimEligibilityService->GetSearchboxConfig();

  if (!_sessionHandle || !searchboxConfig) {
    return;
  }

  BOOL has_primary_account =
      _identityManager &&
      _identityManager->HasPrimaryAccount(signin::ConsentLevel::kSignin);

  _inputStateModel = std::make_unique<contextual_search::InputStateModel>(
      *_sessionHandle, *searchboxConfig, GURL(), _isIncognito,
      has_primary_account);

  [self startInputStateObservation];
  // `Initialize` is synchronous and immediately notifies observers, updating
  // the state.
  _inputStateModel->Initialize();

  // iOS doesn't rely on the active hint text from `_inputState`, strings only
  // changes when `searchboxConfig` is updated.
  _cachedStrings =
      ServerStringsFromInputState(_inputStateModel->GetInputState());

  [self.delegate inputStateManagerDidUpdateUIState:self];
}

#pragma mark Observation

// Starts observing state changes from the model.
- (void)startInputStateObservation {
  if (!_inputStateModel) {
    return;
  }
  __weak __typeof(self) weakSelf = self;
  _inputStateSubscription = _inputStateModel->subscribe(
      base::BindRepeating(^(const contextual_search::InputState& inputState) {
        [weakSelf didUpdateInputState:inputState];
      }));
}

// Notifies the delegate of state updates.
- (void)didUpdateInputState:(const contextual_search::InputState&)inputState {
  _inputState = inputState;

  [self reconcileToolModeWithInputState];
  [self reconcileModelWithInputState];

  [self.delegate inputStateManagerDidUpdateUIState:self];
}

// Returns YES if the given mode matches the tool mode with leeway.
- (BOOL)isMode:(ComposeboxMode)mode matchingTool:(omnibox::ToolMode)tool {
  switch (mode) {
    case ComposeboxMode::kRegularSearch:
    case ComposeboxMode::kAIM:
      // Both RegularSearch and AIM map to TOOL_MODE_UNSPECIFIED.
      return tool == omnibox::ToolMode::TOOL_MODE_UNSPECIFIED;
    default:
      return ToolModeForComposeboxMode(mode, self.items.hasImage) == tool;
  }
}

// Returns YES if the given model option matches the model mode.
- (BOOL)isModelOption:(ComposeboxModelOption)option
    matchingModelMode:(omnibox::ModelMode)modelMode {
  if ([self activeMode] == ComposeboxMode::kRegularSearch) {
    // Any model is valid in regular search.
    return YES;
  }
  if (option == ComposeboxModelOption::kNone) {
    // kNone is not valid in AI mode.
    return NO;
  }
  return option == ModelOptionForModelMode(modelMode);
}

// Synchronizes the local active tool mode with the server-side input state,
// ensuring the local selection is valid according to the current server
// configuration.
- (void)reconcileToolModeWithInputState {
  if (!_inputState.has_value()) {
    return;
  }
  ComposeboxMode currentMode = [self activeMode];
  // Local and inputStateModel matches.
  if ([self isMode:currentMode matchingTool:_inputState->active_tool]) {
    return;
  }

  omnibox::ToolMode targetToolMode =
      ToolModeForComposeboxMode(currentMode, self.items.hasImage);

  BOOL localOptionValid = [self canSelectTool:currentMode];
  if (localOptionValid) {
    // Local is valid, update inputStateModel.
    [self setActiveToolInInputState:targetToolMode];
  } else {
    // Local is not valid, reset to default.
    _modeHolder.mode = [self defaultTool];
  }
}

// Synchronizes the local active model with the server-side input state,
// ensuring the local selection is valid according to the current server
// configuration.
- (void)reconcileModelWithInputState {
  if (!_inputState.has_value()) {
    return;
  }
  // Local and inputStateModel matches.
  if ([self isModelOption:_activeModel
          matchingModelMode:_inputState->active_model]) {
    return;
  }

  if ([self canSelectModel:_activeModel]) {
    // Local is valid, update inputStateModel.
    [self setActiveModelInInputState:_activeModel explicitUserAction:NO];
  } else {
    // Local is not valid, reset to default.
    [self setActiveModel:[self defaultModel] explicitUserAction:NO];
  }
}

#pragma mark - State Availability

// Whether the given attachment option is allowed.
- (BOOL)isAttachmentAllowed:(ComposeboxAttachmentOption)attachmentOption {
  if (![self attachmentsAvailable]) {
    return NO;
  }

  if (EnableComposeboxServerSideState()) {
    return [self isAttachmentAllowedByServer:attachmentOption];
  } else {
    return [self isAttachmentAllowedLocally:attachmentOption];
  }
}

// Whether the given attachment option is disabled.
- (BOOL)isAttachmentDisabled:(ComposeboxAttachmentOption)attachmentOption {
  if (![self canAddMoreAttachments]) {
    return YES;
  }

  if (EnableComposeboxServerSideState()) {
    return [self isAttachmentDisabledByServer:attachmentOption];
  } else {
    return [self isAttachmentDisabledLocally:attachmentOption];
  }
}

// Whether the given tool mode is allowed.
- (BOOL)isToolAllowed:(ComposeboxMode)mode {
  if (mode == [self defaultTool]) {
    return YES;
  }

  // All tools are gated by AI omnibox eligibility.
  if (![self isEligibleToAIM]) {
    return NO;
  }

  // Global checks first
  switch (mode) {
    case ComposeboxMode::kImageGeneration:
      if (experimental_flags::ShouldForceDisableComposeboxCreateImages()) {
        return NO;
      }
      break;
    case ComposeboxMode::kCanvas:
      if (!ShowComposeboxAdditionalAdvancedTools() ||
          experimental_flags::ShouldForceDisableComposeboxCanvas()) {
        return NO;
      }
      break;
    case ComposeboxMode::kDeepSearch:
      if (!ShowDeepSearchTool() ||
          experimental_flags::ShouldForceDisableComposeboxDeepSearch()) {
        return NO;
      }
      break;
    case ComposeboxMode::kAIM:
      break;
    case ComposeboxMode::kRegularSearch:
      if (_entrypoint == ComposeboxEntrypoint::kCobrowse) {
        return NO;
      }
      break;
  }

  if (EnableComposeboxServerSideState()) {
    return [self isToolAllowedByServer:mode];
  } else {
    return [self isToolAllowedLocally:mode];
  }
}

// Whether the given tool mode is disabled.
- (BOOL)isToolDisabled:(ComposeboxMode)mode {
  if ([self activeMode] == mode) {
    // Allows the user to exit the active mode.
    return NO;
  }
  if (EnableComposeboxServerSideState()) {
    return [self isToolDisabledByServer:mode];
  } else {
    return [self isToolDisabledLocally:mode];
  }
}

// Whether the given model option is allowed.
- (BOOL)isModelAllowed:(ComposeboxModelOption)modelOption {
  // None is the fallback/default option that is always available.
  if (modelOption == ComposeboxModelOption::kNone) {
    return YES;
  }

  // All models are gated by AI omnibox eligibility.
  if (![self isEligibleToAIM]) {
    return NO;
  }

  // Models are only available from server state.
  if (!ShowComposeboxAdditionalAdvancedTools() ||
      !EnableComposeboxServerSideState()) {
    return NO;
  }

  if (!_inputState.has_value()) {
    return NO;
  }

  omnibox::ModelMode modelMode =
      ModelModeForModelOption(modelOption, _inputState.value());
  return std::ranges::contains(_inputState->allowed_models, modelMode);
}

// Whether the given model option is disabled.
- (BOOL)isModelDisabled:(ComposeboxModelOption)modelOption {
  // Models are only available from server state.
  if (!ShowComposeboxAdditionalAdvancedTools() ||
      !EnableComposeboxServerSideState()) {
    return NO;
  }

  if (!_inputState.has_value()) {
    return NO;
  }

  omnibox::ModelMode modelMode =
      ModelModeForModelOption(modelOption, _inputState.value());
  return std::ranges::contains(_inputState->disabled_models, modelMode);
}

#pragma mark - InputState Helpers

// Returns the current active mode from the mode holder.
- (ComposeboxMode)activeMode {
  return _modeHolder.mode;
}

/// Sets the active model in input state model.
- (void)setActiveModelInInputState:(ComposeboxModelOption)modelOption
                explicitUserAction:(BOOL)explicitUserAction {
  if (!_inputState.has_value()) {
    return;
  }
  // Set the model in input state.
  omnibox::ModelMode requestedModelMode =
      ModelModeForModelOption(modelOption, _inputState.value());

  if (_inputState->active_model == requestedModelMode) {
    return;
  }

  _inputStateModel->setActiveModel(requestedModelMode);

  contextual_search::ContextualSearchMetricsRecorder* recorder =
      _sessionHandle ? _sessionHandle->GetMetricsRecorder() : nullptr;
  if (explicitUserAction && recorder) {
    recorder->RecordModelMode(requestedModelMode);
  }
}

/// Sets the `activeTool` mode in the input state model.
- (void)setActiveToolInInputState:(omnibox::ToolMode)activeTool {
  if (!_inputStateModel) {
    return;
  }

  if (_inputStateModel->GetInputState().active_tool == activeTool) {
    return;
  }

  contextual_search::ContextualSearchMetricsRecorder* recorder =
      _sessionHandle ? _sessionHandle->GetMetricsRecorder() : nullptr;
  if (recorder) {
    recorder->RecordToolMode(activeTool);
  }

  _inputStateModel->setActiveTool(activeTool);
}

#pragma mark - Eligibility & Availability

/// Returns the default model for the active mode.
- (ComposeboxModelOption)defaultModel {
  if ([self activeMode] == ComposeboxMode::kRegularSearch) {
    return ComposeboxModelOption::kNone;
  }
  if (!_inputState.has_value()) {
    return ComposeboxModelOption::kNone;
  }
  return ModelOptionForModelMode(_inputState->GetDefaultModel());
}

/// Returns the default mode/tool that is implicit in the context. The tool is
/// not displayed in the input menu.
- (ComposeboxMode)defaultTool {
  if (_entrypoint == ComposeboxEntrypoint::kCobrowse) {
    return ComposeboxMode::kAIM;
  } else {
    return ComposeboxMode::kRegularSearch;
  }
}

/// Whether the user is eligible to share content (enterprise policy).
- (BOOL)isContentSharingEnabled {
  return _prefService && _sessionHandle &&
         _sessionHandle->CheckSearchContentSharingSettings(_prefService);
}

/// Whether the user is eligible to upload PDFs.
- (BOOL)isEligibleToUploadPdf {
  if (experimental_flags::ShouldForceDisableComposeboxPdfUpload()) {
    return NO;
  }
  if (!_aimEligibilityService || ![self isContentSharingEnabled]) {
    return NO;
  }
  return _aimEligibilityService->IsPdfUploadEligible();
}

/// Whether any attachments are available based on user eligibility.
- (BOOL)attachmentsAvailable {
  if (![self isContentSharingEnabled]) {
    return NO;
  }

  BOOL canSearchWithAI = [self isEligibleToAIM];
  BOOL canCreateImage = [self isToolAllowed:ComposeboxMode::kImageGeneration];
  BOOL canUseCanvas = [self isToolAllowed:ComposeboxMode::kCanvas];
  BOOL canUseDeepSearch = [self isToolAllowed:ComposeboxMode::kDeepSearch];
  return canUseCanvas || canCreateImage || canUseDeepSearch || canSearchWithAI;
}

#pragma mark - Attachment Rules

// The absolute value for the maximum number of attachments available.
- (NSUInteger)totalAttachmentLimit {
  if (!EnableComposeboxServerSideState() || !_inputState.has_value()) {
    return kAttachmentLimit;
  }
  return _inputState->max_total_inputs;
}

#pragma mark - Helpers

/// Whether the given attachment option is allowed by the server.
- (BOOL)isAttachmentAllowedByServer:
    (ComposeboxAttachmentOption)attachmentOption {
  if (!_inputState.has_value()) {
    return NO;
  }
  using enum ComposeboxAttachmentOption;
  switch (attachmentOption) {
    case kCurrentTab:
    case kTab:
      return std::ranges::contains(_inputState->allowed_input_types,
                                   omnibox::INPUT_TYPE_BROWSER_TAB);
    case kFile:
      return std::ranges::contains(_inputState->allowed_input_types,
                                   omnibox::INPUT_TYPE_LENS_FILE);
    case kGallery:
    case kCamera:
      return std::ranges::contains(_inputState->allowed_input_types,
                                   omnibox::INPUT_TYPE_LENS_IMAGE);
  }
}

/// Whether the given attachment option is allowed by local rules.
- (BOOL)isAttachmentAllowedLocally:
    (ComposeboxAttachmentOption)attachmentOption {
  using enum ComposeboxAttachmentOption;
  switch (attachmentOption) {
    case kCurrentTab:
    case kTab:
      return YES;
    case kFile:
      return [self isEligibleToUploadPdf];
    case kGallery:
    case kCamera:
      return YES;
  }
}

/// Whether the given attachment option is disabled by the server.
- (BOOL)isAttachmentDisabledByServer:
    (ComposeboxAttachmentOption)attachmentOption {
  if (!_inputState.has_value()) {
    return NO;
  }
  using enum ComposeboxAttachmentOption;
  switch (attachmentOption) {
    case kCurrentTab:
    case kTab:
      return std::ranges::contains(_inputState->disabled_input_types,
                                   omnibox::INPUT_TYPE_BROWSER_TAB);
    case kFile:
      return std::ranges::contains(_inputState->disabled_input_types,
                                   omnibox::INPUT_TYPE_LENS_FILE);
    case kGallery:
    case kCamera:
      return std::ranges::contains(_inputState->disabled_input_types,
                                   omnibox::INPUT_TYPE_LENS_IMAGE);
  }
}

/// Whether the given attachment option is disabled by local rules.
- (BOOL)isAttachmentDisabledLocally:
    (ComposeboxAttachmentOption)attachmentOption {
  BOOL isImageCreationMode =
      [self activeMode] == ComposeboxMode::kImageGeneration;
  using enum ComposeboxAttachmentOption;
  switch (attachmentOption) {
    case kCurrentTab:
    case kTab:
    case kFile:
      return isImageCreationMode;
    case kGallery:
    case kCamera:
      return NO;
  }
}

/// Whether the given tool mode is allowed by the server.
- (BOOL)isToolAllowedByServer:(ComposeboxMode)mode {
  if (!_inputState.has_value()) {
    return NO;
  }
  using enum ComposeboxMode;
  switch (mode) {
    case kImageGeneration:
      return std::ranges::contains(_inputState->allowed_tools,
                                   omnibox::ToolMode::TOOL_MODE_IMAGE_GEN);
    case kCanvas:
      return std::ranges::contains(_inputState->allowed_tools,
                                   omnibox::ToolMode::TOOL_MODE_CANVAS);
    case kDeepSearch:
      return std::ranges::contains(_inputState->allowed_tools,
                                   omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH);
    case kAIM:
      return [self isEligibleToAIM];
    case kRegularSearch:
      return YES;
  }
}

/// Whether the given tool mode is allowed by local rules.
- (BOOL)isToolAllowedLocally:(ComposeboxMode)mode {
  using enum ComposeboxMode;
  switch (mode) {
    case kImageGeneration:
      return _aimEligibilityService
                 ? _aimEligibilityService->IsCreateImagesEligible()
                 : NO;
    case kCanvas:
      return _aimEligibilityService ? _aimEligibilityService->IsCanvasEligible()
                                    : NO;
    case kDeepSearch:
      return _aimEligibilityService
                 ? _aimEligibilityService->IsDeepSearchEligible()
                 : NO;
    case kAIM:
      return [self isEligibleToAIM];
    case kRegularSearch:
      return YES;
  }
}

/// Whether the given tool mode is disabled by the server.
- (BOOL)isToolDisabledByServer:(ComposeboxMode)mode {
  if (!_inputState.has_value()) {
    return NO;
  }
  using enum ComposeboxMode;
  switch (mode) {
    case kImageGeneration:
      return std::ranges::contains(_inputState->disabled_tools,
                                   omnibox::ToolMode::TOOL_MODE_IMAGE_GEN) ||
             std::ranges::contains(
                 _inputState->disabled_tools,
                 omnibox::ToolMode::TOOL_MODE_IMAGE_GEN_UPLOAD);
    case kCanvas:
      return std::ranges::contains(_inputState->disabled_tools,
                                   omnibox::ToolMode::TOOL_MODE_CANVAS);
    case kDeepSearch:
      return std::ranges::contains(_inputState->disabled_tools,
                                   omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH);
    case kAIM:
    case kRegularSearch:
      return NO;
  }
}

/// Whether the given tool mode is disabled by local rules.
- (BOOL)isToolDisabledLocally:(ComposeboxMode)mode {
  return mode == ComposeboxMode:: kImageGeneration &&
         self.items.hasTabOrFile;
}

@end
