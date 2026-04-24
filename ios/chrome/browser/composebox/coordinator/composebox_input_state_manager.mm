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
      ModelModeForModelOption(modelOption, _inputState);

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

/// Returns the default mode/tool that is implicit in the context. The tool is
/// not displayed in the input menu.
- (ComposeboxMode)defaultTool {
  if (_entrypoint == ComposeboxEntrypoint::kCobrowse) {
    return ComposeboxMode::kAIM;
  } else {
    return ComposeboxMode::kRegularSearch;
  }
}

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

- (BOOL)canSelectTool:(ComposeboxMode)mode {
  return [self isToolAllowed:mode] && ![self isToolDisabled:mode];
}

- (BOOL)canSelectModel:(ComposeboxModelOption)modelOption {
  return
      [self isModelAllowed:modelOption] && ![self isModelDisabled:modelOption];
}

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

  omnibox::ModelMode modelMode =
      ModelModeForModelOption(modelOption, _inputState);
  return std::ranges::contains(_inputState.allowed_models, modelMode);
}

- (BOOL)isModelDisabled:(ComposeboxModelOption)modelOption {
  // Models are only available from server state.
  if (!ShowComposeboxAdditionalAdvancedTools() ||
      !EnableComposeboxServerSideState()) {
    return NO;
  }

  omnibox::ModelMode modelMode =
      ModelModeForModelOption(modelOption, _inputState);
  return std::ranges::contains(_inputState.disabled_models, modelMode);
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
    bool model_allowed = std::ranges::contains(inputState.allowed_models,
                                               preselectionState.active_model);
    bool model_disabled = std::ranges::contains(inputState.disabled_models,
                                                preselectionState.active_model);
    canSelectModel = model_allowed && !model_disabled;
  }

  if (canSelectModel && _inputStateModel) {
    _inputStateModel->setActiveModel(preselectionState.active_model);
  }

  bool canSelectTool = NO;
  if (!EnableComposeboxServerSideState()) {
    canSelectTool = YES;
  } else {
    bool tool_allowed = std::ranges::contains(inputState.allowed_tools,
                                              preselectionState.active_tool);
    bool tool_disabled = std::ranges::contains(inputState.disabled_tools,
                                               preselectionState.active_tool);
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

#pragma mark - Eligibility & Availability

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
  if (EnableComposeboxServerSideState()) {
    return _inputState.max_total_inputs;
  }
  return kAttachmentLimit;
}

#pragma mark - Helpers

/// Whether the given attachment option is allowed by the server.
- (BOOL)isAttachmentAllowedByServer:
    (ComposeboxAttachmentOption)attachmentOption {
  using enum ComposeboxAttachmentOption;
  switch (attachmentOption) {
    case kCurrentTab:
    case kTab:
      return std::ranges::contains(_inputState.allowed_input_types,
                                   omnibox::INPUT_TYPE_BROWSER_TAB);
    case kFile:
      return std::ranges::contains(_inputState.allowed_input_types,
                                   omnibox::INPUT_TYPE_LENS_FILE);
    case kGallery:
    case kCamera:
      return std::ranges::contains(_inputState.allowed_input_types,
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
  using enum ComposeboxAttachmentOption;
  switch (attachmentOption) {
    case kCurrentTab:
    case kTab:
      return std::ranges::contains(_inputState.disabled_input_types,
                                   omnibox::INPUT_TYPE_BROWSER_TAB);
    case kFile:
      return std::ranges::contains(_inputState.disabled_input_types,
                                   omnibox::INPUT_TYPE_LENS_FILE);
    case kGallery:
    case kCamera:
      return std::ranges::contains(_inputState.disabled_input_types,
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
  using enum ComposeboxMode;
  switch (mode) {
    case kImageGeneration:
      return std::ranges::contains(_inputState.allowed_tools,
                                   omnibox::ToolMode::TOOL_MODE_IMAGE_GEN);
    case kCanvas:
      return std::ranges::contains(_inputState.allowed_tools,
                                   omnibox::ToolMode::TOOL_MODE_CANVAS);
    case kDeepSearch:
      return std::ranges::contains(_inputState.allowed_tools,
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
  using enum ComposeboxMode;
  switch (mode) {
    case kImageGeneration:
      return std::ranges::contains(_inputState.disabled_tools,
                                   omnibox::ToolMode::TOOL_MODE_IMAGE_GEN) ||
             std::ranges::contains(
                 _inputState.disabled_tools,
                 omnibox::ToolMode::TOOL_MODE_IMAGE_GEN_UPLOAD);
    case kCanvas:
      return std::ranges::contains(_inputState.disabled_tools,
                                   omnibox::ToolMode::TOOL_MODE_CANVAS);
    case kDeepSearch:
      return std::ranges::contains(_inputState.disabled_tools,
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
