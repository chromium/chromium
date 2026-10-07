// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_websocket_backend.h"

#import <string>

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/scoped_refptr.h"
#import "base/sequence_checker.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/bind_post_task.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_error_codes.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_websocket_backend+Testing.h"
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
constexpr NSUInteger kMaxInFlightAudioSends = 5;

// Milliseconds of prefix padding for automatic speech activity detection.
constexpr int kPrefixPaddingMs = 20;

// HTTP status code for rate limiting / quota exhaustion.
constexpr int kHTTPStatusTooManyRequests = 429;

// Internal connection states for the WebSocket transport.
enum class WebSocketState {
  kDisconnected = 0,
  kConnecting,
  kHandshaking,
  kConnected,
  kFailed,
};

}  // namespace

@class TTCWebSocketSessionDelegate;

@interface TTCWebSocketBackend ()

// Invoked on the sequence when the underlying WebSocket opens.
- (void)webSocketTask:(NSURLSessionWebSocketTask*)webSocketTask
    didOpenWithProtocol:(NSString*)protocol;

// Invoked on the sequence when the remote connection closes.
- (void)webSocketTask:(NSURLSessionWebSocketTask*)webSocketTask
     didCloseWithCode:(NSURLSessionWebSocketCloseCode)closeCode
               reason:(NSData*)reason;

// Invoked on the sequence when an underlying transport task completes with an
// error.
- (void)webSocketTask:(NSURLSessionTask*)task
    didCompleteWithError:(NSError*)error;

@end

// Private delegate that prevents NSURLSession from strongly
// retaining TTCWebSocketBackend.
@interface TTCWebSocketSessionDelegate
    : NSObject <NSURLSessionWebSocketDelegate>

- (instancetype)initWithBackend:(TTCWebSocketBackend*)backend
                     taskRunner:
                         (scoped_refptr<base::SequencedTaskRunner>)taskRunner;

@end

@implementation TTCWebSocketSessionDelegate {
  __weak TTCWebSocketBackend* _backend;
  scoped_refptr<base::SequencedTaskRunner> _taskRunner;
}

- (instancetype)initWithBackend:(TTCWebSocketBackend*)backend
                     taskRunner:
                         (scoped_refptr<base::SequencedTaskRunner>)taskRunner {
  self = [super init];
  if (self) {
    _backend = backend;
    _taskRunner = taskRunner;
  }
  return self;
}

- (void)URLSession:(NSURLSession*)session
          webSocketTask:(NSURLSessionWebSocketTask*)webSocketTask
    didOpenWithProtocol:(NSString*)protocol {
  __weak TTCWebSocketBackend* weakBackend = _backend;
  _taskRunner->PostTask(FROM_HERE, base::BindOnce(^{
                          [weakBackend webSocketTask:webSocketTask
                                 didOpenWithProtocol:protocol];
                        }));
}

- (void)URLSession:(NSURLSession*)session
       webSocketTask:(NSURLSessionWebSocketTask*)webSocketTask
    didCloseWithCode:(NSURLSessionWebSocketCloseCode)closeCode
              reason:(NSData*)reason {
  __weak TTCWebSocketBackend* weakBackend = _backend;
  _taskRunner->PostTask(FROM_HERE, base::BindOnce(^{
                          [weakBackend webSocketTask:webSocketTask
                                    didCloseWithCode:closeCode
                                              reason:reason];
                        }));
}

- (void)URLSession:(NSURLSession*)session
                    task:(NSURLSessionTask*)task
    didCompleteWithError:(NSError*)error {
  __weak TTCWebSocketBackend* weakBackend = _backend;
  _taskRunner->PostTask(FROM_HERE, base::BindOnce(^{
                          [weakBackend webSocketTask:task
                                didCompleteWithError:error];
                        }));
}

@end

@implementation TTCWebSocketBackend {
  SEQUENCE_CHECKER(_sequenceChecker);

  // Provider configuration specifying endpoint URL, model, voice, and API key.
  ios::provider::TTCConfig _config;

  // Active URL session managing the WebSocket task.
  NSURLSession* _session;

  // Session delegate avoiding a strong retain cycle with NSURLSession.
  TTCWebSocketSessionDelegate* _sessionDelegate;

  // Indicates whether an asynchronous receiveMessage call is currently in
  // flight.
  BOOL _isReceiving;

  // Active WebSocket task for bidirectional streaming.
  NSURLSessionWebSocketTask* _task;

  // Periodic timer sending WebSocket pings to keep the connection alive.
  NSTimer* _heartbeatTimer;

  // Current lifecycle state of the WebSocket transport.
  WebSocketState _state;

  // Current count of audio messages sent upstream awaiting acknowledgment.
  NSUInteger _inFlightSends;
}

