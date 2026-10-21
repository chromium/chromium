// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_conversation.h"

#import <inttypes.h>
#import <stddef.h>
#import <stdint.h>

#import <algorithm>
#import <memory>
#import <string>
#import <utility>
#import <vector>

#import "base/apple/foundation_util.h"
#import "base/byte_size.h"
#import "base/check.h"
#import "base/containers/span.h"
#import "base/strings/strcat.h"
#import "base/strings/string_util.h"
#import "base/strings/stringprintf.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/audio/ttc_audio_controller.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/audio/ttc_audio_session_controller.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_error_codes.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_journal.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_websocket_backend.h"
#import "url/gurl.h"

namespace {

// Extracts a `ttc::ErrorCode` from `error`, falling back to
// `ttc::ErrorCode::kAudioUnknownError` for non-TTC error domains.
ttc::ErrorCode ExtractTTCErrorCode(NSError* error) {
  if (error && [error.domain isEqualToString:kTTCErrorDomain]) {
    return static_cast<ttc::ErrorCode>(error.code);
  }
  return ttc::ErrorCode::kAudioUnknownError;
}

// Formats a simplified summary line for `chunks` upstream audio chunks sent in
// a burst representing `bytes` total audio data.
std::string FormatAudioSentSummary(size_t chunks, size_t bytes) {
  return base::StringPrintf("Sent audio: %zu %s (%" PRIu64 " KiB)", chunks,
                            chunks == 1 ? "chunk" : "chunks",
                            base::ByteSize(bytes).InKiB());
}

// Appends `chunk` to `accumulated` and logs or updates the coalesced
// `kConversation` transcription entry in `journal`.
void LogOrUpdateTranscriptionEntry(TTCJournal* journal,
                                   const std::string& chunk,
                                   const std::string& event_name,
                                   const std::string& prefix,
                                   std::string& accumulated,
                                   size_t& chunks,
                                   uint64_t& entry_id) {
  if (!journal) {
    return;
  }
  accumulated.append(chunk);
  chunks++;
  std::vector<TTCJournalDetail> details =
      TTCJournalDetailsBuilder()
          .SetSummary(base::StrCat({prefix, accumulated}))
          .Add("text", accumulated)
          .Add("chunks", chunks)
          .Add("length", accumulated.size())
          .Build();
  if (entry_id == 0 || !journal->UpdateEntry(entry_id, details)) {
    entry_id = journal->Log(TTCJournalComponent::kConversation, event_name,
                            std::move(details));
  }
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
    : TtcConversation([[TTCAudioSessionController alloc] init],
                      std::make_unique<TtcWebSocketBackend>()) {}

TtcConversation::~TtcConversation() {
  Disconnect();
}

TTCJournal* TtcConversation::journal() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return journal_.get();
}

void TtcConversation::set_journal(TTCJournal* journal) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  journal_ = journal ? journal->GetWeakPtr() : nullptr;
  audio_controller_.journal = journal;
}

#pragma mark - Public

void TtcConversation::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const uint64_t current_generation = ++session_generation_;
  ResetTurnJournalState();

  if (backend_) {
    LogBackendSetupStarted();
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
  ResetTurnJournalState();

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
  audio_controller_.journal = nullptr;
  audio_controller_.delegate = nil;
  audio_delegate_bridge_ = nil;
  delegate_ = nullptr;
  journal_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void TtcConversation::SendToolSetUpdate(
    const std::vector<ttc::ToolDefinition>& tool_definitions) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (backend_) {
    LogToolDefinitionsSent(tool_definitions);
    backend_->SendToolSetUpdate(tool_definitions);
  }
}

void TtcConversation::SendContextUpdate(
    const GURL& url,
    const std::string& title,
    const optimization_guide::proto::AnnotatedPageContent& apc) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (backend_) {
    LogContextUpdateSent(url, title, apc);
    backend_->SendContextUpdate(url, title, apc);
  }
}

#pragma mark - TTCAudioControllerDelegate

void TtcConversation::OnAudioChunkCaptured(NSData* pcm_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!backend_) {
    return;
  }

  base::span<const uint8_t> raw_bytes = base::apple::NSDataToSpan(pcm_data);
  if (raw_bytes.empty() || raw_bytes.size() % sizeof(int16_t) != 0) {
    return;
  }

  // Disable barge-in: suppress microphone capture while response audio is
  // playing.
  if (audio_controller_.isPlaying) {
    LogAudioInputSuppressed();
    return;
  }
  logged_playback_suppression_ = false;

  if (!backend_->is_transport_connected()) {
    LogAudioInputDropped();
    return;
  }

  RecordAndLogAudioChunkSent(raw_bytes.size());
  backend_->SendAudioChunk(
      base::subtle::reinterpret_span<const int16_t>(raw_bytes));
}

