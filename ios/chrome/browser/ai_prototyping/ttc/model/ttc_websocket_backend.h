// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_WEBSOCKET_BACKEND_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_WEBSOCKET_BACKEND_H_

#import <Foundation/Foundation.h>
#import <stddef.h>
#import <stdint.h>

#import <string>
#import <vector>

#import "base/containers/span.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/sequence_checker.h"
#import "components/ttc/app/public/error_codes.h"
#import "components/ttc/app/public/tool_types.h"
#import "components/ttc/app/ttc_backend.h"
#import "ios/public/provider/chrome/browser/intelligence/ttc_api.h"
#import "url/gurl.h"

// Backend implementation managing bidirectional streaming with a live model
// over WebSockets using Apple's native `NSURLSessionWebSocketTask`.
//
// Chromium on iOS does not use Chromium's C++ `net/websockets` stack for
// browser feature WebSockets (where `enable_websockets` is disabled). Native
// Apple `NSURLSessionWebSocketTask` is used for direct WebSocket communication.
class TtcWebSocketBackend : public ttc::TtcBackend {
 public:
  // Creates a WebSocket backend loading configuration via
  // `ios::provider::GetTTCConfig()` and lazily initializing an `NSURLSession`
  // on `Connect()`.
  TtcWebSocketBackend();

  TtcWebSocketBackend(const TtcWebSocketBackend&) = delete;
  TtcWebSocketBackend& operator=(const TtcWebSocketBackend&) = delete;

  ~TtcWebSocketBackend() override;

  // `ttc::TtcBackend` implementation:
  void Connect(Observer* observer) override;
  void SendToolSetUpdate(
      const std::vector<ttc::ToolDefinition>& tools) override;
  void SendAudioChunk(base::span<const int16_t> audio_data) override;
  void SendTextInput(const std::string& text) override;
  void SendContextUpdate(
      const GURL& url,
      const std::string& title,
      const optimization_guide::proto::AnnotatedPageContent& apc) override;
  void ReportPlaybackStatus(int64_t last_played_sequence_number) override;
  void Close() override;
  bool is_transport_connected() const override;

  // Invoked on the sequence by `TTCWebSocketSessionDelegate` when the
  // underlying WebSocket opens.
  void OnWebSocketOpened(NSURLSessionWebSocketTask* task, NSString* protocol);

  // Invoked on the sequence by `TTCWebSocketSessionDelegate` when the remote
  // connection closes.
  void OnWebSocketClosed(NSURLSessionWebSocketTask* task,
                         NSURLSessionWebSocketCloseCode close_code,
                         NSData* reason);

  // Invoked on the sequence by `TTCWebSocketSessionDelegate` when an underlying
  // transport task completes with an error.
  void OnWebSocketTaskCompleted(NSURLSessionTask* task, NSError* error);

  // Returns a weak pointer to this backend instance.
  base::WeakPtr<TtcWebSocketBackend> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // Testing helpers:
  void SetObserverForTesting(Observer* observer);
  size_t in_flight_sends_for_testing() const { return in_flight_sends_; }
  void set_in_flight_sends_for_testing(size_t in_flight_sends) {
    in_flight_sends_ = in_flight_sends;
  }
  void SimulateHandshakingStateForTesting();
  void HandleFatalErrorForTesting(NSError* error);
  void HandleConnectionClosedForTesting();
  void ParseServerMessageForTesting(NSData* payload);

  // Builds the UTF-8 JSON payload for the initial streaming service setup
  // handshake.
  static NSData* CreateSetupPayload(NSString* model,
                                    NSString* voice_name,
                                    NSString* system_instruction);

  // Builds the UTF-8 JSON payload wrapping 16kHz linear PCM audio into
  // `realtimeInput`.
  static NSData* CreateAudioChunkPayload(NSData* pcm_data);

  // Builds the UTF-8 JSON payload for submitting a user text query.
  static NSData* CreateTextInputPayload(NSString* text);

 private:
  // Internal connection states for the WebSocket transport.
  enum class WebSocketState {
    kDisconnected = 0,
    kConnecting,
    kHandshaking,
    kConnected,
    kFailed,
  };

  // Handles successful TCP/TLS connection open and HTTP 101 upgrade.
  void HandleConnectionOpened();

  // Handles clean connection closure signaled by the server or transport.
  void HandleConnectionClosed();

  // Handles unrecoverable network, transport, or protocol errors.
  void HandleFatalError(NSError* error);

  // Handles explicit server-level error frames (e.g., HTTP 429 quota
  // exhaustion).
  void HandleServerError(ttc::ErrorCode error_code);

  // Task-scoped completion handlers guarding against stale task callbacks.
  void HandleFatalErrorForTask(NSURLSessionWebSocketTask* task, NSError* error);
  void HandleSendMessageFinishedForTask(NSURLSessionWebSocketTask* task,
                                        NSError* error);
  void HandleSendMessageFinished(NSError* error);
  void HandleReceivedMessageForTask(NSURLSessionWebSocketTask* task,
                                    NSURLSessionWebSocketMessage* message,
                                    NSError* error);
  void HandleReceivedMessage(NSURLSessionWebSocketMessage* message,
                             NSError* error);

  // Serializes and transmits the initial setup frame configuring the session.
  void SendSetupFrame();

  // Arms the asynchronous receive loop to await the next message from the
  // server.
  void ListenForNextMessage();

  // Schedules a repeating ping timer to keep intermediate NATs and proxies
  // alive.
  void StartHeartbeat();

  // Invalidates and clears the active heartbeat ping timer.
  void StopHeartbeat();

  // Sends a WebSocket ping to verify connection health and receive a pong
  // response.
  void SendHeartbeatPing();

  // Parses a JSON payload received from the streaming service and dispatches
  // observer events. May return prior to finishing parsing the message if an
  // observer callback calls `Close()`.
  void ParseServerMessage(NSData* payload);

  SEQUENCE_CHECKER(sequence_checker_);

  // Unowned observer notified of lifecycle, audio, and error events.
  raw_ptr<Observer> observer_ = nullptr;

  // Provider configuration specifying endpoint URL, model, voice, and API key.
  ios::provider::TTCConfig config_;

  // Active URL session managing the WebSocket task.
  NSURLSession* session_ = nil;

  // Indicates whether an asynchronous `receiveMessage` call is currently in
  // flight.
  bool is_receiving_ = false;

  // Active WebSocket task for bidirectional streaming.
  NSURLSessionWebSocketTask* task_ = nil;

  // Periodic timer sending WebSocket pings to keep the connection alive.
  NSTimer* heartbeat_timer_ = nil;

  // Current lifecycle state of the WebSocket transport.
  WebSocketState state_ = WebSocketState::kDisconnected;

  // Current count of audio messages sent upstream awaiting acknowledgment.
  size_t in_flight_sends_ = 0;

  base::WeakPtrFactory<TtcWebSocketBackend> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_WEBSOCKET_BACKEND_H_
