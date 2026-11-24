// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/coordinator/composebox_input_state_manager.h"

#import <algorithm>
#import <optional>
#import <ranges>
#import <unordered_map>

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/contextual_search/contextual_search_metrics_recorder.h"
#import "components/contextual_search/contextual_search_session_handle.h"
#import "components/omnibox/browser/aim_eligibility_service.h"
#import "components/omnibox/common/omnibox_features.h"
#import "components/prefs/pref_service.h"
#import "components/search/search.h"
#import "components/search_engines/template_url_service.h"
#import "components/search_engines/util.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_constants.h"
#import "ios/chrome/browser/composebox/menu/coordinator/composebox_menu_shared_tab.h"
#import "ios/chrome/browser/composebox/public/features.h"
#import "ios/chrome/browser/composebox/shared/metrics/composebox_metrics_recorder.h"
#import "ios/chrome/browser/composebox/ui/composebox_input_item.h"
#import "ios/chrome/browser/composebox/ui/composebox_input_item_collection.h"
#import "ios/chrome/browser/composebox/ui/composebox_model_item.h"
#import "ios/chrome/browser/composebox/ui/composebox_ui_config.h"
#import "ios/chrome/browser/composebox/ui/composebox_ui_input_state.h"
#import "ios/chrome/browser/composebox/ui/composebox_ui_util.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_utils.h"
#import "ios/chrome/browser/search_engines/model/search_engine_observer_bridge.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/NSString+Chromium.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_id.h"