void TtcConversation::OnInputEnergyUpdated(float energy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (backend_ && !audio_controller_.isPlaying &&
      backend_->is_transport_connected()) {
    peak_input_energy_ = std::max(peak_input_energy_, energy);
  }
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
  LogBackendInitialized();
  if (delegate_) {
    delegate_->OnConversationInitialized();
  }
}

void TtcConversation::OnBackendClosed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LogBackendClosed();
  if (delegate_) {
    delegate_->OnConversationClosed();
  }
}

void TtcConversation::OnBackendError(ttc::ErrorCode error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LogBackendError(error);
  HandleError(error);
}

void TtcConversation::OnAudioOutput(base::span<const int16_t> audio_data,
                                    int64_t sequence_number) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (audio_data.empty()) {
    return;
  }
  FlushSentAudioJournalState();
  base::span<const uint8_t> audio_bytes = base::as_byte_span(audio_data);
  NSData* pcm_data = [NSData dataWithBytes:audio_bytes.data()
                                    length:audio_bytes.size()];
  [audio_controller_ playStreamingAudioChunk:pcm_data];
}

void TtcConversation::OnGenerationStateChanged(bool started,
                                               bool completed,
                                               bool interrupted) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LogGenerationStateChanged(started, completed, interrupted);
  if (completed || interrupted) {
    ResetTurnJournalState();
  }
  if (interrupted) {
    [audio_controller_ clearPlaybackQueue];
    [audio_controller_ stopPlayback];
  }
}

void TtcConversation::OnTranscriptions(
    const std::string& input_transcription,
    const std::string& output_transcription) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!input_transcription.empty()) {
    LogOrUpdateInputTranscription(input_transcription);
  }
  if (!output_transcription.empty()) {
    LogOrUpdateOutputTranscription(output_transcription);
  }
  // Transcriptions will be forwarded to observers in a subsequent CL.
}

void TtcConversation::OnToolCall(const ttc::ToolRequest& tool_request,
                                 ttc::ToolResponseCallback response_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LogToolCallReceived(tool_request.name);
  // Tool call dispatch will be wired in a subsequent CL.
}

void TtcConversation::OnJournalEvent(
    const ttc::ServerJournalEvent& journal_event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (journal_) {
    journal_->HandleServerJournalEvent(journal_event);
  }
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

#pragma mark - Journal Logging

void TtcConversation::LogBackendSetupStarted() {
  if (!journal_) {
    return;
  }
  journal_->Log(TTCJournalComponent::kBackend, "TtcBackendSetupStarted",
                TTCJournalDetailsBuilder()
                    .SetSummary("Backend setup started...")
                    .Build());
}

void TtcConversation::LogBackendInitialized() {
  logged_disconnected_drop_ = false;
  if (!journal_) {
    return;
  }
  journal_->Log(
      TTCJournalComponent::kBackend, "TtcBackendInitialized",
      TTCJournalDetailsBuilder().SetSummary("Backend setup completed").Build());
}

void TtcConversation::LogBackendClosed() {
  FlushSentAudioJournalState();
  if (!journal_) {
    return;
  }
  journal_->Log(
      TTCJournalComponent::kBackend, "TtcBackendClosed",
      TTCJournalDetailsBuilder().SetSummary("Backend closed").Build());
}

void TtcConversation::LogBackendError(ttc::ErrorCode error) {
  FlushSentAudioJournalState();
  if (!journal_) {
    return;
  }
  const int error_code = static_cast<int>(error);
  journal_->LogError(
      TTCJournalComponent::kBackend, "TtcBackendError",
      TTCJournalDetailsBuilder()
          .SetSummary(base::StringPrintf("Backend error (code=%d)", error_code))
          .AddError(error)
          .Add("error_code", error_code)
          .Build());
}

void TtcConversation::LogAudioInputSuppressed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  FlushSentAudioJournalState();
  if (logged_playback_suppression_) {
    return;
  }
  logged_playback_suppression_ = true;
  if (!journal_) {
    return;
  }
  journal_->Log(TTCJournalComponent::kAudio, "AudioInputSuppressed",
                TTCJournalDetailsBuilder()
                    .SetSummary("Audio input suppressed (playback active)")
                    .Add("reason", "playback_active")
                    .Build());
}

void TtcConversation::LogAudioInputDropped() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  FlushSentAudioJournalState();
  if (logged_disconnected_drop_) {
    return;
  }
  logged_disconnected_drop_ = true;
  if (!journal_) {
    return;
  }
  journal_->LogError(TTCJournalComponent::kAudio, "AudioInputDropped",
                     TTCJournalDetailsBuilder()
                         .SetSummary("Audio input dropped (backend not ready)")
                         .Add("reason", "backend_not_connected")
                         .Build());
}

void TtcConversation::RecordAndLogAudioChunkSent(size_t byte_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  logged_disconnected_drop_ = false;
  const bool is_first_chunk = (sent_audio_chunks_ == 0);
  sent_audio_chunks_++;
  sent_audio_bytes_ += byte_size;
  if (is_first_chunk && journal_) {
    journal_->Log(TTCJournalComponent::kAudio, "AudioInputStarted",
                  TTCJournalDetailsBuilder()
                      .SetSummary("Sending audio to server...")
                      .Build());
  }
}

