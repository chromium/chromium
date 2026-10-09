// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_conversation.h"

#import <stdint.h>

#import <memory>
#import <string>
#import <utility>
#import <vector>

#import "base/containers/span.h"
#import "base/memory/raw_ptr.h"
#import "components/ttc/app/public/error_codes.h"
#import "components/ttc/app/test_utils.h"
#import "components/ttc/app/ttc_backend.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/audio/ttc_audio_controller.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_conversation_delegate.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_error_codes.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::NiceMock;

// Expose internal audio delegate to unit tests to simulate engine callbacks.
@interface TTCConversation (Testing) <TTCAudioControllerDelegate>
@end

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

#pragma mark - Fake Conversation Delegate

@interface FakeTTCConversationDelegate : NSObject <TTCConversationDelegate>

@property(nonatomic, assign) float lastEnergy;
@property(nonatomic, strong) NSError* lastError;
@property(nonatomic, assign) BOOL didInitialize;
@property(nonatomic, assign) BOOL didClose;

@end

@implementation FakeTTCConversationDelegate

- (instancetype)init {
  self = [super init];
  if (self) {
    _lastEnergy = -1.0f;
  }
  return self;
}

- (void)conversation:(TTCConversation*)conversation
    didUpdateAudioEnergy:(float)energy {
  self.lastEnergy = energy;
}

- (void)conversationDidInitialize:(TTCConversation*)conversation {
  self.didInitialize = YES;
}

- (void)conversationDidClose:(TTCConversation*)conversation {
  self.didClose = YES;
}

- (void)conversation:(TTCConversation*)conversation
    didEncounterError:(NSError*)error {
  self.lastError = error;
}

@end

#pragma mark - Unit Tests

class TTCConversationTest : public PlatformTest {
 protected:
  TTCConversationTest() {
    fake_audio_controller_ = [[FakeTTCAudioController alloc] init];
    auto backend = std::make_unique<NiceMock<ttc::MockTtcBackend>>();
    mock_backend_ = backend.get();
    delegate_ = [[FakeTTCConversationDelegate alloc] init];
    conversation_ =
        [[TTCConversation alloc] initWithAudioController:fake_audio_controller_
                                                 backend:std::move(backend)];
    conversation_.delegate = delegate_;
  }

  ~TTCConversationTest() override {
    mock_backend_ = nullptr;
    conversation_ = nil;
  }

  web::WebTaskEnvironment task_environment_;
  FakeTTCAudioController* fake_audio_controller_ = nil;
  raw_ptr<NiceMock<ttc::MockTtcBackend>> mock_backend_ = nullptr;
  FakeTTCConversationDelegate* delegate_ = nil;
  TTCConversation* conversation_ = nil;
};

// Tests that start successfully begins capture and connects the backend.
TEST_F(TTCConversationTest, TestStartSuccess) {
  EXPECT_CALL(*mock_backend_, Connect(_));

  [conversation_ start];

  EXPECT_TRUE(fake_audio_controller_.isCapturing);
  EXPECT_TRUE(mock_backend_->is_transport_connected());
}

// Tests that start failure stops the conversation and forwards error to
// delegate.
TEST_F(TTCConversationTest, TestStartFailure) {
  NSError* simulated_error = [NSError errorWithDomain:@"TestDomain"
                                                 code:-100
                                             userInfo:nil];
  fake_audio_controller_.startCaptureBlock =
      ^(void (^completion)(BOOL, NSError*)) {
        completion(NO, simulated_error);
      };

  [conversation_ start];

  EXPECT_NSEQ(delegate_.lastError, simulated_error);
}

// Tests that stop halts capture and playback and disconnects the backend.
TEST_F(TTCConversationTest, TestStopHaltsCaptureAndPlayback) {
  [conversation_ start];

  EXPECT_CALL(*mock_backend_, Close());
  [conversation_ stop];

  EXPECT_TRUE(fake_audio_controller_.didStopCapture);
  EXPECT_TRUE(fake_audio_controller_.didStopPlayback);
  EXPECT_FALSE(mock_backend_->is_transport_connected());
}