@synthesize delegate = _delegate;

#pragma mark - Initializers

- (instancetype)initWithSession:(NSURLSession*)session {
  self = [super init];
  if (self) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
    _config = ios::provider::GetTTCConfig();
    _session = session;
    _isReceiving = NO;
    _state = WebSocketState::kDisconnected;
    _inFlightSends = 0;
  }
  return self;
}

- (instancetype)init {
  return [self initWithSession:nil];
}

#pragma mark - TTCBackend Properties

- (BOOL)isConnected {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  return _state == WebSocketState::kConnected;
}

#pragma mark - TTCBackend Connection Lifecycle

- (void)connect {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_state != WebSocketState::kDisconnected &&
      _state != WebSocketState::kFailed) {
    return;
  }

  // Refresh configuration in case test mocks or credentials were set after
  // init.
  _config = ios::provider::GetTTCConfig();

  // `is_valid()` validates that endpoint URL and model are non-empty. In
  // addition, an API key is required to authenticate with the streaming
  // service.
  if (!_config.is_valid() || _config.api_key.empty()) {
    _state = WebSocketState::kFailed;
    [self.delegate backend:self
          didFailWithError:TTCErrorCode::kMissingConfiguration];
    return;
  }

  GURL endpointGURL(_config.endpoint_url);
  if (!endpointGURL.is_valid() || !endpointGURL.SchemeIsWSOrWSS()) {
    _state = WebSocketState::kFailed;
    [self.delegate backend:self
          didFailWithError:TTCErrorCode::kMissingConfiguration];
    return;
  }

  NSURLComponents* components = [NSURLComponents
      componentsWithString:base::SysUTF8ToNSString(_config.endpoint_url)];
  NSMutableArray<NSURLQueryItem*>* queryItems =
      [NSMutableArray arrayWithArray:components.queryItems ?: @[]];
  [queryItems
      addObject:[NSURLQueryItem queryItemWithName:@"key"
                                            value:base::SysUTF8ToNSString(
                                                      _config.api_key)]];
  components.queryItems = queryItems;
  NSURL* endpointUrl = components.URL;
  if (!endpointUrl) {
    _state = WebSocketState::kFailed;
    [self.delegate backend:self
          didFailWithError:TTCErrorCode::kMissingConfiguration];
    return;
  }

  _state = WebSocketState::kConnecting;
  _inFlightSends = 0;

  if (!_session) {
    NSURLSessionConfiguration* sessionConfig =
        [NSURLSessionConfiguration defaultSessionConfiguration];
    _sessionDelegate = [[TTCWebSocketSessionDelegate alloc]
        initWithBackend:self
             taskRunner:base::SequencedTaskRunner::GetCurrentDefault()];
    _session = [NSURLSession sessionWithConfiguration:sessionConfig
                                             delegate:_sessionDelegate
                                        delegateQueue:nil];
  }

  _task = [_session webSocketTaskWithURL:endpointUrl];
  [_task resume];
}

- (void)disconnect {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [self stopHeartbeat];

  if (_task) {
    [_task cancelWithCloseCode:NSURLSessionWebSocketCloseCodeNormalClosure
                        reason:nil];
    _task = nil;
  }

  if (_session) {
    [_session invalidateAndCancel];
    _session = nil;
    _sessionDelegate = nil;
  }

  _state = WebSocketState::kDisconnected;
  _inFlightSends = 0;
  _isReceiving = NO;
}

#pragma mark - TTCBackend Upstream Data

- (void)sendAudioChunk:(NSData*)audioData {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_state != WebSocketState::kConnected || !audioData.length) {
    return;
  }

  if (!_task || _inFlightSends >= kMaxInFlightAudioSends) {
    return;
  }

  NSData* payload = [[self class] createAudioChunkPayloadWithPCMData:audioData];
  if (!payload) {
    return;
  }

  NSString* messageText = [[NSString alloc] initWithData:payload
                                                encoding:NSUTF8StringEncoding];
  if (!messageText) {
    return;
  }

  _inFlightSends++;
  NSURLSessionWebSocketMessage* message =
      [[NSURLSessionWebSocketMessage alloc] initWithString:messageText];

  NSURLSessionWebSocketTask* currentTask = _task;
  __weak __typeof(self) weakSelf = self;
  void (^completion)(NSError*) = base::CallbackToBlock(base::BindPostTask(
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(^(NSError* error) {
        [weakSelf handleSendMessageFinishedForTask:currentTask withError:error];
      })));
  [_task sendMessage:message completionHandler:completion];
}

