// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_websocket_backend.h"

#import <Foundation/Foundation.h>

#import "base/test/task_environment.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_error_codes.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_websocket_backend+Testing.h"
#import "ios/chrome/test/providers/intelligence/test_ttc_api.h"
#import "ios/public/provider/chrome/browser/intelligence/ttc_api.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

constexpr char kTestApiKey[] = "test-api-key";
constexpr char kTestEndpoint[] = "wss://example.com/ws/live";
constexpr char kTestModel[] = "models/test-live-model";
constexpr char kTestSystemInstruction[] = "You are a test assistant.";
constexpr char kTestVoice[] = "TestVoice";

ios::provider::TTCConfig GetValidTestConfig() {
  return ios::provider::TTCConfig{
      .system_instruction = kTestSystemInstruction,
      .model = kTestModel,
      .voice_name = kTestVoice,
      .endpoint_url = kTestEndpoint,
      .api_key = kTestApiKey,
  };
}

}  // namespace

class TTCWebSocketBackendTest : public PlatformTest {
 public:
  void SetUp() override {
    @autoreleasepool {
      PlatformTest::SetUp();
      ios::provider::test::SetTTCConfigForTesting(GetValidTestConfig());
      backend_ = [[TTCWebSocketBackend alloc] init];
      delegate_mock_ = OCMProtocolMock(@protocol(TTCBackendDelegate));
      backend_.delegate = delegate_mock_;
    }
  }