// Tests that calling stop while capture is starting invalidates the async
// completion callback.
TEST_F(TTCConversationTest, TestStopInvalidatesAsyncStartCallback) {
  __block void (^saved_completion)(BOOL, NSError*) = nil;
  fake_audio_controller_.startCaptureBlock =
      ^(void (^completion)(BOOL, NSError*)) {
        saved_completion = [completion copy];
      };

  [conversation_ start];
  ASSERT_TRUE(saved_completion != nil);

  [conversation_ stop];

  // Invoke the delayed start completion with an error; should be ignored due
  // to generation increment.
  NSError* late_error = [NSError errorWithDomain:@"TestDomain"
                                            code:-1
                                        userInfo:nil];
  saved_completion(NO, late_error);
  EXPECT_NSEQ(delegate_.lastError, nil);
}

// Tests that barge-in is disallowed: loud microphone input does NOT
// interrupt assistant playback.
TEST_F(TTCConversationTest, TestBargeInDisallowedDuringPlayback) {
  [conversation_ start];
  ASSERT_TRUE(mock_backend_->observer());
  const int16_t raw_pcm[] = {0x0001};
  mock_backend_->observer()->OnAudioOutput(raw_pcm, 0);

  // Simulate loud microphone energy while talking.
  [conversation_ audioController:fake_audio_controller_
            didUpdateInputEnergy:0.95f];

  // Playback must NOT be stopped.
  EXPECT_FALSE(fake_audio_controller_.didStopPlayback);
  EXPECT_FALSE(fake_audio_controller_.didClearPlaybackQueue);
  EXPECT_FLOAT_EQ(delegate_.lastEnergy, 0.95f);
}

// Tests that audio controller errors are forwarded to the conversation
// delegate.
TEST_F(TTCConversationTest, TestAudioControllerErrorHandling) {
  [conversation_ start];

  NSError* error = [NSError errorWithDomain:@"TestAudioDomain"
                                       code:-50
                                   userInfo:nil];
  [conversation_ audioController:fake_audio_controller_
               didEncounterError:error];

  EXPECT_NSEQ(delegate_.lastError, error);
}

// Tests that external capture termination stops the conversation.
TEST_F(TTCConversationTest, TestAudioControllerExternalStopCapture) {
  [conversation_ start];

  EXPECT_CALL(*mock_backend_, Close());
  [conversation_ audioControllerDidStopCapture:fake_audio_controller_];

  EXPECT_TRUE(fake_audio_controller_.didStopPlayback);
  EXPECT_FALSE(mock_backend_->is_transport_connected());
}

// Tests that disconnect stops capture and playback, disconnects the audio
// controller, clears delegates, and is idempotent.
TEST_F(TTCConversationTest, TestDisconnect) {
  [conversation_ start];
  EXPECT_TRUE(fake_audio_controller_.isCapturing);

  [conversation_ disconnect];
  EXPECT_TRUE(fake_audio_controller_.didStopCapture);
  EXPECT_TRUE(fake_audio_controller_.didStopPlayback);
  EXPECT_TRUE(fake_audio_controller_.didDisconnect);
  EXPECT_EQ(conversation_.delegate, nil);
  EXPECT_EQ(fake_audio_controller_.delegate, nil);

  // Repeated calls should be safe and idempotent.
  EXPECT_NO_FATAL_FAILURE([conversation_ disconnect]);
}

// Tests that start connects to the backend.
TEST_F(TTCConversationTest, TestStartConnectsBackend) {
  EXPECT_CALL(*mock_backend_, Connect(_));
  [conversation_ start];
  EXPECT_TRUE(mock_backend_->is_transport_connected());
}

// Tests that stop disconnects the backend.
TEST_F(TTCConversationTest, TestStopDisconnectsBackend) {
  [conversation_ start];
  EXPECT_TRUE(mock_backend_->is_transport_connected());

  EXPECT_CALL(*mock_backend_, Close());
  [conversation_ stop];
  EXPECT_FALSE(mock_backend_->is_transport_connected());
}

