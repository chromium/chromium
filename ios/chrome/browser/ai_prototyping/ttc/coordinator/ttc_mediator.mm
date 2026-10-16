// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/coordinator/ttc_mediator.h"

#import <memory>
#import <optional>

#import "base/callback_list.h"
#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/to_string.h"
#import "components/ttc/app/public/error_codes.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_journal.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_keyed_service.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_session_controller.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_session_controller_observer_bridge.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_states.h"
#import "ios/chrome/browser/ai_prototyping/ttc/ui/ttc_consumer.h"

namespace {

// Error message displayed when the TTC service is unavailable.
NSString* const kServiceUnavailableError = @"TTC service is unavailable";

// Default error message displayed when a session error occurs without a
// specific description.
NSString* const kDefaultSessionError = @"Session error occurred";

// Converts a `TTCSessionLifecycle` to the corresponding `TTCSessionUIState`.
TTCSessionUIState SessionUIStateForLifecycle(TTCSessionLifecycle lifecycle) {
  switch (lifecycle) {
    case TTCSessionLifecycle::kInitializing:
      return TTCSessionUIState::kConnecting;
    case TTCSessionLifecycle::kLive:
      return TTCSessionUIState::kListening;
    case TTCSessionLifecycle::kFinished:
      return TTCSessionUIState::kIdle;
  }
}

// Converts a `TTCJournalFilter` to an optional `TTCJournalComponent`.
std::optional<TTCJournalComponent> JournalComponentForFilter(
    TTCJournalFilter filter) {
  switch (filter) {
    case TTCJournalFilter::kAll:
      return std::nullopt;
    case TTCJournalFilter::kSession:
      return TTCJournalComponent::kSession;
    case TTCJournalFilter::kConversation:
      return TTCJournalComponent::kConversation;
    case TTCJournalFilter::kBackend:
      return TTCJournalComponent::kBackend;
    case TTCJournalFilter::kAudio:
      return TTCJournalComponent::kAudio;
    case TTCJournalFilter::kActuation:
      return TTCJournalComponent::kActuation;
  }
}

}  // namespace

@interface TTCMediator () <TTCSessionControllerObserver>
@end

@implementation TTCMediator {
  // Current voice session lifecycle state (Idle, Connecting, Listening, Error).
  TTCSessionUIState _currentState;

  // TTC keyed service for the profile.
  raw_ptr<TTCKeyedService> _ttcService;

  // Subscription observing state changes in TTCKeyedService.
  base::CallbackListSubscription _stateChangeSubscription;

  // Subscription observing entries added to TTCJournal.
  base::CallbackListSubscription _journalEntryAddedSubscription;

  // Subscription observing entries updated in-place in TTCJournal.
  base::CallbackListSubscription _journalEntryUpdatedSubscription;

  // Subscription observing when TTCJournal is cleared.
  base::CallbackListSubscription _journalClearedSubscription;

  // Optional component filter applied to displayed journal lines.
  std::optional<TTCJournalComponent> _journalComponentFilter;

  // Whether the journal displays full detailed telemetry (`YES`) or simplified
  // human-readable lines. Defaults to NO.
  BOOL _journalDetailedMode;

  // Bridge observing `TtcSessionController`.
  std::unique_ptr<TtcSessionControllerObserverBridge>
      _sessionControllerObserverBridge;

  // Weak reference to the currently observed session controller.
  base::WeakPtr<TtcSessionController> _observedSessionController;
}

#pragma mark - Initialization