void TtcConversation::FlushSentAudioJournalState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (sent_audio_chunks_ == 0) {
    peak_input_energy_ = 0.0f;
    return;
  }
  if (journal_) {
    journal_->Log(
        TTCJournalComponent::kAudio, "AudioInputSent",
        TTCJournalDetailsBuilder()
            .SetSummary(
                FormatAudioSentSummary(sent_audio_chunks_, sent_audio_bytes_))
            .Add("chunks", sent_audio_chunks_)
            .Add("bytes", sent_audio_bytes_)
            .Add("peak_energy", base::StringPrintf("%.2f", peak_input_energy_))
            .Build());
  }
  sent_audio_chunks_ = 0;
  sent_audio_bytes_ = 0;
  peak_input_energy_ = 0.0f;
}

void TtcConversation::LogGenerationStateChanged(bool started,
                                                bool completed,
                                                bool interrupted) {
  if (completed || interrupted) {
    FlushSentAudioJournalState();
  }
  if (!journal_) {
    return;
  }
  TTCJournalDetailsBuilder details;
  if (interrupted) {
    details.SetSummary("Generation interrupted");
  }
  details.Add("started", started)
      .Add("completed", completed)
      .Add("interrupted", interrupted);
  journal_->Log(TTCJournalComponent::kConversation, "TtcGenerationStateChanged",
                std::move(details).Build());
}

void TtcConversation::LogOrUpdateInputTranscription(const std::string& chunk) {
  LogOrUpdateTranscriptionEntry(journal_.get(), chunk, "TtcInputTranscription",
                                "Input: ", input_transcription_.text,
                                input_transcription_.chunks,
                                input_transcription_.entry_id);
}

void TtcConversation::LogOrUpdateOutputTranscription(const std::string& chunk) {
  FlushSentAudioJournalState();
  LogOrUpdateTranscriptionEntry(journal_.get(), chunk, "TtcOutputTranscription",
                                "Output: ", output_transcription_.text,
                                output_transcription_.chunks,
                                output_transcription_.entry_id);
}

void TtcConversation::LogToolCallReceived(const std::string& tool_name) {
  FlushSentAudioJournalState();
  if (!journal_) {
    return;
  }
  journal_->Log(TTCJournalComponent::kConversation, "TtcToolCallReceived",
                TTCJournalDetailsBuilder()
                    .SetSummary(base::StrCat({"Tool: ", tool_name}))
                    .Add("tool_name", tool_name)
                    .Build());
}

void TtcConversation::LogToolDefinitionsSent(
    const std::vector<ttc::ToolDefinition>& tool_definitions) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!journal_) {
    return;
  }

  std::vector<std::string_view> tool_names;
  tool_names.reserve(tool_definitions.size());
  for (const auto& tool : tool_definitions) {
    tool_names.push_back(tool.name);
  }
  std::string tool_names_joined = base::JoinString(tool_names, ", ");
  std::string summary = tool_names.empty()
                            ? "Sent tools: none"
                            : base::StrCat({"Sent tools: ", tool_names_joined});

  journal_->Log(TTCJournalComponent::kConversation, "TtcToolDefinitionsSent",
                TTCJournalDetailsBuilder()
                    .SetSummary(summary)
                    .Add("count", tool_definitions.size())
                    .Add("tool_names", tool_names_joined)
                    .Build());
}

void TtcConversation::LogContextUpdateSent(
    const GURL& url,
    const std::string& title,
    const optimization_guide::proto::AnnotatedPageContent& apc) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!journal_) {
    return;
  }

  // Uses `title` for concise glanceability in the simplified journal UI,
  // falling back to `url` host or spec when untitled.
  const std::string_view label = [&]() -> std::string_view {
    if (!title.empty()) {
      return title;
    }
    if (url.has_host()) {
      return url.host();
    }
    if (!url.is_empty()) {
      return url.possibly_invalid_spec();
    }
    return "none";
  }();

  TTCJournalDetailsBuilder details;
  details.SetSummary(base::StrCat({"Sent APC: ", label}));
  if (!url.is_empty()) {
    details.Add("url", url.possibly_invalid_spec());
  }
  if (!title.empty()) {
    details.Add("title", title);
  }
  details.Add("size_bytes", apc.ByteSizeLong())
      .Add("has_tree", apc.has_root_node());
  if (apc.has_tab_id()) {
    details.Add("tab_id", apc.tab_id());
  }

  journal_->Log(TTCJournalComponent::kConversation, "TtcContextUpdateSent",
                std::move(details).Build());
}

void TtcConversation::ResetTurnJournalState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  FlushSentAudioJournalState();
  logged_playback_suppression_ = false;
  logged_disconnected_drop_ = false;
  input_transcription_ = {};
  output_transcription_ = {};
}
