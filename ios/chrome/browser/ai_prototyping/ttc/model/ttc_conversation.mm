// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_conversation.h"

#import <stdint.h>

#import <memory>
#import <string>
#import <utility>

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/containers/span.h"
#import "base/sequence_checker.h"
#import "components/ttc/app/public/error_codes.h"
#import "components/ttc/app/public/server_journal_event.h"
#import "components/ttc/app/public/tool_types.h"
#import "components/ttc/app/ttc_backend.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/audio/ttc_audio_controller.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/audio/ttc_audio_engine.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_conversation_delegate.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_error_codes.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_websocket_backend.h"

@interface TTCConversation () <TTCAudioControllerDelegate>

// Forwarded `ttc::TtcBackend::Observer` callbacks.
- (void)onBackendInitialized;
- (void)onBackendClosed;
- (void)onBackendError:(ttc::ErrorCode)errorCode;
- (void)onAudioOutput:(base::span<const int16_t>)audioData
       sequenceNumber:(int64_t)sequenceNumber;
- (void)onGenerationStateChangedStarted:(BOOL)started
                              completed:(BOOL)completed
                            interrupted:(BOOL)interrupted;
- (void)onTranscriptionsWithInput:(const std::string&)inputTranscription
                           output:(const std::string&)outputTranscription;

@end

namespace {

// Bridge forwarding C++ `ttc::TtcBackend::Observer` events to
// `TTCConversation`.
class TtcBackendObserverBridge : public ttc::TtcBackend::Observer {
 public:
  explicit TtcBackendObserverBridge(TTCConversation* conversation)
      : conversation_(conversation) {}
  ~TtcBackendObserverBridge() override = default;

  // `ttc::TtcBackend::Observer` implementation:
  void OnBackendInitialized() override { [conversation_ onBackendInitialized]; }

  void OnBackendClosed() override { [conversation_ onBackendClosed]; }

  void OnBackendError(ttc::ErrorCode error) override {
    [conversation_ onBackendError:error];
  }

  void OnTranscriptions(const std::string& input_transcription,
                        const std::string& output_transcription) override {
    [conversation_ onTranscriptionsWithInput:input_transcription
                                      output:output_transcription];
  }

  void OnAudioOutput(base::span<const int16_t> audio_data,
                     int64_t sequence_number) override {
    [conversation_ onAudioOutput:audio_data sequenceNumber:sequence_number];
  }

  void OnGenerationStateChanged(bool started,
                                bool completed,
                                bool interrupted) override {
    [conversation_ onGenerationStateChangedStarted:started
                                         completed:completed
                                       interrupted:interrupted];
  }

  void OnToolCall(const ttc::ToolRequest& tool_request,
                  ttc::ToolResponseCallback response_callback) override {
    // Tool call dispatch will be wired in a subsequent CL.
  }

  void OnJournalEvent(const ttc::ServerJournalEvent& journal_event) override {
    // Server journal events will be wired in a subsequent CL.
  }

 private:
  __weak TTCConversation* conversation_ = nil;
};

}  // namespace

@implementation TTCConversation {
  SEQUENCE_CHECKER(_sequenceChecker);

  // Observer bridge registered with `_backend`. Declared before `_backend` so
  // `_backend` is destroyed first during deallocation while the bridge is still
  // alive.
  std::unique_ptr<TtcBackendObserverBridge> _backendObserverBridge;

  // Owned C++ backend instance.
  std::unique_ptr<ttc::TtcBackend> _backend;

  // Generation counter incremented in `stop` to invalidate pending in-flight
  // async start or permission callbacks.
  uint64_t _sessionGeneration;

  // Guard flag preventing re-entrant stop invocations.
  BOOL _isStopping;
}

- (instancetype)initWithAudioController:(id<TTCAudioController>)audioController
                                backend:
                                    (std::unique_ptr<ttc::TtcBackend>)backend {
  CHECK(audioController);
  self = [super init];
  if (self) {
    _audioController = audioController;
    _audioController.delegate = self;
    _backendObserverBridge = std::make_unique<TtcBackendObserverBridge>(self);
    _backend = std::move(backend);
    _sessionGeneration = 0;
    _isStopping = NO;
  }
  return self;
}