namespace {

// Returns the local fallback icon for the given `model`, used when the server
// doesn't provide one.
UIImage* FallbackIconForModel(omnibox::ModelMode model) {
  switch (model) {
    case omnibox::ModelMode::MODEL_MODE_GEMINI_PRO:
    case omnibox::ModelMode::MODEL_MODE_GEMINI_PRO_LATEST:
    case omnibox::ModelMode::MODEL_MODE_GEMINI_PRO_NO_GEN_UI:
      return SymbolWithPointSize(SymbolClock, kSymbolActionPointSize);
    case omnibox::ModelMode::MODEL_MODE_GEMINI_PRO_AUTOROUTE:
      return SymbolWithPointSize(SymbolArrowTrianglehead2ClockwiseRotate90,
                                 kSymbolActionPointSize);
    case omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR:
    case omnibox::ModelMode::MODEL_MODE_UNSPECIFIED:
      return SymbolWithPointSize(SymbolAcute, kSymbolActionPointSize);
    default:
      return SymbolWithPointSize(SymbolBolt, kSymbolActionPointSize);
  }
}

// Returns the legacy `ComposeboxMode` matching `tool_mode`, if one exists.
std::optional<ComposeboxMode> ModeForToolMode(omnibox::ToolMode tool_mode) {
  using enum ComposeboxMode;
  switch (tool_mode) {
    case omnibox::ToolMode::TOOL_MODE_CANVAS:
      return kCanvas;
    case omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH:
      return kDeepSearch;
    case omnibox::ToolMode::TOOL_MODE_GEMINI_PRO:
      return kPTM;
    case omnibox::ToolMode::TOOL_MODE_IMAGE_GEN:
      return kImageGeneration;
    case omnibox::ToolMode::TOOL_MODE_AIM:
      return kAIM;
    default:
      return std::nullopt;
  }
}

// Returns the outward `ComposeboxMode` for `tool_mode` while UI consumers still
// read `ComposeboxMode`, mapping unmapped server tools to
// `ComposeboxMode::kAIM`.
ComposeboxMode ComposeboxModeForToolMode(omnibox::ToolMode tool_mode) {
  if (tool_mode == omnibox::ToolMode::TOOL_MODE_UNSPECIFIED) {
    return ComposeboxMode::kRegularSearch;
  }
  return ModeForToolMode(tool_mode).value_or(ComposeboxMode::kAIM);
}

// Returns the tool mode for the given composebox `mode`.
omnibox::ToolMode ToolModeForComposeboxMode(ComposeboxMode mode) {
  switch (mode) {
    case ComposeboxMode::kCanvas:
      return omnibox::ToolMode::TOOL_MODE_CANVAS;
    case ComposeboxMode::kDeepSearch:
      return omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH;
    case ComposeboxMode::kPTM:
      return omnibox::ToolMode::TOOL_MODE_GEMINI_PRO;
    case ComposeboxMode::kImageGeneration:
      return omnibox::ToolMode::TOOL_MODE_IMAGE_GEN;
    case ComposeboxMode::kAIM:
      return omnibox::ToolMode::TOOL_MODE_AIM;
    case ComposeboxMode::kRegularSearch:
      return omnibox::ToolMode::TOOL_MODE_UNSPECIFIED;
  }
}

// Returns the server UI config object from a given input state.
ComposeboxUIConfig* ServerUIConfigFromInputState(
    const contextual_search::InputState& input_state) {
  std::unordered_map<ComposeboxMode, ComposeboxItemUIConfig*> tool_mapping;
  for (const omnibox::ToolConfig& tool_config : input_state.tool_configs) {
    NSString* menuLabel = base::SysUTF8ToNSString(tool_config.menu_label());
    NSString* chipLabel = base::SysUTF8ToNSString(tool_config.chip_label());
    NSString* hintText = base::SysUTF8ToNSString(tool_config.hint_text());
    UIImage* icon = nil;
    if (tool_config.has_icon() && tool_config.icon().has_icon_id()) {
      icon = ImageForIconResourceId(tool_config.icon().icon_id(),
                                    kSymbolActionPointSize);
    }
    std::optional<ComposeboxMode> mode = ModeForToolMode(tool_config.tool());
    if (mode) {
      tool_mapping[*mode] =
          [[ComposeboxItemUIConfig alloc] initWithMenuLabel:menuLabel
                                                  chipLabel:chipLabel
                                                   hintText:hintText
                                                       icon:icon];
    }
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

  return [[ComposeboxUIConfig alloc] initWithToolMapping:tool_mapping
                                      modelSectionHeader:modelSectionHeader
                                      toolsSectionHeader:toolsSectionHeader];
}

// Returns the server `InputType` corresponding to `attachment_option`.
omnibox::InputType InputTypeForAttachmentOption(
    ComposeboxAttachmentOption attachment_option) {
  using enum ComposeboxAttachmentOption;
  switch (attachment_option) {
    case kCurrentTab:
    case kTab:
      return omnibox::INPUT_TYPE_BROWSER_TAB;
    case kFile:
      return omnibox::INPUT_TYPE_LENS_FILE;
    case kGallery:
    case kCamera:
      return omnibox::INPUT_TYPE_LENS_IMAGE;
    case kDrive:
      return omnibox::INPUT_TYPE_DRIVE;
  }
}

// Returns the server `InputType` corresponding to an attached `item_type`.
omnibox::InputType InputTypeForItemType(ComposeboxInputItemType item_type) {
  using enum ComposeboxInputItemType;
  switch (item_type) {
    case kComposeboxInputItemTypeImage:
      return omnibox::INPUT_TYPE_LENS_IMAGE;
    case kComposeboxInputItemTypePDF:
    case kComposeboxInputItemTypeRawFile:
      return omnibox::INPUT_TYPE_LENS_FILE;
    case kComposeboxInputItemTypeTab:
      return omnibox::INPUT_TYPE_BROWSER_TAB;
    case kComposeboxInputItemTypeDrive:
      return omnibox::INPUT_TYPE_DRIVE;
  }
}
}  // namespace

@interface ComposeboxInputStateManager () <IdentityManagerObserving,
                                           SearchEngineObserving>
@end

@implementation ComposeboxInputStateManager {
  // The entrypoint from which the composebox was opened.
  ComposeboxEntrypoint _entrypoint;
  // The active tool mode: `TOOL_MODE_UNSPECIFIED` in regular search,
  // `TOOL_MODE_AIM` in plain AI Mode, or the active server tool.
  omnibox::ToolMode _activeToolMode;
  // The active model, or `MODEL_MODE_UNSPECIFIED` when no model is explicitly
  // selected (e.g. in regular search). Unlike `InputStateModel`, which only
  // models AIM state and always keeps a concrete active model, this manager
  // also covers regular search where no model applies.
  omnibox::ModelMode _activeModel;

  // The web state list of the browser.
  raw_ptr<WebStateList> _webStateList;
  // Pref service for user preferences.
  raw_ptr<PrefService> _prefService;
  // Service to check AIM eligibility.
  raw_ptr<AimEligibilityService> _aimEligibilityService;
  // Identity manager for checking account status.
  raw_ptr<signin::IdentityManager> _identityManager;
  // Bridge to observe `IdentityManager` events.
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserverBridge;
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
  // Cached server UI config.
  ComposeboxUIConfig* _cachedUIConfig;
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
    // Initialize with local fallback config. These will be overwritten
    // when server-side config becomes available via the input state model.
    _cachedUIConfig = [ComposeboxUIConfig localFallbackUIConfig];
    _webStateList = webStateList;
    _prefService = prefService;
    _aimEligibilityService = aimEligibilityService;
    _identityManager = identityManager;
    if (_identityManager) {
      _identityManagerObserverBridge =
          std::make_unique<signin::IdentityManagerObserverBridge>(
              _identityManager, self);
    }
    _templateURLService = templateURLService;
    // sessionHandle can be nil only in tests.
    if (sessionHandle) {
      _sessionHandle = sessionHandle->AsWeakPtr();
    }
    _entrypoint = entrypoint;
    _activeToolMode = [self defaultToolMode];
    _isIncognito = isIncognito;

    if (_templateURLService) {
      _isDSEGoogle = search::DefaultSearchProviderIsGoogle(_templateURLService);
      _searchEngineObserver = std::make_unique<SearchEngineObserverBridge>(
          self, _templateURLService);
    }

    if (_aimEligibilityService) {
      __weak __typeof(self) weakSelf = self;
      _aimEligibilitySubscription =
          _aimEligibilityService->RegisterEligibilityChangedCallback(
              base::BindRepeating(^{
                [weakSelf onAimEligibilityChanged];
              }));
    }

    [self updateSearchboxConfig];
  }
  return self;
}