- (void)sendTextInput:(NSString*)text {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_state != WebSocketState::kConnected || !text.length || !_task) {
    return;
  }

  NSData* payload = [[self class] createTextInputPayloadWithText:text];
  if (!payload) {
    return;
  }

  NSString* messageText = [[NSString alloc] initWithData:payload
                                                encoding:NSUTF8StringEncoding];
  if (!messageText) {
    return;
  }

  NSURLSessionWebSocketMessage* message =
      [[NSURLSessionWebSocketMessage alloc] initWithString:messageText];

  NSURLSessionWebSocketTask* currentTask = _task;
  __weak __typeof(self) weakSelf = self;
  void (^completion)(NSError*) = base::CallbackToBlock(base::BindPostTask(
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(^(NSError* error) {
        if (error) {
          [weakSelf handleFatalErrorForTask:currentTask withError:error];
        }
      })));
  [_task sendMessage:message completionHandler:completion];
}

- (void)sendContextUpdateWithURL:(const GURL&)url
                           title:(NSString*)title
                         content:(NSString*)content {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  // Context update streaming will be implemented in a subsequent change.
}

- (void)reportPlaybackStatus:(int64_t)lastPlayedSequenceNumber {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  // Playback sequence status reported for server-side jitter buffer alignment.
}

#pragma mark - Session Delegate Notifications (Called on Sequence)

- (void)webSocketTask:(NSURLSessionWebSocketTask*)webSocketTask
    didOpenWithProtocol:(NSString*)protocol {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_task != webSocketTask || _state == WebSocketState::kDisconnected) {
    return;
  }
  [self handleConnectionOpened];
}

- (void)webSocketTask:(NSURLSessionWebSocketTask*)webSocketTask
     didCloseWithCode:(NSURLSessionWebSocketCloseCode)closeCode
               reason:(NSData*)reason {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_task != webSocketTask || _state == WebSocketState::kDisconnected) {
    return;
  }
  [self handleConnectionClosed];
}

- (void)webSocketTask:(NSURLSessionTask*)task
    didCompleteWithError:(NSError*)error {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_task != task || !error) {
    return;
  }
  if (_state == WebSocketState::kDisconnected ||
      ([error.domain isEqualToString:NSURLErrorDomain] &&
       error.code == NSURLErrorCancelled)) {
    return;
  }
  [self handleFatalErrorWithError:error];
}

#pragma mark - Private Connection Handlers

// Handles successful TCP/TLS connection open and HTTP 101 upgrade.
// Transitions state to handshaking, transmits the initial setup payload,
// and starts the heartbeat and receive loops.
- (void)handleConnectionOpened {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  _state = WebSocketState::kHandshaking;
  [self sendSetupFrame];
  [self startHeartbeat];
  [self listenForNextMessage];
}

// Handles clean connection closure signaled by the server or transport.
- (void)handleConnectionClosed {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [self stopHeartbeat];
  if (_task) {
    [_task cancelWithCloseCode:NSURLSessionWebSocketCloseCodeNormalClosure
                        reason:nil];
    _task = nil;
  }
  if (_session) {
    [_session invalidateAndCancel];
    _session = nil;
    _sessionDelegate = nil;
  }
  _state = WebSocketState::kDisconnected;
  _isReceiving = NO;
  _inFlightSends = 0;
  [self.delegate backendDidClose:self];
}

// Handles unrecoverable network, transport, or protocol errors.
- (void)handleFatalErrorWithError:(NSError*)error {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_state == WebSocketState::kDisconnected ||
      _state == WebSocketState::kFailed) {
    return;
  }
  if (error && [error.domain isEqualToString:NSURLErrorDomain] &&
      error.code == NSURLErrorCancelled) {
    return;
  }
  [self stopHeartbeat];
  if (_task) {
    [_task cancelWithCloseCode:NSURLSessionWebSocketCloseCodeAbnormalClosure
                        reason:nil];
    _task = nil;
  }
  if (_session) {
    [_session invalidateAndCancel];
    _session = nil;
    _sessionDelegate = nil;
  }
  _state = WebSocketState::kFailed;
  _isReceiving = NO;
  _inFlightSends = 0;
  [self.delegate backend:self didFailWithError:TTCErrorCode::kNetworkError];
}

