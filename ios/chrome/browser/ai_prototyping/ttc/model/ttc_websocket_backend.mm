// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_websocket_backend.h"

#import <stddef.h>
#import <stdint.h>

#import <string>
#import <vector>

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/containers/span.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/scoped_refptr.h"
#import "base/memory/weak_ptr.h"
#import "base/sequence_checker.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/bind_post_task.h"
#import "base/task/sequenced_task_runner.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "ios/public/provider/chrome/browser/intelligence/ttc_api.h"
#import "url/gurl.h"

namespace {

// Heartbeat ping interval to keep intermediate proxy connections alive.
// 10 seconds is well within standard mobile carrier NAT idle timeouts (15-30s).
constexpr NSTimeInterval kHeartbeatIntervalSeconds = 10.0;

// Maximum number of audio chunks awaiting network acknowledgment before
// dropping incoming chunks to maintain real-time responsiveness.
// `NSURLSessionWebSocketTask` does not expose underlying buffer levels (unlike
// browser JS `WebSocket.bufferedAmount`), so a small window prevents stale
// audio buffer buildup under network latency.
constexpr size_t kMaxInFlightAudioSends = 5;

// Milliseconds of prefix padding for automatic speech activity detection.
constexpr int kPrefixPaddingMs = 20;

// HTTP status code for rate limiting / quota exhaustion.
constexpr int kHTTPStatusTooManyRequests = 429;

}  // namespace

// Objective-C bridge forwarding `NSURLSessionWebSocketDelegate` callbacks to
// the C++ `TtcWebSocketBackend` on its sequenced task runner.
@interface TTCWebSocketSessionDelegate
    : NSObject <NSURLSessionWebSocketDelegate>

- (instancetype)initWithBackend:(TtcWebSocketBackend*)backend
                     taskRunner:
                         (scoped_refptr<base::SequencedTaskRunner>)taskRunner;

@end

@implementation TTCWebSocketSessionDelegate {
  base::WeakPtr<TtcWebSocketBackend> _backend;
  scoped_refptr<base::SequencedTaskRunner> _taskRunner;
}

- (instancetype)initWithBackend:(TtcWebSocketBackend*)backend
                     taskRunner:
                         (scoped_refptr<base::SequencedTaskRunner>)taskRunner {
  self = [super init];
  if (self) {
    _backend = backend->GetWeakPtr();
    _taskRunner = taskRunner;
  }
  return self;
}

- (void)URLSession:(NSURLSession*)session
          webSocketTask:(NSURLSessionWebSocketTask*)webSocketTask
    didOpenWithProtocol:(NSString*)protocol {
  _taskRunner->PostTask(FROM_HERE,
                        base::BindOnce(&TtcWebSocketBackend::OnWebSocketOpened,
                                       _backend, webSocketTask, protocol));
}

- (void)URLSession:(NSURLSession*)session
       webSocketTask:(NSURLSessionWebSocketTask*)webSocketTask
    didCloseWithCode:(NSURLSessionWebSocketCloseCode)closeCode
              reason:(NSData*)reason {
  _taskRunner->PostTask(
      FROM_HERE, base::BindOnce(&TtcWebSocketBackend::OnWebSocketClosed,
                                _backend, webSocketTask, closeCode, reason));
}

- (void)URLSession:(NSURLSession*)session
                    task:(NSURLSessionTask*)task
    didCompleteWithError:(NSError*)error {
  _taskRunner->PostTask(
      FROM_HERE, base::BindOnce(&TtcWebSocketBackend::OnWebSocketTaskCompleted,
                                _backend, task, error));
}

@end

#pragma mark - Initializers

TtcWebSocketBackend::TtcWebSocketBackend()
    : config_(ios::provider::GetTTCConfig()) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

TtcWebSocketBackend::~TtcWebSocketBackend() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Close();
}

#pragma mark - TTCBackend Properties

