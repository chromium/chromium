// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_CONVERSATION_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_CONVERSATION_H_

#import <Foundation/Foundation.h>
#import <stdint.h>

#import <memory>
#import <string>

#import "base/containers/span.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/sequence_checker.h"
#import "components/ttc/app/public/error_codes.h"
#import "components/ttc/app/public/server_journal_event.h"
#import "components/ttc/app/public/tool_types.h"
#import "components/ttc/app/ttc_backend.h"

@protocol TTCAudioController;
@class TTCAudioControllerDelegateBridge;

// Coordinates between audio input/output and the transport session for TTC on
// iOS.
class TtcConversation : public ttc::TtcBackend::Observer {
 public:
  // Delegate receiving initialization, closure, audio energy, and error events
  // from a `TtcConversation`.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Invoked when the underlying transport session has completed
    // initialization.
    virtual void OnConversationInitialized() = 0;

    // Invoked when the underlying transport session closes cleanly.
    virtual void OnConversationClosed() = 0;

    // Invoked when the microphone audio energy level updates. `energy` is
    // normalized in [0.0, 1.0].
    virtual void OnAudioEnergyUpdated(float energy) = 0;

    // Invoked when the conversation encounters an audio or transport error.
    virtual void OnConversationError(ttc::ErrorCode error) = 0;
  };

  // Convenience constructor using a default `TTCAudioSessionController` and
  // default `TtcWebSocketBackend`.
  TtcConversation();

  // Constructs a conversation with a custom `audio_controller` and `backend`.
  TtcConversation(id<TTCAudioController> audio_controller,
                  std::unique_ptr<ttc::TtcBackend> backend);

  TtcConversation(const TtcConversation&) = delete;
  TtcConversation& operator=(const TtcConversation&) = delete;

  ~TtcConversation() override;

  // Sets or returns the delegate receiving conversation events.
  void set_delegate(Delegate* delegate) { delegate_ = delegate; }
  Delegate* delegate() const { return delegate_; }

  // Underlying audio controller.
  id<TTCAudioController> audio_controller() const { return audio_controller_; }

  // Underlying model execution backend.
  ttc::TtcBackend* backend() const { return backend_.get(); }

  // Starts the conversation session: connects to the backend and begins audio
  // capture.
  void Start();

  // Stops the conversation session: halts active audio capture and playback,
  // and disconnects the backend.
  void Stop();

  // Stops capture and playback, clears the audio controller and conversation
  // delegates, and invalidates in-flight operations. Safe to call multiple
  // times.
  void Disconnect();

  // Forwarded `TTCAudioControllerDelegate` callbacks.
  void OnAudioChunkCaptured(NSData* pcm_data);
  void OnInputEnergyUpdated(float energy);
  void OnAudioControllerError(NSError* error);
  void OnAudioControllerStoppedCapture();

  // Returns a weak pointer to this conversation instance.
  base::WeakPtr<TtcConversation> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // `ttc::TtcBackend::Observer` implementation:
  void OnBackendInitialized() override;
  void OnBackendClosed() override;
  void OnBackendError(ttc::ErrorCode error) override;
  void OnTranscriptions(const std::string& input_transcription,
                        const std::string& output_transcription) override;
  void OnAudioOutput(base::span<const int16_t> audio_data,
                     int64_t sequence_number) override;
  void OnGenerationStateChanged(bool started,
                                bool completed,
                                bool interrupted) override;
  void OnToolCall(const ttc::ToolRequest& tool_request,
                  ttc::ToolResponseCallback response_callback) override;
  void OnJournalEvent(const ttc::ServerJournalEvent& journal_event) override;

 private:
  // Handles completion of an asynchronous audio capture start request.
  void HandleStartCaptureResult(bool success,
                                NSError* error,
                                uint64_t generation);

  // Notifies `delegate_` of a conversation error.
  void HandleError(ttc::ErrorCode error);

  SEQUENCE_CHECKER(sequence_checker_);

  // Non-owning pointer to the conversation delegate.
  raw_ptr<Delegate> delegate_ = nullptr;

  // Underlying Objective-C audio controller.
  id<TTCAudioController> audio_controller_ = nil;

  // Objective-C delegate bridge forwarding `TTCAudioControllerDelegate`
  // callbacks to this `TtcConversation`.
  TTCAudioControllerDelegateBridge* audio_delegate_bridge_ = nil;

  // Owned C++ backend instance.
  std::unique_ptr<ttc::TtcBackend> backend_;

  // Generation counter incremented in `Stop()` to invalidate pending in-flight
  // async start or permission callbacks.
  uint64_t session_generation_ = 0;

  // Guard flag preventing re-entrant `Stop()` invocations.
  bool is_stopping_ = false;

  base::WeakPtrFactory<TtcConversation> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_CONVERSATION_H_
