// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_conversation.h"

#import <stdint.h>

#import <memory>
#import <optional>
#import <string>
#import <utility>
#import <vector>

#import "base/containers/span.h"
#import "base/memory/raw_ptr.h"
#import "components/ttc/app/public/error_codes.h"
#import "components/ttc/app/test_utils.h"
#import "components/ttc/app/ttc_backend.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/audio/ttc_audio_controller.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_error_codes.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::NiceMock;

#pragma mark - Fake Audio Controller

@interface FakeTTCAudioController : NSObject <TTCAudioController>

@property(nonatomic, weak) id<TTCAudioControllerDelegate> delegate;
@property(nonatomic, assign, getter=isCapturing) BOOL capturing;
@property(nonatomic, assign, getter=isPlaying) BOOL playing;
@property(nonatomic, assign, getter=isLoopbackEnabled) BOOL loopbackEnabled;
@property(nonatomic, assign, getter=isOutputRoutedToSpeaker)
    BOOL outputRoutedToSpeaker;

@property(nonatomic, copy) void (^startCaptureBlock)
    (void (^completion)(BOOL success, NSError* error));
@property(nonatomic, strong) NSData* lastPlayedChunk;
@property(nonatomic, assign) BOOL didStopCapture;
@property(nonatomic, assign) BOOL didStopPlayback;
@property(nonatomic, assign) BOOL didClearPlaybackQueue;
@property(nonatomic, assign) BOOL didDisconnect;

@end

@implementation FakeTTCAudioController

- (void)startCaptureWithCompletion:(void (^)(BOOL success,
                                             NSError* error))completion {
  if (self.startCaptureBlock) {
    self.startCaptureBlock(completion);
    return;
  }
  self.capturing = YES;
  if (completion) {
    completion(YES, nil);
  }
}

- (void)stopCapture {
  self.capturing = NO;
  self.didStopCapture = YES;
}

- (void)playStreamingAudioChunk:(NSData*)pcm24kData {
  self.playing = YES;
  self.lastPlayedChunk = pcm24kData;
}

- (void)clearPlaybackQueue {
  self.didClearPlaybackQueue = YES;
}

- (void)stopPlayback {
  self.playing = NO;
  self.didStopPlayback = YES;
}

- (void)playTestTone {
}

- (void)stopTestTone {
}

- (void)disconnect {
  self.didDisconnect = YES;
}

@end

namespace {

// Fake `TtcConversation::Delegate` implementation for unit testing.
class FakeTtcConversationDelegate : public TtcConversation::Delegate {
 public:
  FakeTtcConversationDelegate() = default;
  ~FakeTtcConversationDelegate() override = default;

  float last_energy() const { return last_energy_; }
  std::optional<ttc::ErrorCode> last_error() const { return last_error_; }
  bool did_initialize() const { return did_initialize_; }
  bool did_close() const { return did_close_; }

  // `TtcConversation::Delegate` implementation:
  void OnConversationInitialized() override { did_initialize_ = true; }
  void OnConversationClosed() override { did_close_ = true; }
  void OnAudioEnergyUpdated(float energy) override { last_energy_ = energy; }
  void OnConversationError(ttc::ErrorCode error) override {
    last_error_ = error;
  }

 private:
  float last_energy_ = -1.0f;
  std::optional<ttc::ErrorCode> last_error_;
  bool did_initialize_ = false;
  bool did_close_ = false;
};

}  // namespace

#pragma mark - Unit Tests

class TtcConversationTest : public PlatformTest {
 protected:
  TtcConversationTest() {
    fake_audio_controller_ = [[FakeTTCAudioController alloc] init];
    auto backend = std::make_unique<NiceMock<ttc::MockTtcBackend>>();
    mock_backend_ = backend.get();
    conversation_ = std::make_unique<TtcConversation>(fake_audio_controller_,
                                                      std::move(backend));
    conversation_->set_delegate(&delegate_);
  }

  ~TtcConversationTest() override {
    testing::Mock::VerifyAndClearExpectations(mock_backend_);
    mock_backend_ = nullptr;
    conversation_.reset();
  }

  web::WebTaskEnvironment task_environment_;
  FakeTTCAudioController* fake_audio_controller_ = nil;
  raw_ptr<NiceMock<ttc::MockTtcBackend>> mock_backend_ = nullptr;
  FakeTtcConversationDelegate delegate_;
  std::unique_ptr<TtcConversation> conversation_;
};

// Tests that Start successfully begins capture and connects the backend.
TEST_F(TtcConversationTest, TestStartSuccess) {
  EXPECT_CALL(*mock_backend_, Connect(_));

  conversation_->Start();

  EXPECT_TRUE(fake_audio_controller_.isCapturing);
  EXPECT_TRUE(mock_backend_->is_transport_connected());
}