bool TtcWebSocketBackend::is_transport_connected() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return state_ == WebSocketState::kConnected;
}

#pragma mark - TTCBackend Connection Lifecycle

void TtcWebSocketBackend::Connect(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(observer);

  if (state_ != WebSocketState::kDisconnected &&
      state_ != WebSocketState::kFailed) {
    return;
  }
  observer_ = observer;

  // Refresh configuration in case test mocks or credentials were set after
  // init.
  config_ = ios::provider::GetTTCConfig();

  // `is_valid()` validates that endpoint URL and model are non-empty. In
  // addition, an API key is required to authenticate with the streaming
  // service.
  if (!config_.is_valid() || config_.api_key.empty()) {
    state_ = WebSocketState::kFailed;
    observer_->OnBackendError(ttc::ErrorCode::kInternalBackendError);
    return;
  }

  GURL endpoint_gurl(config_.endpoint_url);
  if (!endpoint_gurl.is_valid() || !endpoint_gurl.SchemeIsWSOrWSS()) {
    state_ = WebSocketState::kFailed;
    observer_->OnBackendError(ttc::ErrorCode::kInternalBackendError);
    return;
  }

  NSURLComponents* components = [NSURLComponents
      componentsWithString:base::SysUTF8ToNSString(config_.endpoint_url)];
  NSMutableArray<NSURLQueryItem*>* query_items =
      [NSMutableArray arrayWithArray:components.queryItems ?: @[]];
  [query_items
      addObject:[NSURLQueryItem queryItemWithName:@"key"
                                            value:base::SysUTF8ToNSString(
                                                      config_.api_key)]];
  components.queryItems = query_items;
  NSURL* endpoint_url = components.URL;
  if (!endpoint_url) {
    state_ = WebSocketState::kFailed;
    observer_->OnBackendError(ttc::ErrorCode::kInternalBackendError);
    return;
  }

  state_ = WebSocketState::kConnecting;
  in_flight_sends_ = 0;

  if (!session_) {
    NSURLSessionConfiguration* session_config =
        [NSURLSessionConfiguration defaultSessionConfiguration];
    TTCWebSocketSessionDelegate* session_delegate =
        [[TTCWebSocketSessionDelegate alloc]
            initWithBackend:this
                 taskRunner:base::SequencedTaskRunner::GetCurrentDefault()];
    session_ = [NSURLSession sessionWithConfiguration:session_config
                                             delegate:session_delegate
                                        delegateQueue:nil];
  }

  task_ = [session_ webSocketTaskWithURL:endpoint_url];
  [task_ resume];
}

void TtcWebSocketBackend::Close() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  StopHeartbeat();

  if (task_) {
    [task_ cancelWithCloseCode:NSURLSessionWebSocketCloseCodeNormalClosure
                        reason:nil];
    task_ = nil;
  }

  if (session_) {
    [session_ invalidateAndCancel];
    session_ = nil;
  }

  observer_ = nullptr;
  state_ = WebSocketState::kDisconnected;
  in_flight_sends_ = 0;
  is_receiving_ = false;
  weak_ptr_factory_.InvalidateWeakPtrs();
}

#pragma mark - TTCBackend Upstream Data

void TtcWebSocketBackend::SendToolSetUpdate(
    const std::vector<ttc::ToolDefinition>& tools) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Dynamic tool set updates will be wired in a subsequent change.
}

