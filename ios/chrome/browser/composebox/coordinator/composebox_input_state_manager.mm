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
#import "components/contextual_search/footprints/public/drive_disclaimer_controller.h"
#import "components/contextual_search/footprints/public/fpop_service.h"
#import "components/contextual_search/pref_names.h"
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
#import "ios/chrome/browser/composebox/coordinator/composebox_mode_holder.h"
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
#import "services/network/public/cpp/shared_url_loader_factory.h"

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

// Returns the DriveConsentState corresponding to the given DisclaimerStatus.
contextual_search::DriveConsentState ConsentStateFromDisclaimerStatus(
    drive_picker::DriveDisclaimerController::DisclaimerStatus status) {
  using DisclaimerStatus =
      drive_picker::DriveDisclaimerController::DisclaimerStatus;
  using DriveConsentState = contextual_search::DriveConsentState;

  switch (status) {
    case DisclaimerStatus::kAccepted:
      return DriveConsentState::kConsent;
    case DisclaimerStatus::kNotAccepted:
      return DriveConsentState::kNotConsent;
    case DisclaimerStatus::kRestricted:
      return DriveConsentState::kRestricted;
  }
}

}  // namespace

@interface ComposeboxInputStateManager () <ComposeboxModeObserver,
                                           IdentityManagerObserving,
                                           SearchEngineObserving>
@end