// Tests that microphone chunks captured by the audio controller are forwarded
// to the backend.
TEST_F(TTCConversationTest, TestAudioCaptureForwardsToBackend) {
  [conversation_ start];

  const int16_t mic_pcm[] = {0x1122, 0x3344};
  NSData* chunk = [NSData dataWithBytes:mic_pcm length:sizeof(mic_pcm)];

  EXPECT_CALL(*mock_backend_, SendAudioChunk(ElementsAre(0x1122, 0x3344)));
  [conversation_ audioController:fake_audio_controller_
            didCaptureAudioChunk:chunk];
}

// Tests that backend initialization forwards to conversation delegate.
TEST_F(TTCConversationTest, TestBackendDidInitializeForwardsToDelegate) {
  [conversation_ start];
  ASSERT_TRUE(mock_backend_->observer());
  EXPECT_FALSE(delegate_.didInitialize);

  mock_backend_->observer()->OnBackendInitialized();

  EXPECT_TRUE(delegate_.didInitialize);
}

// Tests that backend closure forwards to conversation delegate.
TEST_F(TTCConversationTest, TestBackendDidCloseForwardsToDelegate) {
  [conversation_ start];
  ASSERT_TRUE(mock_backend_->observer());
  EXPECT_FALSE(delegate_.didClose);

  mock_backend_->observer()->OnBackendClosed();

  EXPECT_TRUE(delegate_.didClose);
}

// Tests that receiving audio output from the backend plays audio.
TEST_F(TTCConversationTest, TestBackendAudioOutputPlaysResponseAudio) {
  [conversation_ start];
  ASSERT_TRUE(mock_backend_->observer());

  const int16_t raw_pcm[] = {0x1234};
  NSData* expected_chunk = [NSData dataWithBytes:raw_pcm
                                          length:sizeof(raw_pcm)];

  mock_backend_->observer()->OnAudioOutput(raw_pcm, 0);

  EXPECT_NSEQ(fake_audio_controller_.lastPlayedChunk, expected_chunk);
  EXPECT_TRUE(fake_audio_controller_.isPlaying);
}

// Tests that generation interruption clears playback.
TEST_F(TTCConversationTest, TestBackendGenerationInterruptedClearsPlayback) {
  [conversation_ start];
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
TEST_F(TTCConversationTest, TestBackendErrorForwardsToDelegate) {
  [conversation_ start];
  ASSERT_TRUE(mock_backend_->observer());

  mock_backend_->observer()->OnBackendError(
      ttc::ErrorCode::kInternalBackendError);

  ASSERT_TRUE(delegate_.lastError != nil);
  EXPECT_NSEQ(delegate_.lastError.domain, kTTCErrorDomain);
  EXPECT_EQ(delegate_.lastError.code,
            static_cast<NSInteger>(ttc::ErrorCode::kInternalBackendError));
}

// Tests that microphone chunks captured by the audio controller are suppressed
// and dropped while response audio is playing (barge-in disabled).
TEST_F(TTCConversationTest, TestAudioCaptureSuppressedWhilePlaying) {
  [conversation_ start];
  ASSERT_TRUE(mock_backend_->observer());

  const int16_t raw_pcm[] = {0x1234};
  mock_backend_->observer()->OnAudioOutput(raw_pcm, 0);

  const int16_t mic_pcm[] = {0x5678};
  NSData* chunk = [NSData dataWithBytes:mic_pcm length:sizeof(mic_pcm)];
  EXPECT_CALL(*mock_backend_, SendAudioChunk(_)).Times(0);
  [conversation_ audioController:fake_audio_controller_
            didCaptureAudioChunk:chunk];
}

// Tests that calling `stop` is idempotent and safe against re-entrancy loops.
TEST_F(TTCConversationTest, TestStopIdempotentAndNoReentrantLoop) {
  [conversation_ start];
  ttc::TtcBackend::Observer* observer = mock_backend_->observer();
  ASSERT_TRUE(observer);

  [conversation_ stop];

  // Calling stop again or receiving closure must not trigger loops or crashes.
  [conversation_ stop];
  observer->OnBackendClosed();
  EXPECT_TRUE(fake_audio_controller_.didStopCapture);
}