void TtcWebSocketBackend::SendAudioChunk(base::span<const int16_t> audio_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ != WebSocketState::kConnected || audio_data.empty()) {
    return;
  }

  if (!task_ || in_flight_sends_ >= kMaxInFlightAudioSends) {
    return;
  }

  base::span<const uint8_t> audio_bytes = base::as_byte_span(audio_data);
  NSData* pcm_data = [NSData dataWithBytes:audio_bytes.data()
                                    length:audio_bytes.size()];
  NSData* payload = CreateAudioChunkPayload(pcm_data);
  if (!payload) {
    return;
  }

  NSString* message_text = [[NSString alloc] initWithData:payload
                                                 encoding:NSUTF8StringEncoding];
  if (!message_text) {
    return;
  }

  in_flight_sends_++;
  NSURLSessionWebSocketMessage* message =
      [[NSURLSessionWebSocketMessage alloc] initWithString:message_text];

  NSURLSessionWebSocketTask* current_task = task_;
  void (^completion)(NSError*) = base::CallbackToBlock(base::BindPostTask(
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(&TtcWebSocketBackend::HandleSendMessageFinishedForTask,
                     weak_ptr_factory_.GetWeakPtr(), current_task)));
  [task_ sendMessage:message completionHandler:completion];
}

void TtcWebSocketBackend::SendTextInput(const std::string& text) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ != WebSocketState::kConnected || text.empty() || !task_) {
    return;
  }

  NSData* payload = CreateTextInputPayload(base::SysUTF8ToNSString(text));
  if (!payload) {
    return;
  }

  NSString* message_text = [[NSString alloc] initWithData:payload
                                                 encoding:NSUTF8StringEncoding];
  if (!message_text) {
    return;
  }

  NSURLSessionWebSocketMessage* message =
      [[NSURLSessionWebSocketMessage alloc] initWithString:message_text];

  NSURLSessionWebSocketTask* current_task = task_;
  void (^completion)(NSError*) = base::CallbackToBlock(base::BindPostTask(
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(&TtcWebSocketBackend::HandleFatalErrorForTask,
                     weak_ptr_factory_.GetWeakPtr(), current_task)));
  [task_ sendMessage:message completionHandler:completion];
}

void TtcWebSocketBackend::SendContextUpdate(
    const GURL& url,
    const std::string& title,
    const optimization_guide::proto::AnnotatedPageContent& apc) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Context update streaming will be implemented in a subsequent change.
}

void TtcWebSocketBackend::ReportPlaybackStatus(
    int64_t last_played_sequence_number) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Playback sequence status reported for server-side jitter buffer alignment.
}

#pragma mark - Session Delegate Notifications (Called on Sequence)

void TtcWebSocketBackend::OnWebSocketOpened(NSURLSessionWebSocketTask* task,
                                            NSString* protocol) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (task_ != task || state_ == WebSocketState::kDisconnected) {
    return;
  }
  HandleConnectionOpened();
}

void TtcWebSocketBackend::OnWebSocketClosed(
    NSURLSessionWebSocketTask* task,
    NSURLSessionWebSocketCloseCode close_code,
    NSData* reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (task_ != task || state_ == WebSocketState::kDisconnected) {
    return;
  }
  HandleConnectionClosed();
}

void TtcWebSocketBackend::OnWebSocketTaskCompleted(NSURLSessionTask* task,
                                                   NSError* error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (task_ != task || !error) {
    return;
  }
  if (state_ == WebSocketState::kDisconnected ||
      ([error.domain isEqualToString:NSURLErrorDomain] &&
       error.code == NSURLErrorCancelled)) {
    return;
  }
  HandleFatalError(error);
}

#pragma mark - Private Connection Handlers

// Handles successful TCP/TLS connection open and HTTP 101 upgrade.
// Transitions state to handshaking, transmits the initial setup payload,
// and starts the heartbeat and receive loops.
void TtcWebSocketBackend::HandleConnectionOpened() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  state_ = WebSocketState::kHandshaking;
  SendSetupFrame();
  StartHeartbeat();
  ListenForNextMessage();
}

// Handles clean connection closure signaled by the server or transport.
void TtcWebSocketBackend::HandleConnectionClosed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  StopHeartbeat();
  if (task_) {
    [task_ cancelWithCloseCode:NSURLSessionWebSocketCloseCodeNormalClosure
                        reason:nil];
    task_ = nil;
  }
  if (session_) {
    [session_ invalidateAndCancel];
    session_ = nil;
  }
  state_ = WebSocketState::kDisconnected;
  is_receiving_ = false;
  in_flight_sends_ = 0;
  if (observer_) {
    observer_->OnBackendClosed();
  }
}