@implementation ComposeboxInputStateManager {
  // The entrypoint from which the composebox was opened.
  ComposeboxEntrypoint _entrypoint;
  // The mode holder to read the active mode.
  ComposeboxModeHolder* _modeHolder;
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

  // Shared URL loader factory for network requests.
  scoped_refptr<network::SharedURLLoaderFactory> _urlLoaderFactory;
  // Controller for Drive privacy disclaimer verification.
  std::unique_ptr<drive_picker::DriveDisclaimerController>
      _driveDisclaimerController;

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
               modeHolder:(ComposeboxModeHolder*)modeHolder
              prefService:(PrefService*)prefService
    aimEligibilityService:(AimEligibilityService*)aimEligibilityService
          identityManager:(signin::IdentityManager*)identityManager
       templateURLService:(TemplateURLService*)templateURLService
            sessionHandle:
                (contextual_search::ContextualSearchSessionHandle*)sessionHandle
               entrypoint:(ComposeboxEntrypoint)entrypoint
              isIncognito:(BOOL)isIncognito {
  return [self initWithWebStateList:webStateList
                         modeHolder:modeHolder
                        prefService:prefService
              aimEligibilityService:aimEligibilityService
                    identityManager:identityManager
                 templateURLService:templateURLService
                      sessionHandle:sessionHandle
                         entrypoint:entrypoint
                        isIncognito:isIncognito
                   urlLoaderFactory:nullptr];
}

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
              isIncognito:(BOOL)isIncognito
         urlLoaderFactory:
             (scoped_refptr<network::SharedURLLoaderFactory>)urlLoaderFactory {
  self = [super init];
  if (self) {
    // Initialize with local fallback config. These will be overwritten
    // when server-side config becomes available via the input state model.
    _cachedUIConfig = [ComposeboxUIConfig localFallbackUIConfig];
    _webStateList = webStateList;
    _modeHolder = modeHolder;
    [_modeHolder addObserver:self];
    _prefService = prefService;
    _aimEligibilityService = aimEligibilityService;
    _identityManager = identityManager;
    if (_identityManager) {
      _identityManagerObserverBridge =
          std::make_unique<signin::IdentityManagerObserverBridge>(
              _identityManager, self);
    }
    _templateURLService = templateURLService;
    _urlLoaderFactory = urlLoaderFactory;
    // sessionHandle can be nil only in tests.
    if (sessionHandle) {
      _sessionHandle = sessionHandle->AsWeakPtr();
    }
    _entrypoint = entrypoint;
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
  [_modeHolder removeObserver:self];
  _modeHolder = nil;
  _searchEngineObserver.reset();
  _aimEligibilitySubscription = {};
  _metricsRecorder = nil;
  _driveDisclaimerController.reset();
  _urlLoaderFactory = nullptr;
}

- (drive_picker::DriveDisclaimerController*)driveDisclaimerController {
  if (_driveDisclaimerController) {
    return _driveDisclaimerController.get();
  }
  if (!_identityManager || !_urlLoaderFactory) {
    return nullptr;
  }
  // Create FpopService (Footprints One Platform Service) to communicate
  // with the Footprints backend for querying user privacy consent settings.
  auto fpopService = contextual_search::FpopService::Create(_identityManager,
                                                            _urlLoaderFactory);
  _driveDisclaimerController =
      std::make_unique<drive_picker::DriveDisclaimerController>(
          std::move(fpopService));
  return _driveDisclaimerController.get();
}

- (omnibox::ModelMode)activeModel {
  // `InputStateModel` keeps its default model active even in regular search,
  // where no model applies on the client.
  if ([self activeMode] == ComposeboxMode::kRegularSearch ||
      !_inputStateModel) {
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
  BOOL switchToAIM = explicitUserAction && _modeHolder.isRegularSearch;
  if (switchToAIM) {
    _modeHolder.mode = ComposeboxMode::kAIM;
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
  // Tool Mode has a different value depending on `items.hasImage`. Update if
  // needed.
  ComposeboxMode activeMode = [self activeMode];
  if (activeMode == ComposeboxMode::kImageGeneration) {
    [self setActiveToolInInputState:ToolModeForComposeboxMode(
                                        activeMode, self.items.hasImage)];
  }

  // AI mode is implicitly enabled by items attachment.
  BOOL shouldSwitchToAIM =
      self.items && !self.items.empty && _modeHolder.isRegularSearch;
  if (shouldSwitchToAIM) {
    _modeHolder.mode = ComposeboxMode::kAIM;
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

- (BOOL)isDSEGoogle {
  if (!_templateURLService) {
    return NO;
  }
  return search::DefaultSearchProviderIsGoogle(_templateURLService);
}

- (BOOL)canSelectTool:(ComposeboxMode)mode {
  return [self isToolAllowed:mode] && ![self isToolDisabled:mode];
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
    case signin::PrimaryAccountChangeEvent::Type::kSet: {
      // Immediately reset cached Drive consent state when switching accounts or
      // signing out to prevent leaking the previous account's consent state
      // while asynchronous FACS status queries are in flight.
      if (_prefService) {
        _prefService->SetInteger(
            contextual_search::kDriveConsentState,
            static_cast<int>(contextual_search::DriveConsentState::kNotReady));
      }
      [self updateSearchboxConfig];
      break;
    }
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      break;
  }
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
      /*is_signed_in=*/has_primary_account,
      /*browser_identity_matches_aim_identity=*/has_primary_account);
  if (_prefService) {
    _inputStateModel->SetPrefService(_prefService);
  }

  [self startInputStateObservation];
  // `Initialize` is synchronous and immediately notifies observers, updating
  // the state.
  _inputStateModel->Initialize();

  if (base::FeatureList::IsEnabled(
          omnibox::kComposeboxDriveContextMenuOption)) {
    if (base::FeatureList::IsEnabled(omnibox::kForceDriveDisclaimerAccepted)) {
      [self onDriveDisclaimerChecked:drive_picker::DriveDisclaimerController::
                                         DisclaimerStatus::kAccepted];
    } else if (auto* controller = [self driveDisclaimerController]) {
      __weak __typeof(self) weakSelf = self;
      controller->CheckDisclaimerStatusAsync(base::BindOnce(
          ^(drive_picker::DriveDisclaimerController::DisclaimerStatus status) {
            [weakSelf onDriveDisclaimerChecked:status];
          }));
    } else {
      [self onDriveDisclaimerChecked:drive_picker::DriveDisclaimerController::
                                         DisclaimerStatus::kRestricted];
    }
  }

  // iOS doesn't rely on the active hint text from `_inputState`, UI config only
  // changes when `searchboxConfig` is updated.
  _cachedUIConfig =
      ServerUIConfigFromInputState(_inputStateModel->GetInputState());

  [self.delegate inputStateManagerDidUpdateUIState:self];
}

- (void)onDriveDisclaimerChecked:
    (drive_picker::DriveDisclaimerController::DisclaimerStatus)status {
  if (!_prefService) {
    return;
  }
  _prefService->SetInteger(
      contextual_search::kDriveConsentState,
      static_cast<int>(ConsentStateFromDisclaimerStatus(status)));
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

// Returns `YES` if the given `model` matches `modelMode`.
- (BOOL)isModel:(omnibox::ModelMode)model
    matchingModelMode:(omnibox::ModelMode)modelMode {
  if ([self activeMode] == ComposeboxMode::kRegularSearch) {
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
  if (![self attachmentsAvailable]) {
    return NO;
  }

  // Enforce signed-in status and restriction check for the Drive option before
  // evaluating server-side or local rules.
  if (attachmentOption == ComposeboxAttachmentOption::kDrive) {
    BOOL hasPrimaryAccount =
        _identityManager &&
        _identityManager->HasPrimaryAccount(signin::ConsentLevel::kSignin);
    if (!hasPrimaryAccount) {
      return NO;
    }

    if (_prefService &&
        _prefService->GetInteger(contextual_search::kDriveConsentState) ==
            static_cast<int>(
                contextual_search::DriveConsentState::kRestricted)) {
      return NO;
    }
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

// Whether the given `model` is allowed.
- (BOOL)isModelAllowed:(omnibox::ModelMode)model {
  if ([ComposeboxUIInputState allToolsEnabled]) {
    return YES;
  }

  // `MODEL_MODE_UNSPECIFIED` represents having no explicit model selection
  // (e.g. when resetting on return to regular search) and is always allowed,
  // even though it is not part of `_inputState->allowed_models`.
  if (model == omnibox::ModelMode::MODEL_MODE_UNSPECIFIED) {
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

  return std::ranges::contains(_inputState->allowed_models, model);
}

// Whether the given `model` is disabled.
- (BOOL)isModelDisabled:(omnibox::ModelMode)model {
  if ([ComposeboxUIInputState allToolsEnabled]) {
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

  return std::ranges::contains(_inputState->disabled_models, model);
}

// Returns the items to display in the model picker, in server order. Models
// without a label are excluded; selected and disabled models are kept.
- (NSArray<ComposeboxModelItem*>*)modelItems {
  if (!_inputState.has_value()) {
    return @[];
  }
  omnibox::ModelMode activeModel = self.activeModel;

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
                 selected:model == activeModel
                 disabled:[self isModelDisabled:model]];
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

// Returns the current active mode from the mode holder.
- (ComposeboxMode)activeMode {
  return _modeHolder.mode;
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

  if (_inputStateModel->GetInputState().active_tool == activeTool) {
    return;
  }

  contextual_search::ContextualSearchMetricsRecorder* recorder =
      _sessionHandle ? _sessionHandle->GetMetricsRecorder() : nullptr;
  if (recorder) {
    recorder->RecordToolMode(activeTool);
  }

  _inputStateModel->SetActiveTool(activeTool);
}

#pragma mark - Eligibility & Availability

/// Returns the default model for the active mode.
- (omnibox::ModelMode)defaultModel {
  if ([self activeMode] == ComposeboxMode::kRegularSearch) {
    return omnibox::ModelMode::MODEL_MODE_UNSPECIFIED;
  }
  if (!_inputState.has_value()) {
    return omnibox::ModelMode::MODEL_MODE_UNSPECIFIED;
  }
  return _inputState->GetDefaultModel();
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
    case kDrive:
      return IsComposeboxDriveOptionEnabled() &&
             std::ranges::contains(_inputState->allowed_input_types,
                                   omnibox::INPUT_TYPE_DRIVE);
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
    case kDrive:
      return IsComposeboxDriveOptionEnabled() && [self isEligibleToUploadPdf];
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
    case kDrive:
      return IsComposeboxDriveOptionEnabled() &&
             std::ranges::contains(_inputState->disabled_input_types,
                                   omnibox::INPUT_TYPE_DRIVE);
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
    case kDrive:
      return IsComposeboxDriveOptionEnabled() && isImageCreationMode;
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
  return mode == ComposeboxMode::kImageGeneration && self.items.hasTabOrFile;
}

@end