// Tests that Start failure forwards the mapped error code to the delegate.
TEST_F(TtcConversationTest, TestStartFailure) {
  NSError* simulated_error =
      CreateTTCError(ttc::ErrorCode::kAudioNoMicrophoneDetected);
  fake_audio_controller_.startCaptureBlock =
      ^(void (^completion)(BOOL, NSError*)) {
        completion(NO, simulated_error);
      };

  conversation_->Start();

  ASSERT_TRUE(delegate_.last_error().has_value());
  EXPECT_EQ(*delegate_.last_error(),
            ttc::ErrorCode::kAudioNoMicrophoneDetected);
}

// Tests that Stop halts capture and playback and disconnects the backend.
TEST_F(TtcConversationTest, TestStopHaltsCaptureAndPlayback) {
  conversation_->Start();

  EXPECT_CALL(*mock_backend_, Close());
  conversation_->Stop();

  EXPECT_TRUE(fake_audio_controller_.didStopCapture);
  EXPECT_TRUE(fake_audio_controller_.didStopPlayback);
  EXPECT_FALSE(mock_backend_->is_transport_connected());
}

// Tests that calling Stop while capture is starting invalidates the async
// completion callback.
TEST_F(TtcConversationTest, TestStopInvalidatesAsyncStartCallback) {
  __block void (^saved_completion)(BOOL, NSError*) = nil;
  fake_audio_controller_.startCaptureBlock =
      ^(void (^completion)(BOOL, NSError*)) {
        saved_completion = [completion copy];
      };

  conversation_->Start();
  ASSERT_TRUE(saved_completion != nil);

  conversation_->Stop();

  // Invoke the delayed start completion with an error; should be ignored due
  // to generation increment.
  NSError* late_error = CreateTTCError(ttc::ErrorCode::kAudioUnknownError);
  saved_completion(NO, late_error);
  EXPECT_FALSE(delegate_.last_error().has_value());
}

// Tests that barge-in is disallowed: loud microphone input does NOT
// interrupt assistant playback.
TEST_F(TtcConversationTest, TestBargeInDisallowedDuringPlayback) {
  conversation_->Start();
  ASSERT_TRUE(mock_backend_->observer());
  const int16_t raw_pcm[] = {0x0001};
  mock_backend_->observer()->OnAudioOutput(raw_pcm, 0);

  // Simulate loud microphone energy while talking.
  [fake_audio_controller_.delegate audioController:fake_audio_controller_
                              didUpdateInputEnergy:0.95f];

  // Playback must NOT be stopped.
  EXPECT_FALSE(fake_audio_controller_.didStopPlayback);
  EXPECT_FALSE(fake_audio_controller_.didClearPlaybackQueue);
  EXPECT_FLOAT_EQ(delegate_.last_energy(), 0.95f);
}

// Tests that audio controller errors are forwarded to the conversation
// delegate.
TEST_F(TtcConversationTest, TestAudioControllerErrorHandling) {
  conversation_->Start();

  NSError* error = CreateTTCError(ttc::ErrorCode::kAudioUnknownError);
  [fake_audio_controller_.delegate audioController:fake_audio_controller_
                                 didEncounterError:error];

  ASSERT_TRUE(delegate_.last_error().has_value());
  EXPECT_EQ(*delegate_.last_error(), ttc::ErrorCode::kAudioUnknownError);
}

// Tests that external capture termination stops the conversation.
TEST_F(TtcConversationTest, TestAudioControllerExternalStopCapture) {
  conversation_->Start();

  EXPECT_CALL(*mock_backend_, Close());
  [fake_audio_controller_.delegate
      audioControllerDidStopCapture:fake_audio_controller_];

  EXPECT_TRUE(fake_audio_controller_.didStopPlayback);
  EXPECT_FALSE(mock_backend_->is_transport_connected());
}

// Tests that Disconnect stops capture and playback, disconnects the audio
// controller, clears delegates, and is idempotent.
TEST_F(TtcConversationTest, TestDisconnect) {
  conversation_->Start();
  EXPECT_TRUE(fake_audio_controller_.isCapturing);

  conversation_->Disconnect();
  EXPECT_TRUE(fake_audio_controller_.didStopCapture);
  EXPECT_TRUE(fake_audio_controller_.didStopPlayback);
  EXPECT_TRUE(fake_audio_controller_.didDisconnect);
  EXPECT_EQ(conversation_->delegate(), nullptr);
  EXPECT_EQ(fake_audio_controller_.delegate, nil);

  // Repeated calls should be safe and idempotent.
  EXPECT_NO_FATAL_FAILURE(conversation_->Disconnect());
}

// Tests that Start connects to the backend.
TEST_F(TtcConversationTest, TestStartConnectsBackend) {
  EXPECT_CALL(*mock_backend_, Connect(_));
  conversation_->Start();
  EXPECT_TRUE(mock_backend_->is_transport_connected());
}