// Handles unrecoverable network, transport, or protocol errors.
void TtcWebSocketBackend::HandleFatalError(NSError* error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ == WebSocketState::kDisconnected ||
      state_ == WebSocketState::kFailed) {
    return;
  }
  if (error && [error.domain isEqualToString:NSURLErrorDomain] &&
      error.code == NSURLErrorCancelled) {
    return;
  }
  StopHeartbeat();
  if (task_) {
    [task_ cancelWithCloseCode:NSURLSessionWebSocketCloseCodeAbnormalClosure
                        reason:nil];
    task_ = nil;
  }
  if (session_) {
    [session_ invalidateAndCancel];
    session_ = nil;
  }
  state_ = WebSocketState::kFailed;
  is_receiving_ = false;
  in_flight_sends_ = 0;
  if (observer_) {
    observer_->OnBackendError(ttc::ErrorCode::kExecutionSessionCreationFailed);
  }
}

// Handles explicit server-level error frames (e.g., HTTP 429 quota exhaustion).
void TtcWebSocketBackend::HandleServerError(ttc::ErrorCode error_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  StopHeartbeat();
  if (task_) {
    [task_ cancelWithCloseCode:NSURLSessionWebSocketCloseCodePolicyViolation
                        reason:nil];
    task_ = nil;
  }
  if (session_) {
    [session_ invalidateAndCancel];
    session_ = nil;
  }
  state_ = WebSocketState::kFailed;
  is_receiving_ = false;
  in_flight_sends_ = 0;
  if (observer_) {
    observer_->OnBackendError(error_code);
  }
}

void TtcWebSocketBackend::HandleFatalErrorForTask(
    NSURLSessionWebSocketTask* task,
    NSError* error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (task_ != task || !error) {
    return;
  }
  HandleFatalError(error);
}

void TtcWebSocketBackend::HandleSendMessageFinishedForTask(
    NSURLSessionWebSocketTask* task,
    NSError* error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (task_ != task) {
    return;
  }
  HandleSendMessageFinished(error);
}

void TtcWebSocketBackend::HandleReceivedMessageForTask(
    NSURLSessionWebSocketTask* task,
    NSURLSessionWebSocketMessage* message,
    NSError* error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (task_ != task) {
    return;
  }
  HandleReceivedMessage(message, error);
}

// Serializes and transmits the initial setup frame configuring the session.
void TtcWebSocketBackend::SendSetupFrame() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NSString* model = base::SysUTF8ToNSString(config_.model);
  NSString* voice = base::SysUTF8ToNSString(config_.voice_name);
  NSString* instruction = base::SysUTF8ToNSString(config_.system_instruction);

  NSData* payload = CreateSetupPayload(model, voice, instruction);
  if (!payload) {
    HandleFatalError(nil);
    return;
  }

  NSString* message_text = [[NSString alloc] initWithData:payload
                                                 encoding:NSUTF8StringEncoding];
  if (!message_text) {
    HandleFatalError(nil);
    return;
  }
  NSURLSessionWebSocketMessage* message =
      [[NSURLSessionWebSocketMessage alloc] initWithString:message_text];

  NSURLSessionWebSocketTask* current_task = task_;
  void (^completion)(NSError*) = base::CallbackToBlock(base::BindPostTask(
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(&TtcWebSocketBackend::HandleFatalErrorForTask,
                     weak_ptr_factory_.GetWeakPtr(), current_task)));
  [task_ sendMessage:message completionHandler:completion];
}