- (instancetype)initWithTTCService:(TTCKeyedService*)ttcService {
  self = [super init];
  if (self) {
    _currentState = TTCSessionUIState::kIdle;
    _ttcService = ttcService;
    _journalDetailedMode = NO;
    _sessionControllerObserverBridge =
        std::make_unique<TtcSessionControllerObserverBridge>(self);
    if (_ttcService) {
      __weak TTCMediator* weakSelf = self;
      _stateChangeSubscription = _ttcService->RegisterStateChangedCallback(
          base::BindRepeating(^(TTCServiceState state) {
            [weakSelf handleServiceStateChanged:state];
          }));
      _journalEntryAddedSubscription =
          _ttcService->journal().AddEntryAddedListener(
              base::BindRepeating(^(const TTCJournalEntry& entry) {
                [weakSelf handleJournalEntryAdded:entry];
              }));
      _journalEntryUpdatedSubscription =
          _ttcService->journal().AddEntryUpdatedListener(
              base::BindRepeating(^(const TTCJournalEntry& entry) {
                [weakSelf handleJournalEntryUpdated:entry];
              }));
      _journalClearedSubscription =
          _ttcService->journal().AddClearedListener(base::BindRepeating(^{
            [weakSelf handleJournalCleared];
          }));
      [self startObservingSessionControllerIfNeeded];
    }
  }
  return self;
}

#pragma mark - Setters

- (void)setConsumer:(id<TTCConsumer>)consumer {
  _consumer = consumer;
  if (_consumer) {
    [self hydrateConsumer];
  }
}

#pragma mark - TTCMutator

- (void)startSession {
  if (!_ttcService || !_ttcService->IsEnabled()) {
    _currentState = TTCSessionUIState::kError;
    [self.consumer setSessionState:_currentState];
    [self.consumer didEncounterError:kServiceUnavailableError];
    return;
  }

  if (_observedSessionController || _ttcService->is_session_active()) {
    return;
  }

  _ttcService->StartSession();
  [self startObservingSessionControllerIfNeeded];
}

- (void)stopSession {
  if (_ttcService && _ttcService->is_session_active()) {
    _ttcService->EndSession();
  }
}

- (void)setLoopbackEnabled:(BOOL)enabled {
  [self.consumer setLoopbackEnabled:enabled];
}

- (void)playTestAudio {
  [self.consumer setTestAudioPlaying:NO];
}

- (void)stopTestAudio {
  [self.consumer setTestAudioPlaying:NO];
}

- (void)viewWillAppear {
  [self hydrateConsumer];
}

- (void)clearJournal {
  if (_ttcService) {
    _ttcService->journal().Clear();
  }
}

- (void)setJournalComponentFilter:(TTCJournalFilter)filter {
  _journalComponentFilter = JournalComponentForFilter(filter);
  [self.consumer setJournalLines:[self formattedJournalLines]];
}

- (void)setJournalDetailedMode:(BOOL)detailed {
  _journalDetailedMode = detailed;
  [self.consumer setJournalLines:[self formattedJournalLines]];
}

#pragma mark - TTCSessionControllerObserver

- (void)sessionController:(TtcSessionController*)controller
       didChangeLifecycle:(TTCSessionLifecycle)lifecycle {
  if (lifecycle == TTCSessionLifecycle::kFinished) {
    [self stopObservingSessionControllerIfNeeded];
    if (_currentState == TTCSessionUIState::kError) {
      [self.consumer setMicEnergyLevel:0.0f];
      return;
    }
  }
  [self updateConsumerForLifecycle:lifecycle];
}

- (void)sessionController:(TtcSessionController*)controller
      didUpdateAudioLevel:(float)audioLevel {
  if (_currentState != TTCSessionUIState::kListening) {
    return;
  }
  [self.consumer setMicEnergyLevel:audioLevel];
}

- (void)sessionController:(TtcSessionController*)controller
         didFailWithError:(ttc::ErrorCode)error {
  _currentState = TTCSessionUIState::kError;
  [self.consumer setSessionState:_currentState];
  NSString* errorMessage = base::SysUTF8ToNSString(base::ToString(error));
  [self.consumer didEncounterError:errorMessage.length > 0
                                       ? errorMessage
                                       : kDefaultSessionError];
}

#pragma mark - Public

- (void)disconnect {
  [self stopObservingSessionControllerIfNeeded];
  _sessionControllerObserverBridge.reset();
  _stateChangeSubscription = {};
  _journalEntryAddedSubscription = {};
  _journalEntryUpdatedSubscription = {};
  _journalClearedSubscription = {};
  _ttcService = nullptr;
  _consumer = nil;
}

#pragma mark - Private