// Handles explicit server-level error frames (e.g., HTTP 429 quota exhaustion).
- (void)handleServerError:(TTCErrorCode)errorCode {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [self stopHeartbeat];
  if (_task) {
    [_task cancelWithCloseCode:NSURLSessionWebSocketCloseCodePolicyViolation
                        reason:nil];
    _task = nil;
  }
  if (_session) {
    [_session invalidateAndCancel];
    _session = nil;
    _sessionDelegate = nil;
  }
  _state = WebSocketState::kFailed;
  _isReceiving = NO;
  _inFlightSends = 0;
  [self.delegate backend:self didFailWithError:errorCode];
}

- (void)handleFatalErrorForTask:(NSURLSessionWebSocketTask*)task
                      withError:(NSError*)error {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_task != task) {
    return;
  }
  [self handleFatalErrorWithError:error];
}

- (void)handleSendMessageFinishedForTask:(NSURLSessionWebSocketTask*)task
                               withError:(NSError*)error {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_task != task) {
    return;
  }
  [self handleSendMessageFinishedWithError:error];
}

- (void)handleReceivedMessage:(NSURLSessionWebSocketMessage*)message
                      forTask:(NSURLSessionWebSocketTask*)task
                        error:(NSError*)error {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_task != task) {
    return;
  }
  [self handleReceivedMessage:message error:error];
}

// Serializes and transmits the initial setup frame configuring the session.
- (void)sendSetupFrame {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  NSString* model = base::SysUTF8ToNSString(_config.model);
  NSString* voice = base::SysUTF8ToNSString(_config.voice_name);
  NSString* instruction = base::SysUTF8ToNSString(_config.system_instruction);

  NSData* payload = [[self class] createSetupPayloadWithModel:model
                                                    voiceName:voice
                                            systemInstruction:instruction];
  if (!payload) {
    [self handleFatalErrorWithError:nil];
    return;
  }

  NSString* messageText = [[NSString alloc] initWithData:payload
                                                encoding:NSUTF8StringEncoding];
  if (!messageText) {
    [self handleFatalErrorWithError:nil];
    return;
  }
  NSURLSessionWebSocketMessage* message =
      [[NSURLSessionWebSocketMessage alloc] initWithString:messageText];

  NSURLSessionWebSocketTask* currentTask = _task;
  __weak __typeof(self) weakSelf = self;
  void (^completion)(NSError*) = base::CallbackToBlock(base::BindPostTask(
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(^(NSError* error) {
        if (error) {
          [weakSelf handleFatalErrorForTask:currentTask withError:error];
        }
      })));
  [_task sendMessage:message completionHandler:completion];
}

// Arms the asynchronous receive loop to await the next message from the server.
- (void)listenForNextMessage {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (!_task || _state == WebSocketState::kDisconnected ||
      _state == WebSocketState::kFailed) {
    return;
  }

  if (_isReceiving) {
    return;
  }
  _isReceiving = YES;

  NSURLSessionWebSocketTask* currentTask = _task;
  __weak __typeof(self) weakSelf = self;
  void (^completion)(NSURLSessionWebSocketMessage*, NSError*) =
      base::CallbackToBlock(base::BindPostTask(
          base::SequencedTaskRunner::GetCurrentDefault(),
          base::BindOnce(
              ^(NSURLSessionWebSocketMessage* message, NSError* error) {
                [weakSelf handleReceivedMessage:message
                                        forTask:currentTask
                                          error:error];
              })));
  [_task receiveMessageWithCompletionHandler:completion];
}

// Processes a message received from the server and re-arms the receive loop.
- (void)handleReceivedMessage:(NSURLSessionWebSocketMessage*)message
                        error:(NSError*)error {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  _isReceiving = NO;

  if (_state == WebSocketState::kDisconnected ||
      _state == WebSocketState::kFailed) {
    return;
  }

  if (error) {
    [self handleFatalErrorWithError:error];
    return;
  }

  if (!message) {
    if (_state == WebSocketState::kConnected ||
        _state == WebSocketState::kHandshaking) {
      [self listenForNextMessage];
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
    [self parseServerMessage:payload];
  }

  if (_state == WebSocketState::kConnected ||
      _state == WebSocketState::kHandshaking) {
    [self listenForNextMessage];
  }
}

// Updates backpressure bookkeeping after an upstream message completes.
- (void)handleSendMessageFinishedWithError:(NSError*)error {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_inFlightSends > 0) {
    _inFlightSends--;
  }
  if (error) {
    [self handleFatalErrorWithError:error];
  }
}