// Tests that Stop disconnects the backend.
TEST_F(TtcConversationTest, TestStopDisconnectsBackend) {
  conversation_->Start();
  EXPECT_TRUE(mock_backend_->is_transport_connected());

  EXPECT_CALL(*mock_backend_, Close());
  conversation_->Stop();
  EXPECT_FALSE(mock_backend_->is_transport_connected());
}

// Tests that microphone chunks captured by the audio controller are forwarded
// to the backend.
TEST_F(TtcConversationTest, TestAudioCaptureForwardsToBackend) {
  conversation_->Start();

  const int16_t mic_pcm[] = {0x1122, 0x3344};
  NSData* chunk = [NSData dataWithBytes:mic_pcm length:sizeof(mic_pcm)];

  EXPECT_CALL(*mock_backend_, SendAudioChunk(ElementsAre(0x1122, 0x3344)));
  [fake_audio_controller_.delegate audioController:fake_audio_controller_
                              didCaptureAudioChunk:chunk];
}

// Tests that backend initialization forwards to conversation delegate.
TEST_F(TtcConversationTest, TestBackendDidInitializeForwardsToDelegate) {
  conversation_->Start();
  ASSERT_TRUE(mock_backend_->observer());
  EXPECT_FALSE(delegate_.did_initialize());

  mock_backend_->observer()->OnBackendInitialized();

  EXPECT_TRUE(delegate_.did_initialize());
}

// Tests that backend closure forwards to conversation delegate.
TEST_F(TtcConversationTest, TestBackendDidCloseForwardsToDelegate) {
  conversation_->Start();
  ASSERT_TRUE(mock_backend_->observer());
  EXPECT_FALSE(delegate_.did_close());

  mock_backend_->observer()->OnBackendClosed();

  EXPECT_TRUE(delegate_.did_close());
}

// Tests that receiving audio output from the backend plays audio.
TEST_F(TtcConversationTest, TestBackendAudioOutputPlaysResponseAudio) {
  conversation_->Start();
  ASSERT_TRUE(mock_backend_->observer());

  const int16_t raw_pcm[] = {0x1234};
  NSData* expected_chunk = [NSData dataWithBytes:raw_pcm
                                          length:sizeof(raw_pcm)];

  mock_backend_->observer()->OnAudioOutput(raw_pcm, 0);

  EXPECT_NSEQ(fake_audio_controller_.lastPlayedChunk, expected_chunk);
  EXPECT_TRUE(fake_audio_controller_.isPlaying);
}

// Tests that generation interruption clears playback.
TEST_F(TtcConversationTest, TestBackendGenerationInterruptedClearsPlayback) {
  conversation_->Start();
  ASSERT_TRUE(mock_backend_->observer());

  const int16_t raw_pcm[] = {0x1234};
  mock_backend_->observer()->OnAudioOutput(raw_pcm, 0);

  mock_backend_->observer()->OnGenerationStateChanged(
      /*started=*/false, /*completed=*/false, /*interrupted=*/true);

  EXPECT_TRUE(fake_audio_controller_.didClearPlaybackQueue);
  EXPECT_TRUE(fake_audio_controller_.didStopPlayback);
}

// Tests that backend errors notify the conversation delegate with the mapped
// `ttc::ErrorCode`.
TEST_F(TtcConversationTest, TestBackendErrorForwardsToDelegate) {
  conversation_->Start();
  ASSERT_TRUE(mock_backend_->observer());

  mock_backend_->observer()->OnBackendError(
      ttc::ErrorCode::kInternalBackendError);

  ASSERT_TRUE(delegate_.last_error().has_value());
  EXPECT_EQ(*delegate_.last_error(), ttc::ErrorCode::kInternalBackendError);
}

// Tests that microphone chunks captured by the audio controller are suppressed
// and dropped while response audio is playing (barge-in disabled).
TEST_F(TtcConversationTest, TestAudioCaptureSuppressedWhilePlaying) {
  conversation_->Start();
  ASSERT_TRUE(mock_backend_->observer());

  const int16_t raw_pcm[] = {0x1234};
  mock_backend_->observer()->OnAudioOutput(raw_pcm, 0);

  const int16_t mic_pcm[] = {0x5678};
  NSData* chunk = [NSData dataWithBytes:mic_pcm length:sizeof(mic_pcm)];
  EXPECT_CALL(*mock_backend_, SendAudioChunk(_)).Times(0);
  [fake_audio_controller_.delegate audioController:fake_audio_controller_
                              didCaptureAudioChunk:chunk];
}

// Tests that calling `Stop()` is idempotent and safe against re-entrancy loops.
TEST_F(TtcConversationTest, TestStopIdempotentAndNoReentrantLoop) {
  conversation_->Start();
  ttc::TtcBackend::Observer* observer = mock_backend_->observer();
  ASSERT_TRUE(observer);

  conversation_->Stop();

  // Calling Stop again or receiving closure must not trigger loops or crashes.
  conversation_->Stop();
  observer->OnBackendClosed();
  EXPECT_TRUE(fake_audio_controller_.didStopCapture);
}
