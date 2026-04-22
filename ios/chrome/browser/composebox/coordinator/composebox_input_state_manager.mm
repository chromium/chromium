// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/coordinator/composebox_input_state_manager.h"

#import "base/memory/raw_ptr.h"
#import "components/contextual_search/contextual_search_metrics_recorder.h"
#import "components/contextual_search/contextual_search_session_handle.h"
#import "components/omnibox/browser/aim_eligibility_service.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/composebox/public/features.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"

@implementation ComposeboxInputStateManager {
  // The entrypoint from which the composebox was opened.
  ComposeboxEntrypoint _entrypoint;

  // The web state list of the browser.
  raw_ptr<WebStateList> _webStateList;
  // Pref service for user preferences.
  raw_ptr<PrefService> _prefService;
  // Service to check AIM eligibility.
  raw_ptr<AimEligibilityService> _aimEligibilityService;
  // Identity manager for checking account status.
  raw_ptr<signin::IdentityManager> _identityManager;
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
}

#pragma mark - Public

- (instancetype)
     initWithWebStateList:(WebStateList*)webStateList
              prefService:(PrefService*)prefService
    aimEligibilityService:(AimEligibilityService*)aimEligibilityService
          identityManager:(signin::IdentityManager*)identityManager
            sessionHandle:
                (contextual_search::ContextualSearchSessionHandle*)sessionHandle
               entrypoint:(ComposeboxEntrypoint)entrypoint
              isIncognito:(BOOL)isIncognito {
  self = [super init];
  if (self) {
    _webStateList = webStateList;
    _prefService = prefService;
    _aimEligibilityService = aimEligibilityService;
    _identityManager = identityManager;
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
  _inputStateSubscription = {};
  _inputStateModel.reset();
  _webStateList = nullptr;
  _prefService = nullptr;
  _aimEligibilityService = nullptr;
  _identityManager = nullptr;
  _sessionHandle.reset();
}

- (omnibox::ToolMode)activeTool {
  if (_inputStateModel) {
    return _inputStateModel->GetInputState().active_tool;
  }
  return omnibox::TOOL_MODE_UNSPECIFIED;
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
  [self.delegate inputStateManager:self didUpdateInputState:inputState];
}

#pragma mark InputState rules helpers

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

// Whether the given model mode is selectable.
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

@end