- (void)disconnect {
  self.items = nil;
  _inputStateSubscription = {};
  _inputStateModel.reset();
  _inputState.reset();
  _cachedUIConfig = nil;
  _webStateList = nullptr;
  _prefService = nullptr;
  _aimEligibilityService = nullptr;
  _identityManagerObserverBridge.reset();
  _identityManager = nullptr;
  _templateURLService = nullptr;
  _sessionHandle.reset();
  _searchEngineObserver.reset();
  _aimEligibilitySubscription = {};
  _metricsRecorder = nil;
}

- (void)setActiveToolMode:(omnibox::ToolMode)toolMode {
  if (![self canSelectToolMode:toolMode]) {
    return;
  }
  omnibox::ToolMode nextToolMode =
      toolMode == omnibox::ToolMode::TOOL_MODE_UNSPECIFIED
          ? [self defaultToolMode]
          : toolMode;
  if (_activeToolMode == nextToolMode) {
    return;
  }
  _activeToolMode = nextToolMode;
  [self handleToolModeChange];
}

- (void)toggleToolMode:(omnibox::ToolMode)toolMode
      activationSource:(AiModeActivationSource)activationSource {
  if (self.activeToolMode == toolMode) {
    [self setActiveToolMode:[self defaultToolMode]];
    return;
  }
  if (![self canSelectToolMode:toolMode]) {
    return;
  }
  [self setActiveToolMode:toolMode];
  if (toolMode == omnibox::ToolMode::TOOL_MODE_AIM) {
    [self.metricsRecorder recordAiModeActivationSource:activationSource];
  }
  if (toolMode != omnibox::ToolMode::TOOL_MODE_UNSPECIFIED) {
    [self.metricsRecorder recordToolSelected:toolMode];
  }
}

- (omnibox::ModelMode)activeModel {
  // `InputStateModel` keeps its default model active even in regular search,
  // where no model applies on the client.
  if ([self isRegularSearch] || !_inputStateModel) {
    return omnibox::ModelMode::MODEL_MODE_UNSPECIFIED;
  }
  return _inputStateModel->GetInputState().active_model;
}