// Schedules a repeating ping timer to keep intermediate NATs and proxies alive.
- (void)startHeartbeat {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [self stopHeartbeat];
  __weak __typeof(self) weakSelf = self;
  _heartbeatTimer = [NSTimer timerWithTimeInterval:kHeartbeatIntervalSeconds
                                           repeats:YES
                                             block:^(NSTimer* timer) {
                                               [weakSelf sendHeartbeatPing];
                                             }];
  [[NSRunLoop currentRunLoop] addTimer:_heartbeatTimer
                               forMode:NSRunLoopCommonModes];
}

// Invalidates and clears the active heartbeat ping timer.
- (void)stopHeartbeat {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_heartbeatTimer) {
    [_heartbeatTimer invalidate];
    _heartbeatTimer = nil;
  }
}

// Sends a WebSocket ping to verify connection health and receive a pong
// response.
- (void)sendHeartbeatPing {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (!_task || _state != WebSocketState::kConnected) {
    return;
  }
  NSURLSessionWebSocketTask* currentTask = _task;
  __weak __typeof(self) weakSelf = self;
  void (^pongHandler)(NSError*) = base::CallbackToBlock(base::BindPostTask(
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(^(NSError* error) {
        if (error) {
          [weakSelf handleFatalErrorForTask:currentTask withError:error];
        }
      })));
  [_task sendPingWithPongReceiveHandler:pongHandler];
}

#pragma mark - TTCWebSocketBackend (Testing)

- (void)parseServerMessage:(NSData*)payload {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  NSError* error = nil;
  id json = [NSJSONSerialization JSONObjectWithData:payload
                                            options:0
                                              error:&error];
  if (error || ![json isKindOfClass:[NSDictionary class]]) {
    return;
  }
  NSDictionary* root = (NSDictionary*)json;

  // 1. Server Error Response.
  id errorObj = root[@"error"];
  if (errorObj && ![errorObj isKindOfClass:[NSNull class]]) {
    int code = 0;
    if ([errorObj isKindOfClass:[NSDictionary class]]) {
      id codeObj = ((NSDictionary*)errorObj)[@"code"];
      if ([codeObj respondsToSelector:@selector(intValue)]) {
        code = [codeObj intValue];
      }
    }
    TTCErrorCode ttcErrorCode = (code == kHTTPStatusTooManyRequests)
                                    ? TTCErrorCode::kRateLimited
                                    : TTCErrorCode::kInternalBackendError;
    [self handleServerError:ttcErrorCode];
    return;
  }

  // 2. Setup Acknowledgement.
  id setupCompleteObj = root[@"setupComplete"];
  if (setupCompleteObj && ![setupCompleteObj isKindOfClass:[NSNull class]]) {
    if (_state == WebSocketState::kHandshaking) {
      _state = WebSocketState::kConnected;
      [self.delegate backendDidInitialize:self];
    }
    return;
  }

  // 3. Server Content.
  id serverContentObj = root[@"serverContent"];
  if ([serverContentObj isKindOfClass:[NSDictionary class]]) {
    NSDictionary* serverContent = (NSDictionary*)serverContentObj;

    // Input speech transcription (server ASR).
    id inputTranscriptionObj = serverContent[@"inputTranscription"];
    if ([inputTranscriptionObj isKindOfClass:[NSDictionary class]]) {
      id userTranscript = inputTranscriptionObj[@"text"];
      if ([userTranscript isKindOfClass:[NSString class]] &&
          [userTranscript length] > 0) {
        [self.delegate backend:self
            didReceiveInputTranscription:userTranscript];
      }
    }

    // Output model speech transcription.
    id outputTranscriptionObj = serverContent[@"outputTranscription"];
    if ([outputTranscriptionObj isKindOfClass:[NSDictionary class]]) {
      id modelTranscript = outputTranscriptionObj[@"text"];
      if ([modelTranscript isKindOfClass:[NSString class]] &&
          [modelTranscript length] > 0) {
        [self.delegate backend:self
            didReceiveOutputTranscription:modelTranscript];
      }
    }

    // Server-side VAD interruption (barge-in).
    id interruptedObj = serverContent[@"interrupted"];
    if ([interruptedObj respondsToSelector:@selector(boolValue)] &&
        [interruptedObj boolValue]) {
      [self.delegate backend:self
          didChangeGenerationStateStarted:NO
                                completed:NO
                              interrupted:YES];
      return;
    }

    // Audio stream output.
    id modelTurnObj = serverContent[@"modelTurn"];
    if ([modelTurnObj isKindOfClass:[NSDictionary class]]) {
      id partsObj = modelTurnObj[@"parts"];
      if ([partsObj isKindOfClass:[NSArray class]]) {
        for (id part in (NSArray*)partsObj) {
          if (![part isKindOfClass:[NSDictionary class]]) {
            continue;
          }
          id inlineDataObj = part[@"inlineData"];
          if (![inlineDataObj isKindOfClass:[NSDictionary class]]) {
            continue;
          }
          id mimeType = inlineDataObj[@"mimeType"];
          id base64Str = inlineDataObj[@"data"];
          if ([mimeType isKindOfClass:[NSString class]] &&
              [mimeType hasPrefix:@"audio/"] &&
              [base64Str isKindOfClass:[NSString class]] &&
              [base64Str length] > 0) {
            NSData* audioData =
                [[NSData alloc] initWithBase64EncodedString:base64Str
                                                    options:0];
            if (audioData.length > 0) {
              [self.delegate backend:self didReceiveAudioOutput:audioData];
            }
          }
        }
      }
    }

    // Turn completion.
    id turnCompleteObj = serverContent[@"turnComplete"];
    if ([turnCompleteObj respondsToSelector:@selector(boolValue)] &&
        [turnCompleteObj boolValue]) {
      [self.delegate backend:self
          didChangeGenerationStateStarted:NO
                                completed:YES
                              interrupted:NO];
    }
  }
}