// Arms the asynchronous receive loop to await the next message from the server.
void TtcWebSocketBackend::ListenForNextMessage() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!task_ || state_ == WebSocketState::kDisconnected ||
      state_ == WebSocketState::kFailed) {
    return;
  }

  if (is_receiving_) {
    return;
  }
  is_receiving_ = true;

  NSURLSessionWebSocketTask* current_task = task_;
  void (^completion)(NSURLSessionWebSocketMessage*, NSError*) =
      base::CallbackToBlock(base::BindPostTask(
          base::SequencedTaskRunner::GetCurrentDefault(),
          base::BindOnce(&TtcWebSocketBackend::HandleReceivedMessageForTask,
                         weak_ptr_factory_.GetWeakPtr(), current_task)));
  [task_ receiveMessageWithCompletionHandler:completion];
}

// Processes a message received from the server and re-arms the receive loop.
void TtcWebSocketBackend::HandleReceivedMessage(
    NSURLSessionWebSocketMessage* message,
    NSError* error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_receiving_ = false;

  if (state_ == WebSocketState::kDisconnected ||
      state_ == WebSocketState::kFailed) {
    return;
  }

  if (error) {
    HandleFatalError(error);
    return;
  }

  if (!message) {
    if (state_ == WebSocketState::kConnected ||
        state_ == WebSocketState::kHandshaking) {
      ListenForNextMessage();
    }
    return;
  }

  NSData* payload = nil;
  if (message.type == NSURLSessionWebSocketMessageTypeString) {
    payload = [message.string dataUsingEncoding:NSUTF8StringEncoding];
  } else if (message.type == NSURLSessionWebSocketMessageTypeData) {
    payload = message.data;
  }
  if (payload) {
    ParseServerMessage(payload);
  }

  if (state_ == WebSocketState::kConnected ||
      state_ == WebSocketState::kHandshaking) {
    ListenForNextMessage();
  }
}

// Updates backpressure bookkeeping after an upstream message completes.
void TtcWebSocketBackend::HandleSendMessageFinished(NSError* error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (in_flight_sends_ > 0) {
    in_flight_sends_--;
  }
  if (error) {
    HandleFatalError(error);
  }
}

// Schedules a repeating ping timer to keep intermediate NATs and proxies alive.
void TtcWebSocketBackend::StartHeartbeat() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  StopHeartbeat();
  base::WeakPtr<TtcWebSocketBackend> weak_ptr = weak_ptr_factory_.GetWeakPtr();
  heartbeat_timer_ = [NSTimer timerWithTimeInterval:kHeartbeatIntervalSeconds
                                            repeats:YES
                                              block:^(NSTimer* timer) {
                                                if (weak_ptr) {
                                                  weak_ptr->SendHeartbeatPing();
                                                }
                                              }];
  [[NSRunLoop currentRunLoop] addTimer:heartbeat_timer_
                               forMode:NSRunLoopCommonModes];
}

// Invalidates and clears the active heartbeat ping timer.
void TtcWebSocketBackend::StopHeartbeat() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (heartbeat_timer_) {
    [heartbeat_timer_ invalidate];
    heartbeat_timer_ = nil;
  }
}

// Sends a WebSocket ping to verify connection health and receive a pong
// response.
void TtcWebSocketBackend::SendHeartbeatPing() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!task_ || state_ != WebSocketState::kConnected) {
    return;
  }
  NSURLSessionWebSocketTask* current_task = task_;
  void (^pong_handler)(NSError*) = base::CallbackToBlock(base::BindPostTask(
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(&TtcWebSocketBackend::HandleFatalErrorForTask,
                     weak_ptr_factory_.GetWeakPtr(), current_task)));
  [task_ sendPingWithPongReceiveHandler:pong_handler];
}