- (instancetype)init {
  return [self initWithAudioController:[[TTCAudioEngine alloc] init]
                               backend:std::make_unique<TtcWebSocketBackend>()];
}

- (ttc::TtcBackend*)backend {
  return _backend.get();
}

#pragma mark - Public

- (void)start {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  const uint64_t currentGeneration = ++_sessionGeneration;

  if (_backend) {
    _backend->Connect(_backendObserverBridge.get());
  }
  // `Connect()` may synchronously fail and notify the delegate of a fatal
  // error, which calls `stop` and increments `_sessionGeneration`. Bail out
  // instead of starting audio capture for an aborted session.
  if (_sessionGeneration != currentGeneration) {
    return;
  }

  __weak __typeof(self) weakSelf = self;
  [_audioController startCaptureWithCompletion:^(BOOL success, NSError* error) {
    [weakSelf handleStartCaptureSuccess:success
                                  error:error
                             generation:currentGeneration];
  }];
}

- (void)stop {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_isStopping) {
    return;
  }
  _isStopping = YES;

  // Invalidate any pending in-flight start completions.
  _sessionGeneration++;

  [_audioController stopCapture];
  [_audioController stopPlayback];
  if (_backend) {
    _backend->Close();
  }

  _isStopping = NO;
}

- (void)disconnect {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [self stop];
  [_audioController disconnect];
  _audioController.delegate = nil;
  self.delegate = nil;
}

#pragma mark - TTCAudioControllerDelegate

- (void)audioController:(id<TTCAudioController>)controller
    didCaptureAudioChunk:(NSData*)pcmData {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  // Disable barge-in: suppress microphone capture while response audio is
  // playing.
  if (_audioController.isPlaying || !_backend) {
    return;
  }

  base::span<const uint8_t> rawBytes = base::apple::NSDataToSpan(pcmData);
  if (rawBytes.empty() || rawBytes.size() % sizeof(int16_t) != 0) {
    return;
  }

  _backend->SendAudioChunk(
      base::subtle::reinterpret_span<const int16_t>(rawBytes));
}

- (void)audioController:(id<TTCAudioController>)controller
    didUpdateInputEnergy:(float)energy {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [self.delegate conversation:self didUpdateAudioEnergy:energy];
}

- (void)audioController:(id<TTCAudioController>)controller
      didEncounterError:(NSError*)error {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [self handleError:error];
}

- (void)audioControllerDidStopCapture:(id<TTCAudioController>)controller {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [self stop];
}

#pragma mark - Backend Observer Callbacks

- (void)onBackendInitialized {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [self.delegate conversationDidInitialize:self];
}

- (void)onBackendClosed {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [self.delegate conversationDidClose:self];
}

- (void)onBackendError:(ttc::ErrorCode)errorCode {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [self handleError:CreateTTCError(errorCode)];
}

- (void)onAudioOutput:(base::span<const int16_t>)audioData
       sequenceNumber:(int64_t)sequenceNumber {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (audioData.empty()) {
    return;
  }
  base::span<const uint8_t> audioBytes = base::as_byte_span(audioData);
  NSData* pcmData = [NSData dataWithBytes:audioBytes.data()
                                   length:audioBytes.size()];
  [_audioController playStreamingAudioChunk:pcmData];
}

- (void)onGenerationStateChangedStarted:(BOOL)started
                              completed:(BOOL)completed
                            interrupted:(BOOL)interrupted {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (interrupted) {
    [_audioController clearPlaybackQueue];
    [_audioController stopPlayback];
  }
}

- (void)onTranscriptionsWithInput:(const std::string&)inputTranscription
                           output:(const std::string&)outputTranscription {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  // Transcriptions will be forwarded to observers in a subsequent CL.
}

#pragma mark - Private

- (void)handleStartCaptureSuccess:(BOOL)success
                            error:(NSError*)error
                       generation:(uint64_t)generation {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_sessionGeneration != generation) {
    return;
  }

  if (!success) {
    if (!error) {
      error = CreateTTCError(ttc::ErrorCode::kAudioUnknownError);
    }
    [self handleError:error];
  }
}

- (void)handleError:(NSError*)error {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [self.delegate conversation:self didEncounterError:error];
}

@end
