// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/coordinator/composebox_input_state_manager.h"

#import <algorithm>
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
#import "ios/chrome/browser/composebox/coordinator/composebox_constants.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_mode_holder.h"
#import "ios/chrome/browser/composebox/public/features.h"
#import "ios/chrome/browser/composebox/ui/composebox_input_item.h"
#import "ios/chrome/browser/composebox/ui/composebox_input_item_collection.h"
#import "ios/chrome/browser/composebox/ui/composebox_strings.h"
#import "ios/chrome/browser/composebox/ui/composebox_ui_input_state.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_id.h"

namespace {

// Returns the model option required by the given model mode.
ComposeboxModelOption ModelOptionForModelMode(omnibox::ModelMode model_mode) {
  using enum ComposeboxModelOption;
  switch (model_mode) {
    case omnibox::ModelMode::MODEL_MODE_GEMINI_PRO_AUTOROUTE:
      return ComposeboxModelOption::kAuto;
    case omnibox::ModelMode::MODEL_MODE_GEMINI_PRO:
      return ComposeboxModelOption::kThinking;
    case omnibox::ModelMode::MODEL_MODE_GEMINI_PRO_NO_GEN_UI:
      return ComposeboxModelOption::kThinkingNoGenUI;
    case omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR:
    case omnibox::ModelMode::MODEL_MODE_UNSPECIFIED:
      return ComposeboxModelOption::kRegular;
    default:
      base::debug::DumpWithoutCrashing();
      return ComposeboxModelOption::kRegular;
  }
}

// Returns the input plate control for the given tool mode.
std::optional<ComposeboxMode> ModeForToolMode(omnibox::ToolMode tool_mode) {
  switch (tool_mode) {
    case omnibox::ToolMode::TOOL_MODE_CANVAS:
      return ComposeboxMode::kCanvas;
    case omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH:
      return ComposeboxMode::kDeepSearch;
    case omnibox::ToolMode::TOOL_MODE_IMAGE_GEN:
    case omnibox::ToolMode::TOOL_MODE_IMAGE_GEN_UPLOAD:
    case omnibox::ToolMode::TOOL_MODE_IMAGE_GEN_SELFIE:
      return ComposeboxMode::kImageGeneration;
    case omnibox::ToolMode::TOOL_MODE_AIM:
      return ComposeboxMode::kAIM;
    default:
      return std::nullopt;
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

@implementation ComposeboxInputStateManager {
  // The entrypoint from which the composebox was opened.
  ComposeboxEntrypoint _entrypoint;
  // The mode holder to read the active mode.
  ComposeboxModeHolder* _modeHolder;

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
  contextual_search::InputState _inputState;
  // Subscription for state updates from the model.
  base::CallbackListSubscription _inputStateSubscription;
  // Cached server strings.
  ComposeboxStrings* _cachedStrings;
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
  }
  return self;
}

- (void)disconnect {
  self.items = nil;
  _inputStateSubscription = {};
  _inputStateModel.reset();
  _cachedStrings = nil;
  _webStateList = nullptr;
  _prefService = nullptr;
  _aimEligibilityService = nullptr;
  _identityManager = nullptr;
  _templateURLService = nullptr;
  _sessionHandle.reset();
  _modeHolder = nil;
}

- (void)setSearchboxConfig:(const omnibox::SearchboxConfig&)searchboxConfig {
  if (!_sessionHandle) {
    return;
  }

  std::optional<contextual_search::InputState> preselectionState;
  // Only preselect when there was already a input state model created.
  // Otherwise it's safe to assume it is the first time a searchbox config is
  // loaded.
  if (_inputStateModel) {
    preselectionState = _inputState;
  }

  BOOL has_primary_account =
      _identityManager &&
      _identityManager->HasPrimaryAccount(signin::ConsentLevel::kSignin);

  _inputStateModel = std::make_unique<contextual_search::InputStateModel>(
      *_sessionHandle, searchboxConfig, GURL(), _isIncognito,
      has_primary_account);

  [self startInputStateObservation];
  // `Initialize` is synchronous and immediately notifies observers, updating
  // the state.
  _inputStateModel->Initialize();

  if (preselectionState.has_value()) {
    // It is safe to apply preselection immediately because the state is already
    // updated.
    [self applyPreselection:*preselectionState
          forReferenceState:_inputStateModel->GetInputState()];
  }
}

- (omnibox::ToolMode)activeTool {
  if (_inputStateModel) {
    return _inputStateModel->GetInputState().active_tool;
  }
  return omnibox::TOOL_MODE_UNSPECIFIED;
}

- (ComposeboxModelOption)activeModel {
  if ([self activeMode] == ComposeboxMode::kRegularSearch ||
      !_inputStateModel) {
    return ComposeboxModelOption::kNone;
  }
  return ModelOptionForModelMode(
      _inputStateModel->GetInputState().active_model);
}

- (void)setActiveTool:(omnibox::ToolMode)activeTool {
  if (!_inputStateModel) {
    return;
  }

  if (_inputStateModel->GetInputState().active_tool != activeTool) {
    contextual_search::ContextualSearchMetricsRecorder* recorder =
        _sessionHandle ? _sessionHandle->GetMetricsRecorder() : nullptr;
    if (recorder) {
      recorder->RecordToolMode(activeTool);
    }
  }
  _inputStateModel->setActiveTool(activeTool);
}

- (void)setActiveModel:(ComposeboxModelOption)modelOption
    explicitUserAction:(BOOL)explicitUserAction {
  if (!_inputStateModel) {
    return;
  }

  omnibox::ModelMode requestedModelMode =
      omnibox::ModelMode::MODEL_MODE_UNSPECIFIED;
  using enum ComposeboxModelOption;
  switch (modelOption) {
    case kNone:
      requestedModelMode = _inputState.GetDefaultModel();
      break;
    case kRegular:
      requestedModelMode = omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR;
      break;
    case kAuto:
      requestedModelMode = omnibox::ModelMode::MODEL_MODE_GEMINI_PRO_AUTOROUTE;
      break;
    case kThinking:
      requestedModelMode = omnibox::ModelMode::MODEL_MODE_GEMINI_PRO;
      break;
    case kThinkingNoGenUI:
      requestedModelMode = omnibox::ModelMode::MODEL_MODE_GEMINI_PRO_NO_GEN_UI;
      break;
  }

  if (_inputState.active_model == requestedModelMode) {
    return;
  }

  _inputStateModel->setActiveModel(requestedModelMode);

  contextual_search::ContextualSearchMetricsRecorder* recorder =
      _sessionHandle ? _sessionHandle->GetMetricsRecorder() : nullptr;
  if (explicitUserAction && recorder) {
    recorder->RecordModelMode(requestedModelMode);
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

- (void)recordInputStateOnSubmission {
  contextual_search::ContextualSearchMetricsRecorder* recorder =
      _sessionHandle ? _sessionHandle->GetMetricsRecorder() : nullptr;
  if (recorder && _inputStateModel) {
    std::vector<omnibox::InputType> active_input_types =
        contextual_search::InputStateModel::GetCurrentInputTypes(
            _sessionHandle.get());
    recorder->RecordModesOnSubmission(
        _inputState.active_tool, _inputState.active_model, active_input_types);
  }
}

- (void)
    recordInputStateOnSessionEndWithNavigation:(BOOL)inNavigation
                                     mimeTypes:
                                         (const std::vector<lens::MimeType>&)
                                             mimeTypes {
  contextual_search::ContextualSearchMetricsRecorder* recorder =
      _sessionHandle ? _sessionHandle->GetMetricsRecorder() : nullptr;
  if (recorder) {
    recorder->RecordFileTypesOnSessionEnd(mimeTypes, inNavigation);

    if (_inputStateModel) {
      recorder->RecordActiveModesOnSessionEnd(
          _inputStateModel->GetInputState().active_tool,
          _inputStateModel->GetInputState().active_model, inNavigation);
    }

    recorder->RecordNavigationResult(inNavigation);
  }
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
  if (EnableComposeboxServerSideState()) {
    auto limits = _inputState.max_inputs_by_type;
    auto type = omnibox::InputType::INPUT_TYPE_LENS_IMAGE;
    if (limits.count(type)) {
      int serverLimit = limits[type];
      NSUInteger remainingSlots =
          std::max(0, serverLimit - static_cast<int>(self.items.imagesCount));
      return MIN(remainingSlots, remainingAttachmentCapacity);
    }
  }
  return remainingAttachmentCapacity;
}

- (std::unordered_set<ComposeboxModelOption>)allowedModels {
  std::unordered_set<ComposeboxModelOption> allowed = {};
  if (!ShowComposeboxAdditionalAdvancedTools()) {
    return allowed;
  }
  for (auto modelType : _inputState.allowed_models) {
    allowed.insert(ModelOptionForModelMode(modelType));
  }

  return allowed;
}

- (std::unordered_set<ComposeboxModelOption>)disabledModels {
  std::unordered_set<ComposeboxModelOption> disabled = {};
  if (!ShowComposeboxAdditionalAdvancedTools()) {
    return disabled;
  }
  for (auto modelType : _inputState.disabled_models) {
    disabled.insert(ModelOptionForModelMode(modelType));
  }

  return disabled;
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

  if (EnableComposeboxServerSideState()) {
    auto limits = _inputState.max_inputs_by_type;
    auto type = omnibox::InputType::INPUT_TYPE_BROWSER_TAB;
    if (limits.count(type)) {
      NSUInteger serverLimit = static_cast<NSUInteger>(limits[type]);
      return MIN(serverLimit, capacityForTabs);
    }
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
  state.allowModelPicker = ShowComposeboxAdditionalAdvancedTools();

  state.activeTool = [self activeMode];
  state.activeModel = self.activeModel;

  state.strings = _cachedStrings;

  // Populate allowed/disabled attachments
  std::unordered_set<ComposeboxAttachmentOption> allowedAttachments;
  std::unordered_set<ComposeboxAttachmentOption> disabledAttachments;

  if ([self canAttachActiveTabWithAttachedWebStateIDs:attachedWebStateIDs] &&
      [self tabAttachmentAllowed]) {
    allowedAttachments.insert(ComposeboxAttachmentOption::kCurrentTab);
  }
  if ([self tabAttachmentAllowed]) {
    allowedAttachments.insert(ComposeboxAttachmentOption::kTab);
  }
  if ([self tabAttachmentDisabled]) {
    disabledAttachments.insert(ComposeboxAttachmentOption::kTab);
  }
  if ([self fileAttachmentAllowed]) {
    allowedAttachments.insert(ComposeboxAttachmentOption::kFile);
  }
  if ([self fileAttachmentDisabled]) {
    disabledAttachments.insert(ComposeboxAttachmentOption::kFile);
  }
  if ([self imageAttachmentAllowed]) {
    allowedAttachments.insert(ComposeboxAttachmentOption::kGallery);
    allowedAttachments.insert(ComposeboxAttachmentOption::kCamera);
  }
  if ([self imageAttachmentDisabled]) {
    disabledAttachments.insert(ComposeboxAttachmentOption::kGallery);
    disabledAttachments.insert(ComposeboxAttachmentOption::kCamera);
  }

  state.allowedAttachments = allowedAttachments;
  state.disabledAttachments = disabledAttachments;

  // Populate allowed/disabled tools
  std::unordered_set<ComposeboxMode> allowedTools;
  std::unordered_set<ComposeboxMode> disabledTools;

  if ([self imageToolAllowed]) {
    allowedTools.insert(ComposeboxMode::kImageGeneration);
  }
  if ([self imageToolDisabled]) {
    disabledTools.insert(ComposeboxMode::kImageGeneration);
  }
  if ([self canvasToolAllowed]) {
    allowedTools.insert(ComposeboxMode::kCanvas);
  }
  if ([self canvasToolDisabled]) {
    disabledTools.insert(ComposeboxMode::kCanvas);
  }
  if ([self deepSearchToolAllowed]) {
    allowedTools.insert(ComposeboxMode::kDeepSearch);
  }
  if ([self deepSearchToolDisabled]) {
    disabledTools.insert(ComposeboxMode::kDeepSearch);
  }
  if (_entrypoint != ComposeboxEntrypoint::kCobrowse) {
    allowedTools.insert(ComposeboxMode::kAIM);
  }

  state.allowedTools = allowedTools;
  state.disabledTools = disabledTools;

  state.allowedModels = [self allowedModels];
  state.disabledModels = [self disabledModels];

  return state;
}

#pragma mark - Private

#pragma mark Preselection

// Attempts to prepopulate the input state after an environment change.
// This is subject to restriction based on the reference state.
- (void)applyPreselection:
            (const contextual_search::InputState&)preselectionState
        forReferenceState:(const contextual_search::InputState&)inputState {
  bool canSelectModel = NO;
  if (!EnableComposeboxServerSideState()) {
    canSelectModel = YES;
  } else {
    bool model_allowed = std::find(inputState.allowed_models.begin(),
                                   inputState.allowed_models.end(),
                                   preselectionState.active_model) !=
                         inputState.allowed_models.end();
    bool model_disabled = std::find(inputState.disabled_models.begin(),
                                    inputState.disabled_models.end(),
                                    preselectionState.active_model) !=
                          inputState.disabled_models.end();
    canSelectModel = model_allowed && !model_disabled;
  }

  if (canSelectModel && _inputStateModel) {
    _inputStateModel->setActiveModel(preselectionState.active_model);
  }

  bool canSelectTool = NO;
  if (!EnableComposeboxServerSideState()) {
    canSelectTool = YES;
  } else {
    bool tool_allowed = std::find(inputState.allowed_tools.begin(),
                                  inputState.allowed_tools.end(),
                                  preselectionState.active_tool) !=
                        inputState.allowed_tools.end();
    bool tool_disabled = std::find(inputState.disabled_tools.begin(),
                                   inputState.disabled_tools.end(),
                                   preselectionState.active_tool) !=
                         inputState.disabled_tools.end();
    canSelectTool = tool_allowed && !tool_disabled;
  }

  if (canSelectTool) {
    [self setActiveTool:preselectionState.active_tool];
  }
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
  _cachedStrings = ServerStringsFromInputState(inputState);
  [self.delegate inputStateManager:self didUpdateInputState:inputState];
}

#pragma mark - InputState Helpers

// Returns the current active mode from the mode holder.
- (ComposeboxMode)activeMode {
  return _modeHolder.mode;
}

// Whether the given mode is allowed in the input state.
- (BOOL)toolAllowedInInputState:(omnibox::ToolMode)toolMode {
  if (!EnableComposeboxServerSideState()) {
    return YES;
  }
  return std::find(_inputState.allowed_tools.begin(),
                   _inputState.allowed_tools.end(),
                   toolMode) != _inputState.allowed_tools.end();
}

// Whether the given mode is disabled in the input state.
- (BOOL)toolDisabledInInputState:(omnibox::ToolMode)toolMode {
  if (!EnableComposeboxServerSideState()) {
    return NO;
  }
  return std::find(_inputState.disabled_tools.begin(),
                   _inputState.disabled_tools.end(),
                   toolMode) != _inputState.disabled_tools.end();
}

// Whether the given tool mode is selectable.
- (BOOL)canSelectToolBasedOnInputState:(omnibox::ToolMode)toolMode {
  return [self toolAllowedInInputState:toolMode] &&
         ![self toolDisabledInInputState:toolMode];
}

// Whether the given mode is allowed in the input state.
- (BOOL)modelAllowedInInputState:(omnibox::ModelMode)modelMode {
  if (!EnableComposeboxServerSideState()) {
    return YES;
  }
  return std::find(_inputState.allowed_models.begin(),
                   _inputState.allowed_models.end(),
                   modelMode) != _inputState.allowed_models.end();
}

// Whether the given mode is disabled in the input state.
- (BOOL)modelDisabledInInputState:(omnibox::ModelMode)modelMode {
  if (!EnableComposeboxServerSideState()) {
    return NO;
  }
  return std::find(_inputState.disabled_models.begin(),
                   _inputState.disabled_models.end(),
                   modelMode) != _inputState.disabled_models.end();
}

// Whether the given model mode is selectable.
- (BOOL)canSelectModelBasedOnInputState:(omnibox::ModelMode)modelMode {
  return [self modelAllowedInInputState:modelMode] &&
         ![self modelDisabledInInputState:modelMode];
}

/// Whether the current input state disables the given input type.
- (BOOL)inputStateDisablesType:(omnibox::InputType)inputType {
  return std::find(_inputState.disabled_input_types.begin(),
                   _inputState.disabled_input_types.end(),
                   inputType) != _inputState.disabled_input_types.end();
}

/// Whether the current input state allows the given input type.
- (BOOL)inputStateAllowsType:(omnibox::InputType)inputType {
  return std::find(_inputState.allowed_input_types.begin(),
                   _inputState.allowed_input_types.end(),
                   inputType) != _inputState.allowed_input_types.end();
}

#pragma mark - Eligibility & Availability

/// Whether the user is eligible to share content (enterprise policy).
- (BOOL)isContentSharingEnabled {
  return _prefService && _sessionHandle &&
         _sessionHandle->CheckSearchContentSharingSettings(_prefService);
}

/// Whether the user is eligible to use Create Image.
- (BOOL)imageToolAllowed {
  if (experimental_flags::ShouldForceDisableComposeboxCreateImages()) {
    return NO;
  }

  if (EnableComposeboxServerSideState()) {
    return
        [self toolAllowedInInputState:omnibox::ToolMode::TOOL_MODE_IMAGE_GEN];
  } else {
    if (!_aimEligibilityService) {
      return NO;
    }
    return _aimEligibilityService->IsCreateImagesEligible();
  }
}

/// Whether the user is eligible to use Canvas.
- (BOOL)canvasToolAllowed {
  if (!ShowComposeboxAdditionalAdvancedTools()) {
    return NO;
  }
  if (experimental_flags::ShouldForceDisableComposeboxCanvas()) {
    return NO;
  }

  if (EnableComposeboxServerSideState()) {
    return [self toolAllowedInInputState:omnibox::TOOL_MODE_CANVAS];
  } else {
    if (!_aimEligibilityService) {
      return NO;
    }
    return _aimEligibilityService->IsCanvasEligible();
  }
}

/// Whether the user is eligible to use Deep Search.
- (BOOL)deepSearchToolAllowed {
  if (!ShowDeepSearchTool()) {
    return NO;
  }
  if (experimental_flags::ShouldForceDisableComposeboxDeepSearch()) {
    return NO;
  }

  if (EnableComposeboxServerSideState()) {
    return [self toolAllowedInInputState:omnibox::TOOL_MODE_DEEP_SEARCH];
  } else {
    if (!_aimEligibilityService) {
      return NO;
    }
    return _aimEligibilityService->IsDeepSearchEligible();
  }
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
  BOOL canCreateImage = [self imageToolAllowed];
  BOOL canUseCanvas = [self canvasToolAllowed];
  BOOL canUseDeepSearch = [self deepSearchToolAllowed];
  return canUseCanvas || canCreateImage || canUseDeepSearch || canSearchWithAI;
}

/// Whether the current state allows tab attachments.
- (BOOL)tabAttachmentAllowed {
  if (![self attachmentsAvailable]) {
    return NO;
  }
  if (EnableComposeboxServerSideState()) {
    return [self inputStateAllowsType:omnibox::INPUT_TYPE_BROWSER_TAB];
  }

  return YES;
}

/// Whether the current state allows file attachments.
- (BOOL)fileAttachmentAllowed {
  if (![self attachmentsAvailable]) {
    return NO;
  }

  if (EnableComposeboxServerSideState()) {
    return [self inputStateAllowsType:omnibox::INPUT_TYPE_LENS_FILE];
  } else {
    return [self isEligibleToUploadPdf];
  }
}

/// Whether the current state allows image attachments.
- (BOOL)imageAttachmentAllowed {
  if (![self attachmentsAvailable]) {
    return NO;
  }

  if (EnableComposeboxServerSideState() &&
      ![self inputStateAllowsType:omnibox::INPUT_TYPE_LENS_IMAGE]) {
    return NO;
  }

  return YES;
}

#pragma mark - Attachment Rules

// The absolute value for the maximum number of attachments available.
- (NSUInteger)totalAttachmentLimit {
  if (EnableComposeboxServerSideState()) {
    return _inputState.max_total_inputs;
  }
  return kAttachmentLimit;
}

// Whether Create Image is in the list of disabled tools.
// If restricted, the tool will persist in the UI with a 'disabled' status,
// pending a change in state.
- (BOOL)imageToolDisabled {
  if ([self activeMode] == ComposeboxMode::kImageGeneration) {
    return NO;
  }
  BOOL generateImageDisabled =
      [self toolDisabledInInputState:omnibox::ToolMode::TOOL_MODE_IMAGE_GEN] ||
      [self toolDisabledInInputState:omnibox::ToolMode::
                                         TOOL_MODE_IMAGE_GEN_UPLOAD];

  return generateImageDisabled || self.items.hasTabOrFile;
}

// Whether Canvas is in the list of disabled tools.
// If restricted, the tool will persist in the UI with a 'disabled' status,
// pending a change in state.
- (BOOL)canvasToolDisabled {
  if ([self activeMode] == ComposeboxMode::kCanvas) {
    return NO;
  }
  return [self toolDisabledInInputState:omnibox::ToolMode::TOOL_MODE_CANVAS];
}

// Whether Deep Search is in the list of disabled tools.
// If restricted, the tool will persist in the UI with a 'disabled' status,
// pending a change in state.
- (BOOL)deepSearchToolDisabled {
  if ([self activeMode] == ComposeboxMode::kDeepSearch) {
    return NO;
  }
  return
      [self toolDisabledInInputState:omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH];
}

// Disables tab attachment.
// If restricted, the tool will persist in the UI with a 'disabled' status,
// pending a change in state.
- (BOOL)tabAttachmentDisabled {
  if (![self canAddMoreAttachments]) {
    return YES;
  }

  if (EnableComposeboxServerSideState()) {
    return [self inputStateDisablesType:omnibox::INPUT_TYPE_BROWSER_TAB];
  }

  BOOL isImageCreationMode =
      [self activeMode] == ComposeboxMode::kImageGeneration;
  return isImageCreationMode;
}

// Whether the current state disables file attachments.
// If restricted, the tool will persist in the UI with a 'disabled' status,
// pending a change in state.
- (BOOL)fileAttachmentDisabled {
  if (![self canAddMoreAttachments]) {
    return YES;
  }

  if (EnableComposeboxServerSideState()) {
    return [self inputStateDisablesType:omnibox::INPUT_TYPE_LENS_FILE];
  }

  BOOL isImageCreationMode =
      [self activeMode] == ComposeboxMode::kImageGeneration;
  return isImageCreationMode;
}

// Whether the current state disables image attachments.
// If restricted, the tool will persist in the UI with a 'disabled' status,
// pending a change in state.
- (BOOL)imageAttachmentDisabled {
  if (![self canAddMoreAttachments]) {
    return YES;
  }

  if (EnableComposeboxServerSideState()) {
    return [self inputStateDisablesType:omnibox::INPUT_TYPE_LENS_IMAGE];
  }

  return NO;
}

@end