void TtcWebSocketBackend::ParseServerMessage(NSData* payload) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NSError* error = nil;
  id json = [NSJSONSerialization JSONObjectWithData:payload
                                            options:0
                                              error:&error];
  if (error || ![json isKindOfClass:[NSDictionary class]]) {
    return;
  }
  NSDictionary* root = (NSDictionary*)json;

  // 1. Server Error Response.
  id error_obj = root[@"error"];
  if (error_obj && ![error_obj isKindOfClass:[NSNull class]]) {
    int code = 0;
    if ([error_obj isKindOfClass:[NSDictionary class]]) {
      id code_obj = ((NSDictionary*)error_obj)[@"code"];
      if ([code_obj respondsToSelector:@selector(intValue)]) {
        code = [code_obj intValue];
      }
    }
    ttc::ErrorCode ttc_error_code = (code == kHTTPStatusTooManyRequests)
                                        ? ttc::ErrorCode::kRateLimited
                                        : ttc::ErrorCode::kInternalBackendError;
    HandleServerError(ttc_error_code);
    return;
  }

  // 2. Setup Acknowledgement.
  id setup_complete_obj = root[@"setupComplete"];
  if (setup_complete_obj &&
      ![setup_complete_obj isKindOfClass:[NSNull class]]) {
    if (state_ == WebSocketState::kHandshaking) {
      state_ = WebSocketState::kConnected;
      if (observer_) {
        observer_->OnBackendInitialized();
      }
    }
    return;
  }

  // 3. Server Content.
  id server_content_obj = root[@"serverContent"];
  if ([server_content_obj isKindOfClass:[NSDictionary class]]) {
    base::WeakPtr<TtcWebSocketBackend> weak_this =
        weak_ptr_factory_.GetWeakPtr();
    NSDictionary* server_content = (NSDictionary*)server_content_obj;

    // Input speech transcription (server ASR).
    std::string input_text;
    id input_transcription_obj = server_content[@"inputTranscription"];
    if ([input_transcription_obj isKindOfClass:[NSDictionary class]]) {
      id user_transcript = input_transcription_obj[@"text"];
      if ([user_transcript isKindOfClass:[NSString class]]) {
        input_text = base::SysNSStringToUTF8(user_transcript);
      }
    }

    // Output model speech transcription.
    std::string output_text;
    id output_transcription_obj = server_content[@"outputTranscription"];
    if ([output_transcription_obj isKindOfClass:[NSDictionary class]]) {
      id model_transcript = output_transcription_obj[@"text"];
      if ([model_transcript isKindOfClass:[NSString class]]) {
        output_text = base::SysNSStringToUTF8(model_transcript);
      }
    }

    if ((!input_text.empty() || !output_text.empty()) && observer_) {
      observer_->OnTranscriptions(input_text, output_text);
      if (!weak_this) {
        return;
      }
    }

    // Server-side VAD interruption (barge-in).
    id interrupted_obj = server_content[@"interrupted"];
    if ([interrupted_obj respondsToSelector:@selector(boolValue)] &&
        [interrupted_obj boolValue]) {
      if (observer_) {
        observer_->OnGenerationStateChanged(/*started=*/false,
                                            /*completed=*/false,
                                            /*interrupted=*/true);
      }
      return;
    }

    // Audio stream output.
    id model_turn_obj = server_content[@"modelTurn"];
    if ([model_turn_obj isKindOfClass:[NSDictionary class]]) {
      id parts_obj = model_turn_obj[@"parts"];
      if ([parts_obj isKindOfClass:[NSArray class]]) {
        for (id part in (NSArray*)parts_obj) {
          if (![part isKindOfClass:[NSDictionary class]]) {
            continue;
          }
          id inline_data_obj = part[@"inlineData"];
          if (![inline_data_obj isKindOfClass:[NSDictionary class]]) {
            continue;
          }
          id mime_type = inline_data_obj[@"mimeType"];
          id base64_str = inline_data_obj[@"data"];
          if ([mime_type isKindOfClass:[NSString class]] &&
              [mime_type hasPrefix:@"audio/"] &&
              [base64_str isKindOfClass:[NSString class]] &&
              [base64_str length] > 0) {
            NSData* audio_data =
                [[NSData alloc] initWithBase64EncodedString:base64_str
                                                    options:0];
            base::span<const uint8_t> raw_bytes =
                base::apple::NSDataToSpan(audio_data);
            if (!raw_bytes.empty() && raw_bytes.size() % sizeof(int16_t) == 0 &&
                observer_) {
              observer_->OnAudioOutput(
                  base::subtle::reinterpret_span<const int16_t>(raw_bytes),
                  /*sequence_number=*/0);
              if (!weak_this) {
                return;
              }
            }
          }
        }
      }
    }

    // Turn completion.
    id turn_complete_obj = server_content[@"turnComplete"];
    if ([turn_complete_obj respondsToSelector:@selector(boolValue)] &&
        [turn_complete_obj boolValue]) {
      if (observer_) {
        observer_->OnGenerationStateChanged(/*started=*/false,
                                            /*completed=*/true,
                                            /*interrupted=*/false);
      }
      return;
    }
  }
}

