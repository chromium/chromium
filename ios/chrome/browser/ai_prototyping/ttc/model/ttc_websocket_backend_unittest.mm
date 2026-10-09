// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_websocket_backend.h"

#import <Foundation/Foundation.h>
#import <stdint.h>

#import <memory>
#import <string>

#import "base/containers/span.h"
#import "base/test/task_environment.h"
#import "components/ttc/app/public/error_codes.h"
#import "components/ttc/app/public/server_journal_event.h"
#import "components/ttc/app/public/tool_types.h"
#import "components/ttc/app/ttc_backend.h"
#import "ios/chrome/test/providers/intelligence/test_ttc_api.h"
#import "ios/public/provider/chrome/browser/intelligence/ttc_api.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::InvokeWithoutArgs;
using ::testing::StrictMock;

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

class MockTtcBackendObserver : public ttc::TtcBackend::Observer {
 public:
  MockTtcBackendObserver() = default;
  ~MockTtcBackendObserver() override = default;

  MOCK_METHOD(void, OnBackendInitialized, (), (override));
  MOCK_METHOD(void, OnBackendClosed, (), (override));
  MOCK_METHOD(void, OnBackendError, (ttc::ErrorCode), (override));
  MOCK_METHOD(void,
              OnTranscriptions,
              (const std::string&, const std::string&),
              (override));
  MOCK_METHOD(void,
              OnAudioOutput,
              (base::span<const int16_t>, int64_t),
              (override));
  MOCK_METHOD(void, OnGenerationStateChanged, (bool, bool, bool), (override));
  MOCK_METHOD(void,
              OnToolCall,
              (const ttc::ToolRequest&, ttc::ToolResponseCallback),
              (override));
  MOCK_METHOD(void,
              OnJournalEvent,
              (const ttc::ServerJournalEvent&),
              (override));
};

}  // namespace

class TtcWebSocketBackendTest : public PlatformTest {
 public:
  void SetUp() override {
    @autoreleasepool {
      PlatformTest::SetUp();
      ios::provider::test::SetTTCConfigForTesting(GetValidTestConfig());
      backend_ = std::make_unique<TtcWebSocketBackend>();
      backend_->SetObserverForTesting(&observer_);
    }
  }