- (void)setActiveModel:(omnibox::ModelMode)model
    explicitUserAction:(BOOL)explicitUserAction {
  if (!_inputStateModel) {
    return;
  }

  if (![self canSelectModel:model]) {
    return;
  }

  if (model == _activeModel) {
    return;
  }
  _activeModel = model;

  [self setActiveModelInInputState:model explicitUserAction:explicitUserAction];

  [self.delegate inputStateManagerDidUpdateUIState:self];

  // Update mode on model change.

  // Only when the user explicitly picked the advanced model in regular mode
  // do the switch to AIM.
  BOOL switchToAIM = explicitUserAction && [self isRegularSearch];
  if (switchToAIM) {
    [self setActiveToolMode:omnibox::ToolMode::TOOL_MODE_AIM];
    [self.metricsRecorder
        recordAiModeActivationSource:AiModeActivationSource::kImplicit];
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
  // AI mode is implicitly enabled by items attachment.
  BOOL shouldSwitchToAIM =
      self.items && !self.items.empty && [self isRegularSearch];
  if (shouldSwitchToAIM) {
    [self setActiveToolMode:omnibox::ToolMode::TOOL_MODE_AIM];
    [self.metricsRecorder
        recordAiModeActivationSource:AiModeActivationSource::kImplicit];
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
  if (!_aimEligibilityService) {
    return NO;
  }
  return _aimEligibilityService->IsFuseboxEligible();
}

/// Returns the locale-adjusted short name of the default search engine, or
/// `nil` if unavailable.
- (NSString*)defaultSearchEngineName {
  if (!_templateURLService) {
    return nil;
  }
  const TemplateURL* searchProvider =
      _templateURLService->GetDefaultSearchProvider();
  if (!searchProvider) {
    return nil;
  }
  return base::SysUTF16ToNSString(
      searchProvider->AdjustedShortNameForLocaleDirection());
}

- (BOOL)isDSEGoogle {
  if (!_templateURLService) {
    return NO;
  }
  return search::DefaultSearchProviderIsGoogle(_templateURLService);
}

- (BOOL)canSelectModel:(omnibox::ModelMode)model {
  return [self isModelAllowed:model] && ![self isModelDisabled:model];
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

  return availableSlots;
}

- (NSUInteger)remainingNumberOfImagesAllowed {
  NSUInteger remainingAttachmentCapacity = [self remainingAttachmentCapacity];
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

  const GURL& URL = webState->GetVisibleURL();
  if (IsAimURL(URL) || IsAimZeroStateURL(URL)) {
    return NO;
  }

  BOOL isExtractable =
      CanExtractPageContextForWebState(webState, [self isEligibleToUploadPdf]);
  BOOL alreadyProcessed =
      attachedWebStateIDs.contains(webState->GetUniqueIdentifier());

  return isExtractable && !alreadyProcessed && [self isContentSharingEnabled];
}

- (NSUInteger)maxTabAttachmentCount {
  NSUInteger remainingAttachmentCapacity = [self remainingAttachmentCapacity];
  NSUInteger capacityForTabs =
      remainingAttachmentCapacity + self.items.tabsCount;

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

  state.activeTool = ComposeboxModeForToolMode(self.activeToolMode);
  state.defaultToolActive = self.activeToolMode == [self defaultToolMode];
  state.hasAttachments = self.items && !self.items.empty;

  _cachedUIConfig.hasAttachments = state.hasAttachments;
  _cachedUIConfig.defaultSearchEngineName = [self defaultSearchEngineName];
  state.uiConfig = _cachedUIConfig;

  NSMutableArray<ComposeboxMenuSharedTab*>* sharedTabs =
      [[NSMutableArray alloc] init];
  if (_entrypoint == ComposeboxEntrypoint::kCobrowse) {
    for (ComposeboxInputItem* item in self.items.containedItems) {
      if (item.type == ComposeboxInputItemType::kComposeboxInputItemTypeTab) {
        ComposeboxMenuSharedTab* tab =
            [[ComposeboxMenuSharedTab alloc] initWithURL:item.tabURL
                                                   title:item.title
                                     inputItemIdentifier:item.identifier
                                                 favicon:item.leadingIconImage];
        [sharedTabs addObject:tab];
      }
    }
  }
  state.sharedTabs = sharedTabs;

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
  omnibox::ToolMode defaultToolMode = [self defaultToolMode];
  // Iterates over `ComposeboxMode` for now; this will be migrated to
  // `omnibox::ToolMode` once the UI consumers have been updated.
  for (ComposeboxMode mode : ComposeboxModeSet::All()) {
    omnibox::ToolMode toolMode = ToolModeForComposeboxMode(mode);
    if (toolMode == defaultToolMode) {
      continue;
    }
    if ([self isToolModeAllowed:toolMode]) {
      allowedTools.insert(mode);
    }
    if ([self isToolModeDisabled:toolMode]) {
      disabledTools.insert(mode);
    }
  }

  state.allowedTools = allowedTools;
  state.disabledTools = disabledTools;
  state.modelItems = [self modelItems];

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

#pragma mark - IdentityManagerObserving

- (void)primaryAccountDidChange:
    (const signin::PrimaryAccountChangeEvent&)event {
  switch (event.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
    case signin::PrimaryAccountChangeEvent::Type::kSet:
      [self updateSearchboxConfig];
      break;
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      break;
  }
}

#pragma mark - Tool Mode Management

// Updates the underlying C++ model, invalidates disallowed attachments, and
// notifies the delegate after `_activeToolMode` changes.
- (void)handleToolModeChange {
  [self setActiveToolInInputState:self.activeToolMode];
  [self updateModelOnModeChange];

  // Invalidate items/attachments for the current mode.
  NSArray<ComposeboxInputItem*>* invalidatedItems =
      [self invalidItemsInActiveMode];

  [self.delegate
           inputStateManager:self
               didChangeMode:ComposeboxModeForToolMode(self.activeToolMode)
      invalidatedAttachments:invalidatedItems];
  [self.delegate inputStateManagerDidUpdateUIState:self];
}

/// Updates the active model on tool mode change.
- (void)updateModelOnModeChange {
  if ([self isRegularSearch]) {
    // Clear the model selection when switching to regular search.
    [self setActiveModel:omnibox::ModelMode::MODEL_MODE_UNSPECIFIED
        explicitUserAction:NO];
    return;
  } else if (_activeModel == omnibox::ModelMode::MODEL_MODE_UNSPECIFIED) {
    // In AI tools, set the default model when none is selected.
    [self setActiveModel:[self defaultModel] explicitUserAction:NO];
  }
}

// Returns items (attachments) that are invalid (incompatible with the current
// tool mode or active tool rule) and pending deletion.
- (NSArray<ComposeboxInputItem*>*)invalidItemsInActiveMode {
  NSMutableArray<ComposeboxInputItem*>* invalidatedItems =
      [NSMutableArray array];
  NSArray<ComposeboxInputItem*>* containedItems = self.items.containedItems;

  if ([self isRegularSearch]) {
    // Invalidate all items.
    if (containedItems) {
      [invalidatedItems addObjectsFromArray:containedItems];
    }
    return invalidatedItems;
  }

  for (ComposeboxInputItem* item in containedItems) {
    if (![self
            isInputTypeAllowedByActiveTool:InputTypeForItemType(item.type)]) {
      [invalidatedItems addObject:item];
    }
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
      /*is_signed_in=*/has_primary_account,
      /*browser_identity_matches_aim_identity=*/has_primary_account);
  if (_prefService) {
    _inputStateModel->SetPrefService(_prefService);
  }

  [self startInputStateObservation];
  // `Initialize` is synchronous and immediately notifies observers, updating
  // the state.
  _inputStateModel->Initialize();

  // iOS doesn't rely on the active hint text from `_inputState`, UI config only
  // changes when `searchboxConfig` is updated.
  _cachedUIConfig =
      ServerUIConfigFromInputState(_inputStateModel->GetInputState());

  [self.delegate inputStateManagerDidUpdateUIState:self];
}

#pragma mark Observation

// Starts observing state changes from the model.
- (void)startInputStateObservation {
  if (!_inputStateModel) {
    return;
  }
  __weak __typeof(self) weakSelf = self;
  _inputStateSubscription = _inputStateModel->Subscribe(
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

// Returns `YES` if the given `model` matches `modelMode`.
- (BOOL)isModel:(omnibox::ModelMode)model
    matchingModelMode:(omnibox::ModelMode)modelMode {
  if ([self isRegularSearch]) {
    // Any model is valid in regular search.
    return YES;
  }
  if (model == omnibox::ModelMode::MODEL_MODE_UNSPECIFIED) {
    // `MODEL_MODE_UNSPECIFIED` is not valid in AI mode.
    return NO;
  }
  return model == modelMode;
}

// Synchronizes the local active tool mode with the server-side input state,
// ensuring the local selection is valid according to the current server
// configuration.
- (void)reconcileToolModeWithInputState {
  if (!_inputState.has_value()) {
    return;
  }
  omnibox::ToolMode activeToolMode = self.activeToolMode;
  if (![self canSelectToolMode:activeToolMode]) {
    [self setActiveToolMode:[self defaultToolMode]];
    return;
  }
  // Sync local and `inputState` tools (`TOOL_MODE_AIM` isn't supported in
  // `_inputState` and maps to `TOOL_MODE_UNSPECIFIED`).
  omnibox::ToolMode expectedInputStateTool =
      activeToolMode == omnibox::ToolMode::TOOL_MODE_AIM
          ? omnibox::ToolMode::TOOL_MODE_UNSPECIFIED
          : activeToolMode;
  if (expectedInputStateTool == _inputState->active_tool) {
    return;
  }
  [self setActiveToolInInputState:activeToolMode];
}

// Synchronizes the local active model with the server-side input state,
// ensuring the local selection is valid according to the current server
// configuration.
- (void)reconcileModelWithInputState {
  if (!_inputState.has_value()) {
    return;
  }
  // Local and inputStateModel matches.
  if ([self isModel:_activeModel matchingModelMode:_inputState->active_model]) {
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
  if (![self attachmentsAvailable] || !_inputState.has_value()) {
    return NO;
  }

  if (!std::ranges::contains(_inputState->allowed_input_types,
                             InputTypeForAttachmentOption(attachmentOption))) {
    return NO;
  }

  // Enforce signed-in status and feature flag for the Drive option after
  // verifying server-side eligibility.
  if (attachmentOption == ComposeboxAttachmentOption::kDrive) {
    BOOL hasPrimaryAccount =
        _identityManager &&
        _identityManager->HasPrimaryAccount(signin::ConsentLevel::kSignin);
    if (!hasPrimaryAccount) {
      return NO;
    }

    return IsComposeboxDriveOptionEnabled();
  }

  return YES;
}

// Whether the given attachment option is disabled.
- (BOOL)isAttachmentDisabled:(ComposeboxAttachmentOption)attachmentOption {
  if (![self canAddMoreAttachments]) {
    return YES;
  }

  if (!_inputState.has_value()) {
    return NO;
  }

  if (!std::ranges::contains(_inputState->disabled_input_types,
                             InputTypeForAttachmentOption(attachmentOption))) {
    return NO;
  }

  // When Drive is flag-disabled, it is hidden via `isAttachmentAllowed:` rather
  // than shown as disabled.
  if (attachmentOption == ComposeboxAttachmentOption::kDrive) {
    return IsComposeboxDriveOptionEnabled();
  }

  return YES;
}

// Whether `inputType` is compatible with the active tool's `ToolConfig` rule,
// ignoring attachment count limits and model restrictions.
- (BOOL)isInputTypeAllowedByActiveTool:(omnibox::InputType)inputType {
  if ([self isRegularSearch]) {
    return NO;
  }
  const omnibox::ToolConfig* config =
      [self toolConfigForToolMode:self.activeToolMode];
  if (!config || !config->has_rule() ||
      config->rule().allow_all_input_types()) {
    return YES;
  }
  return std::ranges::contains(config->rule().allowed_input_types(), inputType);
}

// Whether the given `toolMode` can be selected.
- (BOOL)canSelectToolMode:(omnibox::ToolMode)toolMode {
  if (toolMode == omnibox::ToolMode::TOOL_MODE_UNSPECIFIED ||
      toolMode == [self defaultToolMode]) {
    return YES;
  }
  return
      [self isToolModeAllowed:toolMode] && ![self isToolModeDisabled:toolMode];
}

// Whether the given `toolMode` is allowed.
- (BOOL)isToolModeAllowed:(omnibox::ToolMode)toolMode {
  if (toolMode == [self defaultToolMode]) {
    return YES;
  }
  if (![self isEligibleToAIM] || !_inputState.has_value()) {
    return NO;
  }
  switch (toolMode) {
    case omnibox::ToolMode::TOOL_MODE_AIM:
    case omnibox::ToolMode::TOOL_MODE_UNSPECIFIED:
      return _entrypoint != ComposeboxEntrypoint::kCobrowse;
    default:
      return std::ranges::contains(_inputState->allowed_tools, toolMode);
  }
}

// Returns the server `ToolConfig` matching `toolMode`, or `nullptr` if absent.
- (const omnibox::ToolConfig*)toolConfigForToolMode:
    (omnibox::ToolMode)toolMode {
  if (!_inputState.has_value() ||
      toolMode == omnibox::ToolMode::TOOL_MODE_UNSPECIFIED ||
      toolMode == omnibox::ToolMode::TOOL_MODE_AIM) {
    // Plain AI Mode (`TOOL_MODE_AIM`) has no server `ToolConfig`; its input
    // type limits and compatibility rules (`RuleSet` counts and active
    // `ModelRule` restrictions) are still enforced via `_inputState`.
    return nullptr;
  }
  for (const omnibox::ToolConfig& candidate : _inputState->tool_configs) {
    if (candidate.tool() == toolMode) {
      return &candidate;
    }
  }
  return nullptr;
}

// Whether the given `toolMode` is disabled.
- (BOOL)isToolModeDisabled:(omnibox::ToolMode)toolMode {
  if (toolMode == self.activeToolMode || !_inputState.has_value() ||
      toolMode == omnibox::ToolMode::TOOL_MODE_AIM ||
      toolMode == omnibox::ToolMode::TOOL_MODE_UNSPECIFIED) {
    return NO;
  }
  return std::ranges::contains(_inputState->disabled_tools, toolMode);
}

// Whether the given `model` is allowed.
- (BOOL)isModelAllowed:(omnibox::ModelMode)model {
  // `MODEL_MODE_UNSPECIFIED` represents having no explicit model selection
  // (e.g. when resetting on return to regular search) and is always allowed,
  // even though it is not part of `_inputState->allowed_models`.
  if (model == omnibox::ModelMode::MODEL_MODE_UNSPECIFIED) {
    return YES;
  }

  // All models are gated by AI omnibox eligibility.
  if (![self isEligibleToAIM] || !_inputState.has_value()) {
    return NO;
  }

  return std::ranges::contains(_inputState->allowed_models, model);
}

// Whether the active tool's `ToolConfig` disables active model selection.
- (BOOL)isModelSelectionDisabledByActiveTool {
  const omnibox::ToolConfig* activeToolConfig =
      [self toolConfigForToolMode:self.activeToolMode];
  return activeToolConfig && activeToolConfig->disable_active_model_selection();
}

// Whether the given `model` is disabled.
- (BOOL)isModelDisabled:(omnibox::ModelMode)model {
  if (!_inputState.has_value()) {
    return NO;
  }

  return std::ranges::contains(_inputState->disabled_models, model);
}

// Returns the items to display in the model picker, in server order. Models
// without a label are excluded; selected and disabled models are kept.
- (NSArray<ComposeboxModelItem*>*)modelItems {
  if (!_inputState.has_value()) {
    return @[];
  }
  omnibox::ModelMode activeModel = self.activeModel;
  BOOL disableModelSelection = [self isModelSelectionDisabledByActiveTool];

  NSMutableArray<ComposeboxModelItem*>* items = [NSMutableArray array];
  for (const omnibox::ModelConfig& config : _inputState->model_configs) {
    omnibox::ModelMode model = config.model();
    if (![self isModelAllowed:model]) {
      continue;
    }
    if (config.menu_label().empty()) {
      continue;
    }
    NSString* title = base::SysUTF8ToNSString(config.menu_label());
    ComposeboxModelItem* item = [[ComposeboxModelItem alloc]
        initWithModelMode:model
                    title:title
                     icon:[self iconForModelConfig:config]
                 selected:!disableModelSelection && model == activeModel
                 disabled:disableModelSelection ||
                          [self isModelDisabled:model]];
    [items addObject:item];
  }
  return items;
}

// Returns the menu icon for the given `config`: the server icon if available,
// otherwise the local fallback.
- (UIImage*)iconForModelConfig:(const omnibox::ModelConfig&)config {
  if (config.has_icon() && config.icon().has_icon_id()) {
    if (UIImage* icon = ImageForIconResourceId(config.icon().icon_id(),
                                               kSymbolActionPointSize)) {
      return icon;
    }
  }
  return FallbackIconForModel(config.model());
}

#pragma mark - InputState Helpers

// Whether the composebox is currently in regular search.
- (BOOL)isRegularSearch {
  return self.activeToolMode == omnibox::ToolMode::TOOL_MODE_UNSPECIFIED;
}

// Sets the active model in input state model.
- (void)setActiveModelInInputState:(omnibox::ModelMode)model
                explicitUserAction:(BOOL)explicitUserAction {
  if (!_inputState.has_value()) {
    return;
  }
  // `InputStateModel` expects a concrete model from `allowed_models` (and
  // resets `MODEL_MODE_UNSPECIFIED` on config updates). Resolve
  // `MODEL_MODE_UNSPECIFIED` to the server's default model so compatibility
  // rules and query parameters are evaluated against the default model.
  omnibox::ModelMode requestedModelMode =
      model != omnibox::ModelMode::MODEL_MODE_UNSPECIFIED
          ? model
          : _inputState->GetDefaultModel();

  if (_inputState->active_model == requestedModelMode) {
    return;
  }

  _inputStateModel->SetActiveModel(requestedModelMode);

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

  // `InputStateModel` only models AIM state, where plain AI Mode is represented
  // as `TOOL_MODE_UNSPECIFIED`.
  omnibox::ToolMode inputStateTool =
      activeTool == omnibox::ToolMode::TOOL_MODE_AIM
          ? omnibox::ToolMode::TOOL_MODE_UNSPECIFIED
          : activeTool;

  if (_inputStateModel->GetInputState().active_tool == inputStateTool) {
    return;
  }

  contextual_search::ContextualSearchMetricsRecorder* recorder =
      _sessionHandle ? _sessionHandle->GetMetricsRecorder() : nullptr;
  if (recorder) {
    recorder->RecordToolMode(inputStateTool);
  }

  _inputStateModel->SetActiveTool(inputStateTool);
}

#pragma mark - Eligibility & Availability

/// Returns the default model for the active tool mode.
- (omnibox::ModelMode)defaultModel {
  if ([self isRegularSearch] || !_inputState.has_value()) {
    return omnibox::ModelMode::MODEL_MODE_UNSPECIFIED;
  }
  return _inputState->GetDefaultModel();
}

/// Returns the default tool mode implicit in the entrypoint (`TOOL_MODE_AIM`
/// in Cobrowse, `TOOL_MODE_UNSPECIFIED` otherwise).
- (omnibox::ToolMode)defaultToolMode {
  return _entrypoint == ComposeboxEntrypoint::kCobrowse
             ? omnibox::ToolMode::TOOL_MODE_AIM
             : omnibox::ToolMode::TOOL_MODE_UNSPECIFIED;
}

/// Whether the user is eligible to share content (enterprise policy).
- (BOOL)isContentSharingEnabled {
  return _prefService && _sessionHandle &&
         _sessionHandle->CheckSearchContentSharingSettings(_prefService);
}

/// Whether the user is eligible to upload PDFs.
- (BOOL)isEligibleToUploadPdf {
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

  return [self isEligibleToAIM];
}

#pragma mark - Attachment Rules

// The absolute value for the maximum number of attachments available.
- (NSUInteger)totalAttachmentLimit {
  if (!_inputState.has_value()) {
    return kAttachmentLimit;
  }
  return _inputState->max_total_inputs;
}

@end
