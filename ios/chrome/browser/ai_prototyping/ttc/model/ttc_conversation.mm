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
#import "ios/chrome/browser/ai_prototyping/ttc/model/audio/ttc_audio_controller.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/audio/ttc_audio_engine.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_error_codes.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_websocket_backend.h"

namespace {

// Extracts a `ttc::ErrorCode` from `error`, falling back to
// `ttc::ErrorCode::kAudioUnknownError` for non-TTC error domains.
ttc::ErrorCode ExtractTTCErrorCode(NSError* error) {
  if (error && [error.domain isEqualToString:kTTCErrorDomain]) {
    return static_cast<ttc::ErrorCode>(error.code);
  }
  return ttc::ErrorCode::kAudioUnknownError;
}

}  // namespace

// Objective-C bridge forwarding `TTCAudioControllerDelegate` callbacks to a C++
// `TtcConversation`.
@interface TTCAudioControllerDelegateBridge
    : NSObject <TTCAudioControllerDelegate>

- (instancetype)initWithConversation:(TtcConversation*)conversation
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

@implementation TTCAudioControllerDelegateBridge {
  base::WeakPtr<TtcConversation> _conversation;
}

- (instancetype)initWithConversation:(TtcConversation*)conversation {
  self = [super init];
  if (self) {
    _conversation = conversation->GetWeakPtr();
  }
  return self;
}

- (void)audioController:(id<TTCAudioController>)controller
    didCaptureAudioChunk:(NSData*)pcmData {
  if (_conversation) {
    _conversation->OnAudioChunkCaptured(pcmData);
  }
}

- (void)audioController:(id<TTCAudioController>)controller
    didUpdateInputEnergy:(float)energy {
  if (_conversation) {
    _conversation->OnInputEnergyUpdated(energy);
  }
}

- (void)audioController:(id<TTCAudioController>)controller
      didEncounterError:(NSError*)error {
  if (_conversation) {
    _conversation->OnAudioControllerError(error);
  }
}

- (void)audioControllerDidStopCapture:(id<TTCAudioController>)controller {
  if (_conversation) {
    _conversation->OnAudioControllerStoppedCapture();
  }
}

@end

TtcConversation::TtcConversation(id<TTCAudioController> audio_controller,
                                 std::unique_ptr<ttc::TtcBackend> backend)
    : audio_controller_(audio_controller), backend_(std::move(backend)) {
  CHECK(audio_controller_);
  audio_delegate_bridge_ =
      [[TTCAudioControllerDelegateBridge alloc] initWithConversation:this];
  audio_controller_.delegate = audio_delegate_bridge_;
}

TtcConversation::TtcConversation()
    : TtcConversation([[TTCAudioEngine alloc] init],
                      std::make_unique<TtcWebSocketBackend>()) {}

TtcConversation::~TtcConversation() {
  Disconnect();
}

#pragma mark - Public

void TtcConversation::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const uint64_t current_generation = ++session_generation_;

  if (backend_) {
    backend_->Connect(this);
  }
  // `Connect()` may synchronously fail and notify the delegate of a fatal
  // error, which calls `Stop()` and increments `session_generation_`. Bail out
  // instead of starting audio capture for an aborted session.
  if (session_generation_ != current_generation) {
    return;
  }

  base::WeakPtr<TtcConversation> weak_this = weak_ptr_factory_.GetWeakPtr();
  [audio_controller_ startCaptureWithCompletion:^(BOOL success,
                                                  NSError* error) {
    if (weak_this) {
      weak_this->HandleStartCaptureResult(success, error, current_generation);
    }
  }];
}

void TtcConversation::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_stopping_) {
    return;
  }
  is_stopping_ = true;

  // Invalidate any pending in-flight start completions.
  session_generation_++;

  [audio_controller_ stopCapture];
  [audio_controller_ stopPlayback];
  if (backend_) {
    backend_->Close();
  }

  is_stopping_ = false;
}

void TtcConversation::Disconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Stop();
  [audio_controller_ disconnect];
  audio_controller_.delegate = nil;
  audio_delegate_bridge_ = nil;
  delegate_ = nullptr;
  weak_ptr_factory_.InvalidateWeakPtrs();
}

#pragma mark - TTCAudioControllerDelegate

void TtcConversation::OnAudioChunkCaptured(NSData* pcm_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Disable barge-in: suppress microphone capture while response audio is
  // playing.
  if (audio_controller_.isPlaying || !backend_) {
    return;
  }

  base::span<const uint8_t> raw_bytes = base::apple::NSDataToSpan(pcm_data);
  if (raw_bytes.empty() || raw_bytes.size() % sizeof(int16_t) != 0) {
    return;
  }

  backend_->SendAudioChunk(
      base::subtle::reinterpret_span<const int16_t>(raw_bytes));
}

void TtcConversation::OnInputEnergyUpdated(float energy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (delegate_) {
    delegate_->OnAudioEnergyUpdated(energy);
  }
}

void TtcConversation::OnAudioControllerError(NSError* error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  HandleError(ExtractTTCErrorCode(error));
}

void TtcConversation::OnAudioControllerStoppedCapture() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Stop();
}

#pragma mark - Backend Observer Callbacks

void TtcConversation::OnBackendInitialized() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (delegate_) {
    delegate_->OnConversationInitialized();
  }
}

void TtcConversation::OnBackendClosed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (delegate_) {
    delegate_->OnConversationClosed();
  }
}

void TtcConversation::OnBackendError(ttc::ErrorCode error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  HandleError(error);
}

void TtcConversation::OnAudioOutput(base::span<const int16_t> audio_data,
                                    int64_t sequence_number) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (audio_data.empty()) {
    return;
  }
  base::span<const uint8_t> audio_bytes = base::as_byte_span(audio_data);
  NSData* pcm_data = [NSData dataWithBytes:audio_bytes.data()
                                    length:audio_bytes.size()];
  [audio_controller_ playStreamingAudioChunk:pcm_data];
}

void TtcConversation::OnGenerationStateChanged(bool started,
                                               bool completed,
                                               bool interrupted) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (interrupted) {
    [audio_controller_ clearPlaybackQueue];
    [audio_controller_ stopPlayback];
  }
}

void TtcConversation::OnTranscriptions(
    const std::string& input_transcription,
    const std::string& output_transcription) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Transcriptions will be forwarded to observers in a subsequent CL.
}

void TtcConversation::OnToolCall(const ttc::ToolRequest& tool_request,
                                 ttc::ToolResponseCallback response_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Tool call dispatch will be wired in a subsequent CL.
}

void TtcConversation::OnJournalEvent(
    const ttc::ServerJournalEvent& journal_event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Server journal events will be wired in a subsequent CL.
}

#pragma mark - Private

void TtcConversation::HandleStartCaptureResult(bool success,
                                               NSError* error,
                                               uint64_t generation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (session_generation_ != generation) {
    return;
  }

  if (!success) {
    HandleError(ExtractTTCErrorCode(error));
  }
}

void TtcConversation::HandleError(ttc::ErrorCode error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (delegate_) {
    delegate_->OnConversationError(error);
  }
}