// Starts observing the active session controller if not already observing.
- (void)startObservingSessionControllerIfNeeded {
  if (_observedSessionController || !_ttcService ||
      !_ttcService->session_controller() || !_sessionControllerObserverBridge) {
    return;
  }
  _observedSessionController = _ttcService->session_controller()->GetWeakPtr();
  _observedSessionController->AddObserver(
      _sessionControllerObserverBridge.get());
  [self updateConsumerForLifecycle:_observedSessionController->lifecycle()];
}

// Stops observing the active session controller if currently observing.
- (void)stopObservingSessionControllerIfNeeded {
  if (_observedSessionController && _sessionControllerObserverBridge) {
    _observedSessionController->RemoveObserver(
        _sessionControllerObserverBridge.get());
  }
  _observedSessionController.reset();
}

// Updates `_currentState` and notifies the consumer based on the session
// controller lifecycle.
- (void)updateConsumerForLifecycle:(TTCSessionLifecycle)lifecycle {
  _currentState = SessionUIStateForLifecycle(lifecycle);
  [self.consumer setSessionState:_currentState];
  if (_currentState == TTCSessionUIState::kIdle) {
    [self.consumer setMicEnergyLevel:0.0f];
  }
}

// Handles service state changes dispatched by `TTCKeyedService`.
- (void)handleServiceStateChanged:(TTCServiceState)state {
  if (state == TTCServiceState::kSessionActive) {
    [self startObservingSessionControllerIfNeeded];
  } else if (state == TTCServiceState::kSessionInactive) {
    [self stopObservingSessionControllerIfNeeded];
    if (_currentState != TTCSessionUIState::kError) {
      _currentState = TTCSessionUIState::kIdle;
      [self.consumer setSessionState:TTCSessionUIState::kIdle];
      [self.consumer setMicEnergyLevel:0.0f];
    }
  }
}

// Formats a single `TTCJournalEntry` according to `_journalDetailedMode`,
// returning `nil` if the entry should be omitted in simplified mode.
- (NSString*)formatJournalEntry:(const TTCJournalEntry&)entry {
  std::string formatted =
      _journalDetailedMode ? entry.FormatLine() : entry.FormatSimplifiedLine();
  if (formatted.empty()) {
    return nil;
  }
  return base::SysUTF8ToNSString(formatted);
}

// Handles a new entry appended to `TTCJournal`.
- (void)handleJournalEntryAdded:(const TTCJournalEntry&)entry {
  if (_journalComponentFilter.has_value() &&
      entry.component != *_journalComponentFilter) {
    return;
  }
  if (NSString* line = [self formatJournalEntry:entry]) {
    [self.consumer appendJournalLine:line];
  }
}

// Handles an existing entry updated in-place in `TTCJournal`.
- (void)handleJournalEntryUpdated:(const TTCJournalEntry&)entry {
  if (_journalComponentFilter.has_value() &&
      entry.component != *_journalComponentFilter) {
    return;
  }
  [self.consumer setJournalLines:[self formattedJournalLines]];
}

// Handles `TTCJournal::Clear()` notifications.
- (void)handleJournalCleared {
  [self.consumer setJournalLines:@[]];
}

// Returns buffered journal lines matching `_journalComponentFilter`.
- (NSArray<NSString*>*)formattedJournalLines {
  if (!_ttcService) {
    return @[];
  }
  NSMutableArray<NSString*>* lines = [[NSMutableArray alloc] init];
  for (const TTCJournalEntry& entry : _ttcService->journal().entries()) {
    if (_journalComponentFilter.has_value() &&
        entry.component != *_journalComponentFilter) {
      continue;
    }
    if (NSString* line = [self formatJournalEntry:entry]) {
      [lines addObject:line];
    }
  }
  return lines;
}

// Hydrates the consumer with current state and resets audio energy.
- (void)hydrateConsumer {
  if (!self.consumer) {
    return;
  }
  [self.consumer setSessionState:_currentState];
  [self.consumer setMicEnergyLevel:0.0f];
  [self.consumer setLoopbackEnabled:NO];
  [self.consumer setTestAudioPlaying:NO];
  [self.consumer setJournalLines:[self formattedJournalLines]];
}

@end