  void TearDown() override {
    @autoreleasepool {
      [backend_ disconnect];
      backend_.delegate = nil;
      backend_ = nil;
      delegate_mock_ = nil;
      ios::provider::test::ResetTTCConfigForTesting();
      PlatformTest::TearDown();
    }
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  TTCWebSocketBackend* backend_ = nil;
  id delegate_mock_ = nil;
};

// Tests that `createSetupPayloadWithModel:voiceName:systemInstruction:`
// produces a valid JSON structure conforming to the live model API
// specification.
TEST_F(TTCWebSocketBackendTest, TestCreateSetupPayload) {
  NSString* model = @"models/sample-model";
  NSString* voice = @"SampleVoice";
  NSString* instruction = @"Helpful persona.";

  NSData* payload =
      [TTCWebSocketBackend createSetupPayloadWithModel:model
                                             voiceName:voice
                                     systemInstruction:instruction];
  ASSERT_TRUE(payload);

  NSError* error = nil;
  NSDictionary* root = [NSJSONSerialization JSONObjectWithData:payload
                                                       options:0
                                                         error:&error];
  ASSERT_TRUE(root);
  EXPECT_FALSE(error);

  NSDictionary* setup = root[@"setup"];
  ASSERT_TRUE(setup);
  EXPECT_NSEQ(model, setup[@"model"]);

  NSDictionary* genConfig = setup[@"generationConfig"];
  ASSERT_TRUE(genConfig);
  EXPECT_NSEQ(@[ @"AUDIO" ], genConfig[@"responseModalities"]);

  NSDictionary* voiceConfig =
      genConfig[@"speechConfig"][@"voiceConfig"][@"prebuiltVoiceConfig"];
  ASSERT_TRUE(voiceConfig);
  EXPECT_NSEQ(voice, voiceConfig[@"voiceName"]);

  NSArray* parts = setup[@"systemInstruction"][@"parts"];
  ASSERT_TRUE(parts.count > 0);
  EXPECT_NSEQ(instruction, parts[0][@"text"]);
}

// Tests that `createAudioChunkPayloadWithPCMData:` correctly encodes linear PCM
// audio into a base64-encoded `realtimeInput` JSON payload.
TEST_F(TTCWebSocketBackendTest, TestCreateAudioChunkPayload) {
  const uint8_t rawBytes[] = {0x01, 0x02, 0x03, 0x04};
  NSData* samplePcm = [NSData dataWithBytes:rawBytes length:sizeof(rawBytes)];

  NSData* payload =
      [TTCWebSocketBackend createAudioChunkPayloadWithPCMData:samplePcm];
  ASSERT_TRUE(payload);

  NSError* error = nil;
  NSDictionary* root = [NSJSONSerialization JSONObjectWithData:payload
                                                       options:0
                                                         error:&error];
  ASSERT_TRUE(root);
  EXPECT_FALSE(error);

  NSDictionary* audioChunk = root[@"realtimeInput"][@"audio"];
  ASSERT_TRUE(audioChunk);
  EXPECT_NSEQ(@"audio/pcm;rate=16000", audioChunk[@"mimeType"]);
  EXPECT_NSEQ([samplePcm base64EncodedStringWithOptions:0],
              audioChunk[@"data"]);

  // An empty audio chunk should produce nil.
  EXPECT_FALSE(
      [TTCWebSocketBackend createAudioChunkPayloadWithPCMData:[NSData data]]);
}

// Tests that `createTextInputPayloadWithText:` wraps user query text into a
// complete `clientContent` turn JSON payload.
TEST_F(TTCWebSocketBackendTest, TestCreateTextInputPayload) {
  NSString* queryText = @"What is the weather today?";
  NSData* payload =
      [TTCWebSocketBackend createTextInputPayloadWithText:queryText];
  ASSERT_TRUE(payload);

  NSError* error = nil;
  NSDictionary* root = [NSJSONSerialization JSONObjectWithData:payload
                                                       options:0
                                                         error:&error];
  ASSERT_TRUE(root);
  EXPECT_FALSE(error);

  NSArray* turns = root[@"clientContent"][@"turns"];
  ASSERT_TRUE(turns.count > 0);
  EXPECT_NSEQ(@"user", turns[0][@"role"]);
  EXPECT_NSEQ(queryText, turns[0][@"parts"][0][@"text"]);
  EXPECT_TRUE([root[@"clientContent"][@"turnComplete"] boolValue]);

  // Empty text should produce nil.
  EXPECT_FALSE([TTCWebSocketBackend createTextInputPayloadWithText:@""]);
}

// Tests that receiving `setupComplete` from the server transitions connection
// state to connected and notifies the delegate when in handshaking state.
TEST_F(TTCWebSocketBackendTest, TestParseSetupComplete) {
  OCMExpect([delegate_mock_ backendDidInitialize:backend_]);

  [backend_ simulateHandshakingStateForTesting];

  NSDictionary* message = @{@"setupComplete" : @{}};
  NSData* payload = [NSJSONSerialization dataWithJSONObject:message
                                                    options:0
                                                      error:nil];
  [backend_ parseServerMessage:payload];

  EXPECT_TRUE(backend_.isConnected);
  EXPECT_OCMOCK_VERIFY(delegate_mock_);
}

// Tests that `setupComplete` is ignored if the backend is not in the
// handshaking state.
TEST_F(TTCWebSocketBackendTest, TestParseSetupCompleteIgnoredIfNotHandshaking) {
  [[delegate_mock_ reject] backendDidInitialize:backend_];

  NSDictionary* message = @{@"setupComplete" : @{}};
  NSData* payload = [NSJSONSerialization dataWithJSONObject:message
                                                    options:0
                                                      error:nil];
  [backend_ parseServerMessage:payload];

  EXPECT_FALSE(backend_.isConnected);
  EXPECT_OCMOCK_VERIFY(delegate_mock_);
}

// Tests that receiving synthesized audio output chunks decodes base64 linear
// PCM and dispatches them to the delegate.
TEST_F(TTCWebSocketBackendTest, TestParseAudioChunk) {
  const uint8_t rawPcm[] = {0x10, 0x20, 0x30, 0x40};
  NSData* pcmData = [NSData dataWithBytes:rawPcm length:sizeof(rawPcm)];
  NSString* base64Pcm = [pcmData base64EncodedStringWithOptions:0];

  NSDictionary* message = @{
    @"serverContent" : @{
      @"modelTurn" : @{
        @"parts" : @[ @{
          @"inlineData" : @{
            @"mimeType" : @"audio/pcm;rate=24000",
            @"data" : base64Pcm,
          }
        } ]
      }
    }
  };

  OCMExpect([delegate_mock_ backend:backend_ didReceiveAudioOutput:pcmData]);

  NSData* payload = [NSJSONSerialization dataWithJSONObject:message
                                                    options:0
                                                      error:nil];
  [backend_ parseServerMessage:payload];

  EXPECT_OCMOCK_VERIFY(delegate_mock_);
}

// Tests that receiving an interrupted signal dispatches a generation state
// update with `interrupted == YES`.
TEST_F(TTCWebSocketBackendTest, TestParseInterruption) {
  OCMExpect([delegate_mock_ backend:backend_
      didChangeGenerationStateStarted:NO
                            completed:NO
                          interrupted:YES]);

  NSDictionary* message = @{@"serverContent" : @{@"interrupted" : @YES}};
  NSData* payload = [NSJSONSerialization dataWithJSONObject:message
                                                    options:0
                                                      error:nil];
  [backend_ parseServerMessage:payload];

  EXPECT_OCMOCK_VERIFY(delegate_mock_);
}

// Tests that receiving `turnComplete` dispatches a generation state update with
// `completed == YES`.
TEST_F(TTCWebSocketBackendTest, TestParseTurnComplete) {
  OCMExpect([delegate_mock_ backend:backend_
      didChangeGenerationStateStarted:NO
                            completed:YES
                          interrupted:NO]);

  NSDictionary* message = @{@"serverContent" : @{@"turnComplete" : @YES}};
  NSData* payload = [NSJSONSerialization dataWithJSONObject:message
                                                    options:0
                                                      error:nil];
  [backend_ parseServerMessage:payload];

  EXPECT_OCMOCK_VERIFY(delegate_mock_);
}

// Tests that receiving streaming user and model transcriptions forwards them to
// the delegate independently.
TEST_F(TTCWebSocketBackendTest, TestParseTranscriptions) {
  NSString* userSpeech = @"Hello assistant";
  NSString* modelResponse = @"Hello! How can I help?";

  OCMExpect([delegate_mock_ backend:backend_
       didReceiveInputTranscription:userSpeech]);
  OCMExpect([delegate_mock_ backend:backend_
      didReceiveOutputTranscription:modelResponse]);

  NSDictionary* message = @{
    @"serverContent" : @{
      @"inputTranscription" : @{@"text" : userSpeech},
      @"outputTranscription" : @{@"text" : modelResponse},
    }
  };

  NSData* payload = [NSJSONSerialization dataWithJSONObject:message
                                                    options:0
                                                      error:nil];
  [backend_ parseServerMessage:payload];

  EXPECT_OCMOCK_VERIFY(delegate_mock_);
}

// Tests that receiving an error envelope from the server dispatches the
// corresponding error to the delegate.
TEST_F(TTCWebSocketBackendTest, TestParseServerErrorResponse) {
  OCMExpect([delegate_mock_ backend:backend_
                   didFailWithError:ttc::ErrorCode::kRateLimited]);

  NSDictionary* message = @{
    @"error" : @{
      @"code" : @429,
      @"message" : @"Resource has been exhausted (e.g. check quota).",
      @"status" : @"RESOURCE_EXHAUSTED",
    }
  };

  NSData* payload = [NSJSONSerialization dataWithJSONObject:message
                                                    options:0
                                                      error:nil];
  [backend_ parseServerMessage:payload];

  EXPECT_OCMOCK_VERIFY(delegate_mock_);
}

// Tests that attempting to connect with an invalid or missing API key
// dispatches `kInternalBackendError` error.
TEST_F(TTCWebSocketBackendTest, TestConnectWithoutValidConfig) {
  ios::provider::test::SetTTCConfigForTesting(ios::provider::TTCConfig{});

  OCMExpect([delegate_mock_ backend:backend_
                   didFailWithError:ttc::ErrorCode::kInternalBackendError]);

  [backend_ connect];

  EXPECT_FALSE(backend_.isConnected);
  EXPECT_OCMOCK_VERIFY(delegate_mock_);
}

// Tests that calling `disconnect` stops the session and does NOT notify the
// delegate that the backend closed, avoiding re-entrancy loops.
TEST_F(TTCWebSocketBackendTest, TestDisconnect) {
  [[delegate_mock_ reject] backendDidClose:backend_];

  [backend_ disconnect];

  EXPECT_FALSE(backend_.isConnected);
  EXPECT_OCMOCK_VERIFY(delegate_mock_);
}

// Tests that remote connection closure notifies the delegate that the backend
// closed.
TEST_F(TTCWebSocketBackendTest, TestHandleConnectionClosed) {
  OCMExpect([delegate_mock_ backendDidClose:backend_]);

  [backend_ handleConnectionClosed];

  EXPECT_FALSE(backend_.isConnected);
  EXPECT_OCMOCK_VERIFY(delegate_mock_);
}

// Tests that `parseServerMessage:` defensively handles null, malformed, and
// missing fields without crashing or invoking unexpected delegate callbacks.
TEST_F(TTCWebSocketBackendTest,
       TestParseServerMessageNullAndMalformedPayloads) {
  [[delegate_mock_ reject] backendDidInitialize:OCMOCK_ANY];
  [[delegate_mock_ reject] backend:OCMOCK_ANY didReceiveAudioOutput:OCMOCK_ANY];
  [[delegate_mock_ reject] backend:OCMOCK_ANY
      didReceiveInputTranscription:OCMOCK_ANY];
  [[delegate_mock_ reject] backend:OCMOCK_ANY
      didReceiveOutputTranscription:OCMOCK_ANY];
  [[delegate_mock_ reject] backend:backend_
      didChangeGenerationStateStarted:NO
                            completed:YES
                          interrupted:NO];
  [[delegate_mock_ reject] backend:backend_
      didChangeGenerationStateStarted:NO
                            completed:NO
                          interrupted:YES];

  // 1. NSNull in root error: should be ignored cleanly.
  NSDictionary* nullErrorMsg = @{@"error" : [NSNull null]};
  NSData* nullErrorPayload =
      [NSJSONSerialization dataWithJSONObject:nullErrorMsg options:0 error:nil];
  [backend_ parseServerMessage:nullErrorPayload];

  // 2. NSNull in setupComplete: should not transition to connected or notify.
  NSDictionary* nullSetupMsg = @{@"setupComplete" : [NSNull null]};
  NSData* nullSetupPayload =
      [NSJSONSerialization dataWithJSONObject:nullSetupMsg options:0 error:nil];
  [backend_ parseServerMessage:nullSetupPayload];
  EXPECT_FALSE(backend_.isConnected);

  // 3. NSNull in serverContent: should be ignored cleanly.
  NSDictionary* nullContentMsg = @{@"serverContent" : [NSNull null]};
  NSData* nullContentPayload =
      [NSJSONSerialization dataWithJSONObject:nullContentMsg
                                      options:0
                                        error:nil];
  [backend_ parseServerMessage:nullContentPayload];

  // 4. Malformed parts array containing NSNull and incomplete inlineData.
  NSDictionary* malformedPartsMsg = @{
    @"serverContent" : @{
      @"modelTurn" : @{
        @"parts" : @[
          [NSNull null],
          @{@"inlineData" : [NSNull null]},
          @{@"inlineData" : @{@"mimeType" : [NSNull null]}},
          @{
            @"inlineData" :
                @{@"mimeType" : @"audio/pcm", @"data" : [NSNull null]}
          },
        ]
      }
    }
  };
  NSData* malformedPartsPayload =
      [NSJSONSerialization dataWithJSONObject:malformedPartsMsg
                                      options:0
                                        error:nil];
  [backend_ parseServerMessage:malformedPartsPayload];

  // 5. NSNull in transcriptions.
  NSDictionary* nullTranscriptsMsg = @{
    @"serverContent" : @{
      @"inputTranscription" : [NSNull null],
      @"outputTranscription" : [NSNull null],
    }
  };
  NSData* nullTranscriptsPayload =
      [NSJSONSerialization dataWithJSONObject:nullTranscriptsMsg
                                      options:0
                                        error:nil];
  [backend_ parseServerMessage:nullTranscriptsPayload];

  EXPECT_OCMOCK_VERIFY(delegate_mock_);
}

// Tests that receiving an error envelope with a non-dictionary payload
// dispatches `kInternalBackendError`.
TEST_F(TTCWebSocketBackendTest, TestParseServerErrorWithNonDictionaryPayload) {
  OCMExpect([delegate_mock_ backend:backend_
                   didFailWithError:ttc::ErrorCode::kInternalBackendError]);

  NSDictionary* message = @{@"error" : @"An internal server error occurred."};
  NSData* payload = [NSJSONSerialization dataWithJSONObject:message
                                                    options:0
                                                      error:nil];
  [backend_ parseServerMessage:payload];

  EXPECT_OCMOCK_VERIFY(delegate_mock_);
}

// Tests that calling `sendAudioChunk:` when no task is active does not wedge
// backpressure counters or crash.
TEST_F(TTCWebSocketBackendTest, TestBackpressureWithNilTask) {
  const uint8_t rawBytes[] = {0x01, 0x02};
  NSData* samplePcm = [NSData dataWithBytes:rawBytes length:sizeof(rawBytes)];

  EXPECT_EQ(0u, backend_.inFlightSends);
  for (int i = 0; i < 10; ++i) {
    [backend_ sendAudioChunk:samplePcm];
  }
  EXPECT_EQ(0u, backend_.inFlightSends);
}

// Tests that `sendAudioChunk:` drops packets when in-flight sends reach the
// backpressure threshold.
TEST_F(TTCWebSocketBackendTest, TestBackpressureLimit) {
  const uint8_t rawBytes[] = {0x01, 0x02};
  NSData* samplePcm = [NSData dataWithBytes:rawBytes length:sizeof(rawBytes)];

  backend_.inFlightSends = 5;
  EXPECT_EQ(5u, backend_.inFlightSends);

  [backend_ sendAudioChunk:samplePcm];
  EXPECT_EQ(5u, backend_.inFlightSends);
}

// Tests that session cancellation error (NSURLErrorCancelled) or errors
// arriving after disconnect do not notify the delegate of a failure.
TEST_F(TTCWebSocketBackendTest, TestDisconnectCancellationNoError) {
  [[delegate_mock_ reject]
               backend:backend_
      didFailWithError:ttc::ErrorCode::kExecutionSessionCreationFailed];

  NSError* cancelError = [NSError errorWithDomain:NSURLErrorDomain
                                             code:NSURLErrorCancelled
                                         userInfo:nil];
  [backend_ handleFatalErrorWithError:cancelError];

  [backend_ disconnect];

  NSError* networkError = [NSError errorWithDomain:NSURLErrorDomain
                                              code:NSURLErrorTimedOut
                                          userInfo:nil];
  [backend_ handleFatalErrorWithError:networkError];

  EXPECT_OCMOCK_VERIFY(delegate_mock_);
}

// Tests that a fatal error transitions state to failed and notifies the
// delegate.
TEST_F(TTCWebSocketBackendTest, TestFatalErrorTriggersDelegateFailure) {
  OCMExpect([delegate_mock_
               backend:backend_
      didFailWithError:ttc::ErrorCode::kExecutionSessionCreationFailed]);

  [backend_ simulateHandshakingStateForTesting];

  NSError* error = [NSError errorWithDomain:NSURLErrorDomain
                                       code:NSURLErrorTimedOut
                                   userInfo:nil];
  [backend_ handleFatalErrorWithError:error];

  EXPECT_FALSE(backend_.isConnected);
  EXPECT_OCMOCK_VERIFY(delegate_mock_);
}

// Tests that connecting with an invalid or non-ws/wss URL scheme fails
// gracefully with kInternalBackendError without throwing an exception.
TEST_F(TTCWebSocketBackendTest, TestConnectFailsGracefullyWithInvalidScheme) {
  ios::provider::TTCConfig invalid_config{
      .system_instruction = kTestSystemInstruction,
      .model = kTestModel,
      .voice_name = kTestVoice,
      .endpoint_url = "https://example.com/not-a-websocket",
      .api_key = kTestApiKey,
  };
  ios::provider::test::SetTTCConfigForTesting(invalid_config);

  OCMExpect([delegate_mock_ backend:backend_
                   didFailWithError:ttc::ErrorCode::kInternalBackendError]);

  [backend_ connect];

  EXPECT_FALSE(backend_.isConnected);
  EXPECT_OCMOCK_VERIFY(delegate_mock_);
}

// Tests that duplicate fatal error calls do not trigger multiple delegate
// notifications.
TEST_F(TTCWebSocketBackendTest, TestDuplicateFatalErrorIgnored) {
  OCMExpect([delegate_mock_
               backend:backend_
      didFailWithError:ttc::ErrorCode::kExecutionSessionCreationFailed]);

  [backend_ simulateHandshakingStateForTesting];

  NSError* error = [NSError errorWithDomain:NSURLErrorDomain
                                       code:NSURLErrorTimedOut
                                   userInfo:nil];
  [backend_ handleFatalErrorWithError:error];

  // Subsequent fatal error while failed must be ignored.
  [backend_ handleFatalErrorWithError:error];

  EXPECT_OCMOCK_VERIFY(delegate_mock_);
}

// Tests that receiving an interrupted frame dispatches interruption and drops
// trailing audio and turn-completion from the same frame.
TEST_F(TTCWebSocketBackendTest,
       TestInterruptedMessageShortCircuitsTrailingAudioAndTurnComplete) {
  OCMExpect([delegate_mock_ backend:backend_
      didChangeGenerationStateStarted:NO
                            completed:NO
                          interrupted:YES]);
  [[delegate_mock_ reject] backend:backend_ didReceiveAudioOutput:[OCMArg any]];
  [[delegate_mock_ reject] backend:backend_
      didChangeGenerationStateStarted:[OCMArg any]
                            completed:YES
                          interrupted:[OCMArg any]];

  const uint8_t rawBytes[] = {0x11, 0x22};
  NSData* audioPcm = [NSData dataWithBytes:rawBytes length:sizeof(rawBytes)];
  NSString* base64 = [audioPcm base64EncodedStringWithOptions:0];

  NSDictionary* message = @{
    @"serverContent" : @{
      @"interrupted" : @YES,
      @"modelTurn" : @{
        @"parts" : @[ @{
          @"inlineData" : @{
            @"mimeType" : @"audio/pcm;rate=16000",
            @"data" : base64,
          }
        } ]
      },
      @"turnComplete" : @YES,
    }
  };
  NSData* payload = [NSJSONSerialization dataWithJSONObject:message
                                                    options:0
                                                      error:nil];
  [backend_ parseServerMessage:payload];

  EXPECT_OCMOCK_VERIFY(delegate_mock_);
}