// static
NSData* TtcWebSocketBackend::CreateSetupPayload(NSString* model,
                                                NSString* voice_name,
                                                NSString* system_instruction) {
  NSMutableDictionary* setup = [NSMutableDictionary dictionary];
  if (model.length > 0) {
    setup[@"model"] = model;
  }

  NSMutableDictionary* generation_config = [NSMutableDictionary dictionary];
  generation_config[@"responseModalities"] = @[ @"AUDIO" ];
  if (voice_name.length > 0) {
    generation_config[@"speechConfig"] = @{
      @"voiceConfig" : @{
        @"prebuiltVoiceConfig" : @{
          @"voiceName" : voice_name,
        }
      }
    };
  }
  setup[@"generationConfig"] = generation_config;
  setup[@"inputAudioTranscription"] = @{};
  setup[@"outputAudioTranscription"] = @{};
  setup[@"realtimeInputConfig"] = @{
    @"automaticActivityDetection" : @{
      @"startOfSpeechSensitivity" : @"START_SENSITIVITY_LOW",
      @"prefixPaddingMs" : @(kPrefixPaddingMs),
    }
  };

  if (system_instruction.length > 0) {
    setup[@"systemInstruction"] = @{
      @"parts" : @[ @{@"text" : system_instruction} ],
    };
  }

  NSDictionary* root = @{@"setup" : setup};
  return [NSJSONSerialization dataWithJSONObject:root options:0 error:nil];
}

void TtcWebSocketBackend::SetObserverForTesting(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observer_ = observer;
}

void TtcWebSocketBackend::SimulateHandshakingStateForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  state_ = WebSocketState::kHandshaking;
}

void TtcWebSocketBackend::HandleFatalErrorForTesting(NSError* error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  HandleFatalError(error);
}

void TtcWebSocketBackend::HandleConnectionClosedForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  HandleConnectionClosed();
}

void TtcWebSocketBackend::ParseServerMessageForTesting(NSData* payload) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ParseServerMessage(payload);
}

// static
NSData* TtcWebSocketBackend::CreateAudioChunkPayload(NSData* pcm_data) {
  if (!pcm_data.length) {
    return nil;
  }
  NSString* base64_audio = [pcm_data base64EncodedStringWithOptions:0];
  NSDictionary* root = @{
    @"realtimeInput" : @{
      @"audio" : @{
        @"mimeType" : @"audio/pcm;rate=16000",
        @"data" : base64_audio,
      }
    }
  };
  return [NSJSONSerialization dataWithJSONObject:root options:0 error:nil];
}

// static
NSData* TtcWebSocketBackend::CreateTextInputPayload(NSString* text) {
  if (!text.length) {
    return nil;
  }
  NSDictionary* root = @{
    @"clientContent" : @{
      @"turns" : @[ @{
        @"role" : @"user",
        @"parts" : @[ @{@"text" : text} ],
      } ],
      @"turnComplete" : @YES,
    }
  };
  return [NSJSONSerialization dataWithJSONObject:root options:0 error:nil];
}