+ (NSData*)createSetupPayloadWithModel:(NSString*)model
                             voiceName:(NSString*)voiceName
                     systemInstruction:(NSString*)systemInstruction {
  NSMutableDictionary* setup = [NSMutableDictionary dictionary];
  if (model.length > 0) {
    setup[@"model"] = model;
  }

  NSMutableDictionary* generationConfig = [NSMutableDictionary dictionary];
  generationConfig[@"responseModalities"] = @[ @"AUDIO" ];
  if (voiceName.length > 0) {
    generationConfig[@"speechConfig"] = @{
      @"voiceConfig" : @{
        @"prebuiltVoiceConfig" : @{
          @"voiceName" : voiceName,
        }
      }
    };
  }
  setup[@"generationConfig"] = generationConfig;
  setup[@"inputAudioTranscription"] = @{};
  setup[@"outputAudioTranscription"] = @{};
  setup[@"realtimeInputConfig"] = @{
    @"automaticActivityDetection" : @{
      @"startOfSpeechSensitivity" : @"START_SENSITIVITY_LOW",
      @"prefixPaddingMs" : @(kPrefixPaddingMs),
    }
  };

  if (systemInstruction.length > 0) {
    setup[@"systemInstruction"] = @{
      @"parts" : @[ @{@"text" : systemInstruction} ],
    };
  }

  NSDictionary* root = @{@"setup" : setup};
  return [NSJSONSerialization dataWithJSONObject:root options:0 error:nil];
}

- (NSUInteger)inFlightSends {
  return _inFlightSends;
}

- (void)setInFlightSends:(NSUInteger)inFlightSends {
  _inFlightSends = inFlightSends;
}

- (void)simulateHandshakingStateForTesting {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  _state = WebSocketState::kHandshaking;
}

+ (NSData*)createAudioChunkPayloadWithPCMData:(NSData*)pcmData {
  if (!pcmData.length) {
    return nil;
  }
  NSString* base64Audio = [pcmData base64EncodedStringWithOptions:0];
  NSDictionary* root = @{
    @"realtimeInput" : @{
      @"audio" : @{
        @"mimeType" : @"audio/pcm;rate=16000",
        @"data" : base64Audio,
      }
    }
  };
  return [NSJSONSerialization dataWithJSONObject:root options:0 error:nil];
}

+ (NSData*)createTextInputPayloadWithText:(NSString*)text {
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

@end