  void TearDown() override {
    @autoreleasepool {
      backend_->Close();
      backend_.reset();
      ios::provider::test::ResetTTCConfigForTesting();
      PlatformTest::TearDown();
    }
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  StrictMock<MockTtcBackendObserver> observer_;
  std::unique_ptr<TtcWebSocketBackend> backend_;
};

// Tests that `CreateSetupPayload` produces a valid JSON structure
// conforming to the live model API specification.
TEST_F(TtcWebSocketBackendTest, TestCreateSetupPayload) {
  NSString* model = @"models/sample-model";
  NSString* voice = @"SampleVoice";
  NSString* instruction = @"Helpful persona.";

  NSData* payload =
      TtcWebSocketBackend::CreateSetupPayload(model, voice, instruction);
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

  NSDictionary* gen_config = setup[@"generationConfig"];
  ASSERT_TRUE(gen_config);
  EXPECT_NSEQ(@[ @"AUDIO" ], gen_config[@"responseModalities"]);

  NSDictionary* voice_config =
      gen_config[@"speechConfig"][@"voiceConfig"][@"prebuiltVoiceConfig"];
  ASSERT_TRUE(voice_config);
  EXPECT_NSEQ(voice, voice_config[@"voiceName"]);

  NSArray* parts = setup[@"systemInstruction"][@"parts"];
  ASSERT_TRUE(parts.count > 0);
  EXPECT_NSEQ(instruction, parts[0][@"text"]);
}

// Tests that `CreateAudioChunkPayload` correctly encodes linear PCM
// audio into a base64-encoded `realtimeInput` JSON payload.
TEST_F(TtcWebSocketBackendTest, TestCreateAudioChunkPayload) {
  const uint8_t raw_bytes[] = {0x01, 0x02, 0x03, 0x04};
  NSData* sample_pcm = [NSData dataWithBytes:raw_bytes
                                      length:sizeof(raw_bytes)];

  NSData* payload = TtcWebSocketBackend::CreateAudioChunkPayload(sample_pcm);
  ASSERT_TRUE(payload);

  NSError* error = nil;
  NSDictionary* root = [NSJSONSerialization JSONObjectWithData:payload
                                                       options:0
                                                         error:&error];
  ASSERT_TRUE(root);
  EXPECT_FALSE(error);

  NSDictionary* audio_chunk = root[@"realtimeInput"][@"audio"];
  ASSERT_TRUE(audio_chunk);
  EXPECT_NSEQ(@"audio/pcm;rate=16000", audio_chunk[@"mimeType"]);
  EXPECT_NSEQ([sample_pcm base64EncodedStringWithOptions:0],
              audio_chunk[@"data"]);

  // An empty audio chunk should produce nil.
  EXPECT_FALSE(TtcWebSocketBackend::CreateAudioChunkPayload([NSData data]));
}

// Tests that `CreateTextInputPayload` wraps user query text into a
// complete `clientContent` turn JSON payload.
TEST_F(TtcWebSocketBackendTest, TestCreateTextInputPayload) {
  NSString* query_text = @"What is the weather today?";
  NSData* payload = TtcWebSocketBackend::CreateTextInputPayload(query_text);
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
  EXPECT_NSEQ(query_text, turns[0][@"parts"][0][@"text"]);
  EXPECT_TRUE([root[@"clientContent"][@"turnComplete"] boolValue]);

  // Empty text should produce nil.
  EXPECT_FALSE(TtcWebSocketBackend::CreateTextInputPayload(@""));
}

// Tests that receiving `setupComplete` from the server transitions connection
// state to connected and notifies the observer when in handshaking state.
TEST_F(TtcWebSocketBackendTest, TestParseSetupComplete) {
  EXPECT_CALL(observer_, OnBackendInitialized());

  backend_->SimulateHandshakingStateForTesting();

  NSDictionary* message = @{@"setupComplete" : @{}};
  NSData* payload = [NSJSONSerialization dataWithJSONObject:message
                                                    options:0
                                                      error:nil];
  backend_->ParseServerMessageForTesting(payload);

  EXPECT_TRUE(backend_->is_transport_connected());
}

// Tests that `setupComplete` is ignored if the backend is not in the
// handshaking state.
TEST_F(TtcWebSocketBackendTest, TestParseSetupCompleteIgnoredIfNotHandshaking) {
  EXPECT_CALL(observer_, OnBackendInitialized()).Times(0);

  NSDictionary* message = @{@"setupComplete" : @{}};
  NSData* payload = [NSJSONSerialization dataWithJSONObject:message
                                                    options:0
                                                      error:nil];
  backend_->ParseServerMessageForTesting(payload);

  EXPECT_FALSE(backend_->is_transport_connected());
}

// Tests that receiving synthesized audio output chunks decodes base64 linear
// PCM and dispatches them to the observer.
TEST_F(TtcWebSocketBackendTest, TestParseAudioChunk) {
  const int16_t raw_samples[] = {0x1020, 0x3040};
  NSData* pcm_data = [NSData dataWithBytes:raw_samples
                                    length:sizeof(raw_samples)];
  NSString* base64_pcm = [pcm_data base64EncodedStringWithOptions:0];

  NSDictionary* message = @{
    @"serverContent" : @{
      @"modelTurn" : @{
        @"parts" : @[ @{
          @"inlineData" : @{
            @"mimeType" : @"audio/pcm;rate=24000",
            @"data" : base64_pcm,
          }
        } ]
      }
    }
  };

  EXPECT_CALL(observer_, OnAudioOutput(ElementsAre(0x1020, 0x3040), 0));

  NSData* payload = [NSJSONSerialization dataWithJSONObject:message
                                                    options:0
                                                      error:nil];
  backend_->ParseServerMessageForTesting(payload);
}

// Tests that receiving an interrupted signal dispatches a generation state
// update with `interrupted == true`.
TEST_F(TtcWebSocketBackendTest, TestParseInterruption) {
  EXPECT_CALL(observer_, OnGenerationStateChanged(false, false, true));

  NSDictionary* message = @{@"serverContent" : @{@"interrupted" : @YES}};
  NSData* payload = [NSJSONSerialization dataWithJSONObject:message
                                                    options:0
                                                      error:nil];
  backend_->ParseServerMessageForTesting(payload);
}

// Tests that receiving `turnComplete` dispatches a generation state update with
// `completed == true`.
TEST_F(TtcWebSocketBackendTest, TestParseTurnComplete) {
  EXPECT_CALL(observer_, OnGenerationStateChanged(false, true, false));

  NSDictionary* message = @{@"serverContent" : @{@"turnComplete" : @YES}};
  NSData* payload = [NSJSONSerialization dataWithJSONObject:message
                                                    options:0
                                                      error:nil];
  backend_->ParseServerMessageForTesting(payload);
}

// Tests that receiving streaming user and model transcriptions forwards them to
// the observer.
TEST_F(TtcWebSocketBackendTest, TestParseTranscriptions) {
  NSString* user_speech = @"Hello assistant";
  NSString* model_response = @"Hello! How can I help?";

  EXPECT_CALL(observer_,
              OnTranscriptions("Hello assistant", "Hello! How can I help?"));

  NSDictionary* message = @{
    @"serverContent" : @{
      @"inputTranscription" : @{@"text" : user_speech},
      @"outputTranscription" : @{@"text" : model_response},
    }
  };

  NSData* payload = [NSJSONSerialization dataWithJSONObject:message
                                                    options:0
                                                      error:nil];
  backend_->ParseServerMessageForTesting(payload);
}

// Tests that receiving an error envelope from the server dispatches the
// corresponding error to the observer.
TEST_F(TtcWebSocketBackendTest, TestParseServerErrorResponse) {
  EXPECT_CALL(observer_, OnBackendError(ttc::ErrorCode::kRateLimited));

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
  backend_->ParseServerMessageForTesting(payload);
}

// Tests that attempting to connect with an invalid or missing API key
// dispatches `kInternalBackendError` error.
TEST_F(TtcWebSocketBackendTest, TestConnectWithoutValidConfig) {
  ios::provider::test::SetTTCConfigForTesting(ios::provider::TTCConfig{});

  EXPECT_CALL(observer_, OnBackendError(ttc::ErrorCode::kInternalBackendError));

  backend_->Connect(&observer_);

  EXPECT_FALSE(backend_->is_transport_connected());
}

// Tests that calling `Close` stops the session and does NOT notify the
// observer that the backend closed, avoiding re-entrancy loops.
TEST_F(TtcWebSocketBackendTest, TestDisconnect) {
  EXPECT_CALL(observer_, OnBackendClosed()).Times(0);

  backend_->Close();

  EXPECT_FALSE(backend_->is_transport_connected());
}

// Tests that remote connection closure notifies the observer that the backend
// closed.
TEST_F(TtcWebSocketBackendTest, TestHandleConnectionClosed) {
  EXPECT_CALL(observer_, OnBackendClosed());

  backend_->HandleConnectionClosedForTesting();

  EXPECT_FALSE(backend_->is_transport_connected());
}

// Tests that `ParseServerMessageForTesting` defensively handles null,
// malformed, and missing fields without crashing or invoking unexpected
// observer callbacks.
TEST_F(TtcWebSocketBackendTest,
       TestParseServerMessageNullAndMalformedPayloads) {
  EXPECT_CALL(observer_, OnBackendInitialized()).Times(0);
  EXPECT_CALL(observer_, OnAudioOutput(_, _)).Times(0);
  EXPECT_CALL(observer_, OnTranscriptions(_, _)).Times(0);
  EXPECT_CALL(observer_, OnGenerationStateChanged(_, _, _)).Times(0);

  // 1. NSNull in root error: should be ignored cleanly.
  NSDictionary* null_error_msg = @{@"error" : [NSNull null]};
  NSData* null_error_payload =
      [NSJSONSerialization dataWithJSONObject:null_error_msg
                                      options:0
                                        error:nil];
  backend_->ParseServerMessageForTesting(null_error_payload);

  // 2. NSNull in setupComplete: should not transition to connected or notify.
  NSDictionary* null_setup_msg = @{@"setupComplete" : [NSNull null]};
  NSData* null_setup_payload =
      [NSJSONSerialization dataWithJSONObject:null_setup_msg
                                      options:0
                                        error:nil];
  backend_->ParseServerMessageForTesting(null_setup_payload);
  EXPECT_FALSE(backend_->is_transport_connected());

  // 3. NSNull in serverContent: should be ignored cleanly.
  NSDictionary* null_content_msg = @{@"serverContent" : [NSNull null]};
  NSData* null_content_payload =
      [NSJSONSerialization dataWithJSONObject:null_content_msg
                                      options:0
                                        error:nil];
  backend_->ParseServerMessageForTesting(null_content_payload);

  // 4. Malformed parts array containing NSNull and incomplete inlineData.
  NSDictionary* malformed_parts_msg = @{
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
  NSData* malformed_parts_payload =
      [NSJSONSerialization dataWithJSONObject:malformed_parts_msg
                                      options:0
                                        error:nil];
  backend_->ParseServerMessageForTesting(malformed_parts_payload);

  // 5. NSNull in transcriptions.
  NSDictionary* null_transcripts_msg = @{
    @"serverContent" : @{
      @"inputTranscription" : [NSNull null],
      @"outputTranscription" : [NSNull null],
    }
  };
  NSData* null_transcripts_payload =
      [NSJSONSerialization dataWithJSONObject:null_transcripts_msg
                                      options:0
                                        error:nil];
  backend_->ParseServerMessageForTesting(null_transcripts_payload);
}

// Tests that receiving an error envelope with a non-dictionary payload
// dispatches `kInternalBackendError`.
TEST_F(TtcWebSocketBackendTest, TestParseServerErrorWithNonDictionaryPayload) {
  EXPECT_CALL(observer_, OnBackendError(ttc::ErrorCode::kInternalBackendError));

  NSDictionary* message = @{@"error" : @"An internal server error occurred."};
  NSData* payload = [NSJSONSerialization dataWithJSONObject:message
                                                    options:0
                                                      error:nil];
  backend_->ParseServerMessageForTesting(payload);
}

// Tests that if the observer closes the backend during `OnTranscriptions` or
// `OnAudioOutput`, `ParseServerMessage` aborts and does not dispatch subsequent
// callbacks from the same message.
TEST_F(TtcWebSocketBackendTest, TestParseStopsWhenClosedMidMessage) {
  const int16_t raw_samples[] = {0x1020};
  NSData* pcm_data = [NSData dataWithBytes:raw_samples
                                    length:sizeof(raw_samples)];
  NSString* base64_pcm = [pcm_data base64EncodedStringWithOptions:0];

  // Case 1: Observer closes the backend inside `OnTranscriptions`.
  NSDictionary* message_with_transcript_and_audio = @{
    @"serverContent" : @{
      @"inputTranscription" : @{@"text" : @"Stop now"},
      @"modelTurn" : @{
        @"parts" : @[ @{
          @"inlineData" : @{
            @"mimeType" : @"audio/pcm;rate=24000",
            @"data" : base64_pcm,
          }
        } ]
      },
      @"turnComplete" : @YES,
    }
  };
  NSData* payload_1 =
      [NSJSONSerialization dataWithJSONObject:message_with_transcript_and_audio
                                      options:0
                                        error:nil];

  EXPECT_CALL(observer_, OnTranscriptions("Stop now", ""))
      .WillOnce(InvokeWithoutArgs([this]() { backend_->Close(); }));
  EXPECT_CALL(observer_, OnAudioOutput(_, _)).Times(0);
  EXPECT_CALL(observer_, OnGenerationStateChanged(_, _, _)).Times(0);

  backend_->ParseServerMessageForTesting(payload_1);

  // Case 2: Re-register observer and close the backend inside the first audio
  // part of a multi-part `modelTurn`.
  backend_->SetObserverForTesting(&observer_);
  NSDictionary* message_with_multiple_audio_parts = @{
    @"serverContent" : @{
      @"modelTurn" : @{
        @"parts" : @[
          @{
            @"inlineData" : @{
              @"mimeType" : @"audio/pcm;rate=24000",
              @"data" : base64_pcm,
            }
          },
          @{
            @"inlineData" : @{
              @"mimeType" : @"audio/pcm;rate=24000",
              @"data" : base64_pcm,
            }
          },
        ]
      },
      @"turnComplete" : @YES,
    }
  };
  NSData* payload_2 =
      [NSJSONSerialization dataWithJSONObject:message_with_multiple_audio_parts
                                      options:0
                                        error:nil];

  EXPECT_CALL(observer_, OnAudioOutput(ElementsAre(0x1020), 0))
      .WillOnce(InvokeWithoutArgs([this]() { backend_->Close(); }));
  EXPECT_CALL(observer_, OnGenerationStateChanged(_, _, _)).Times(0);

  backend_->ParseServerMessageForTesting(payload_2);
}

// Tests that calling `SendAudioChunk` when no task is active does not wedge
// backpressure counters or crash.
TEST_F(TtcWebSocketBackendTest, TestBackpressureWithNilTask) {
  const int16_t sample_pcm[] = {0x0201};

  EXPECT_EQ(0u, backend_->in_flight_sends_for_testing());
  for (int i = 0; i < 10; ++i) {
    backend_->SendAudioChunk(sample_pcm);
  }
  EXPECT_EQ(0u, backend_->in_flight_sends_for_testing());
}

// Tests that `SendAudioChunk` drops packets when in-flight sends reach the
// backpressure threshold.
TEST_F(TtcWebSocketBackendTest, TestBackpressureLimit) {
  const int16_t sample_pcm[] = {0x0201};

  backend_->set_in_flight_sends_for_testing(5);
  EXPECT_EQ(5u, backend_->in_flight_sends_for_testing());

  backend_->SendAudioChunk(sample_pcm);
  EXPECT_EQ(5u, backend_->in_flight_sends_for_testing());
}

// Tests that session cancellation error (NSURLErrorCancelled) or errors
// arriving after disconnect do not notify the observer of a failure.
TEST_F(TtcWebSocketBackendTest, TestDisconnectCancellationNoError) {
  EXPECT_CALL(observer_, OnBackendError(_)).Times(0);

  NSError* cancel_error = [NSError errorWithDomain:NSURLErrorDomain
                                              code:NSURLErrorCancelled
                                          userInfo:nil];
  backend_->HandleFatalErrorForTesting(cancel_error);

  backend_->Close();

  NSError* network_error = [NSError errorWithDomain:NSURLErrorDomain
                                               code:NSURLErrorTimedOut
                                           userInfo:nil];
  backend_->HandleFatalErrorForTesting(network_error);
}

// Tests that a fatal error transitions state to failed and notifies the
// observer.
TEST_F(TtcWebSocketBackendTest, TestFatalErrorTriggersDelegateFailure) {
  EXPECT_CALL(observer_,
              OnBackendError(ttc::ErrorCode::kExecutionSessionCreationFailed));

  backend_->SimulateHandshakingStateForTesting();

  NSError* error = [NSError errorWithDomain:NSURLErrorDomain
                                       code:NSURLErrorTimedOut
                                   userInfo:nil];
  backend_->HandleFatalErrorForTesting(error);

  EXPECT_FALSE(backend_->is_transport_connected());
}

// Tests that connecting with an invalid or non-ws/wss URL scheme fails
// gracefully with kInternalBackendError without throwing an exception.
TEST_F(TtcWebSocketBackendTest, TestConnectFailsGracefullyWithInvalidScheme) {
  ios::provider::TTCConfig invalid_config{
      .system_instruction = kTestSystemInstruction,
      .model = kTestModel,
      .voice_name = kTestVoice,
      .endpoint_url = "https://example.com/not-a-websocket",
      .api_key = kTestApiKey,
  };
  ios::provider::test::SetTTCConfigForTesting(invalid_config);

  EXPECT_CALL(observer_, OnBackendError(ttc::ErrorCode::kInternalBackendError));

  backend_->Connect(&observer_);

  EXPECT_FALSE(backend_->is_transport_connected());
}

// Tests that duplicate fatal error calls do not trigger multiple observer
// notifications.
TEST_F(TtcWebSocketBackendTest, TestDuplicateFatalErrorIgnored) {
  EXPECT_CALL(observer_,
              OnBackendError(ttc::ErrorCode::kExecutionSessionCreationFailed))
      .Times(1);

  backend_->SimulateHandshakingStateForTesting();

  NSError* error = [NSError errorWithDomain:NSURLErrorDomain
                                       code:NSURLErrorTimedOut
                                   userInfo:nil];
  backend_->HandleFatalErrorForTesting(error);

  // Subsequent fatal error while failed must be ignored.
  backend_->HandleFatalErrorForTesting(error);
}

// Tests that receiving an interrupted frame dispatches interruption and drops
// trailing audio and turn-completion from the same frame.
TEST_F(TtcWebSocketBackendTest,
       TestInterruptedMessageShortCircuitsTrailingAudioAndTurnComplete) {
  EXPECT_CALL(observer_, OnGenerationStateChanged(false, false, true));
  EXPECT_CALL(observer_, OnAudioOutput(_, _)).Times(0);
  EXPECT_CALL(observer_, OnGenerationStateChanged(_, true, _)).Times(0);

  const int16_t raw_samples[] = {0x2211};
  NSData* audio_pcm = [NSData dataWithBytes:raw_samples
                                     length:sizeof(raw_samples)];
  NSString* base64_pcm = [audio_pcm base64EncodedStringWithOptions:0];

  NSDictionary* message = @{
    @"serverContent" : @{
      @"interrupted" : @YES,
      @"modelTurn" : @{
        @"parts" : @[ @{
          @"inlineData" : @{
            @"mimeType" : @"audio/pcm;rate=16000",
            @"data" : base64_pcm,
          }
        } ]
      },
      @"turnComplete" : @YES,
    }
  };
  NSData* payload = [NSJSONSerialization dataWithJSONObject:message
                                                    options:0
                                                      error:nil];
  backend_->ParseServerMessageForTesting(payload);
}
