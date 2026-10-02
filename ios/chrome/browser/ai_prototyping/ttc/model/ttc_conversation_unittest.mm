// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_conversation.h"

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_audio_controller.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_backend.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_conversation_delegate.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_conversation_state.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_error_codes.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

// Expose internal delegates to unit tests to simulate engine/backend callbacks.
@interface TTCConversation (Testing) <TTCAudioControllerDelegate,
                                      TTCBackendDelegate>
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

#pragma mark - Fake Backend

@interface FakeTTCBackend : NSObject <TTCBackend>

@property(nonatomic, weak) id<TTCBackendDelegate> delegate;
@property(nonatomic, assign, getter=isConnected) BOOL connected;
@property(nonatomic, assign) BOOL didConnect;
@property(nonatomic, assign) BOOL didDisconnect;
@property(nonatomic, strong) NSData* lastSentAudioChunk;
@property(nonatomic, strong) NSString* lastSentPrompt;

@end

@implementation FakeTTCBackend

- (void)connect {
  self.connected = YES;
  self.didConnect = YES;
}

- (void)disconnect {
  self.connected = NO;
  self.didDisconnect = YES;
}

- (void)sendAudioChunk:(NSData*)pcmData {
  self.lastSentAudioChunk = pcmData;
}

- (void)sendTextInput:(NSString*)text {
  self.lastSentPrompt = text;
}

- (void)sendContextUpdateWithURL:(const GURL&)url
                           title:(NSString*)title
                         content:(NSString*)content {
}

- (void)reportPlaybackStatus:(int64_t)lastPlayedSequenceNumber {
}

@end

#pragma mark - Fake Conversation Delegate

@interface FakeTTCConversationDelegate : NSObject <TTCConversationDelegate>

@property(nonatomic, assign) TTCConversationState lastState;
@property(nonatomic, assign) NSInteger stateChangeCount;
@property(nonatomic, strong) NSData* lastCapturedChunk;
@property(nonatomic, assign) float lastEnergy;
@property(nonatomic, strong) NSError* lastError;
@property(nonatomic, assign) BOOL didInitialize;

@end

@implementation FakeTTCConversationDelegate

- (instancetype)init {
  self = [super init];
  if (self) {
    _lastState = TTCConversationState::kStopped;
    _stateChangeCount = 0;
    _lastEnergy = -1.0f;
  }
  return self;
}

- (void)conversation:(TTCConversation*)conversation
      didChangeState:(TTCConversationState)state {
  self.lastState = state;
  self.stateChangeCount++;
}

- (void)conversation:(TTCConversation*)conversation
    didCaptureAudioChunk:(NSData*)pcmData {
  self.lastCapturedChunk = pcmData;
}

- (void)conversation:(TTCConversation*)conversation
    didUpdateAudioEnergy:(float)energy {
  self.lastEnergy = energy;
}

- (void)conversationDidInitialize:(TTCConversation*)conversation {
  self.didInitialize = YES;
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
    fake_backend_ = [[FakeTTCBackend alloc] init];
    delegate_ = [[FakeTTCConversationDelegate alloc] init];
    conversation_ =
        [[TTCConversation alloc] initWithAudioController:fake_audio_controller_
                                                 backend:fake_backend_];
    conversation_.delegate = delegate_;
  }

  web::WebTaskEnvironment task_environment_;
  FakeTTCAudioController* fake_audio_controller_ = nil;
  FakeTTCBackend* fake_backend_ = nil;
  FakeTTCConversationDelegate* delegate_ = nil;
  TTCConversation* conversation_ = nil;
};

// Tests that initial state is stopped with no error.
TEST_F(TTCConversationTest, TestInitialState) {
  EXPECT_EQ(conversation_.state, TTCConversationState::kStopped);
  EXPECT_NSEQ(conversation_.lastError, nil);
  EXPECT_EQ(delegate_.stateChangeCount, 0);
}

// Tests that start successfully begins capture and transitions to listening.
TEST_F(TTCConversationTest, TestStartSuccess) {
  [conversation_ start];

  EXPECT_EQ(conversation_.state, TTCConversationState::kListening);
  EXPECT_EQ(delegate_.lastState, TTCConversationState::kListening);
  EXPECT_EQ(delegate_.stateChangeCount, 1);
  EXPECT_TRUE(fake_audio_controller_.isCapturing);
}

// Tests that start failure transitions to stopped and forwards error to
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

  EXPECT_EQ(conversation_.state, TTCConversationState::kStopped);
  EXPECT_NSEQ(conversation_.lastError, simulated_error);
  EXPECT_NSEQ(delegate_.lastError, simulated_error);
}

// Tests that stop halts capture and playback and transitions to stopped.
TEST_F(TTCConversationTest, TestStopHaltsCaptureAndPlayback) {
  [conversation_ start];
  EXPECT_EQ(conversation_.state, TTCConversationState::kListening);

  [conversation_ stop];

  EXPECT_EQ(conversation_.state, TTCConversationState::kStopped);
  EXPECT_EQ(delegate_.lastState, TTCConversationState::kStopped);
  EXPECT_TRUE(fake_audio_controller_.didStopCapture);
  EXPECT_TRUE(fake_audio_controller_.didStopPlayback);
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
  EXPECT_EQ(conversation_.state, TTCConversationState::kStopped);
  ASSERT_TRUE(saved_completion != nil);

  [conversation_ stop];
  EXPECT_EQ(conversation_.state, TTCConversationState::kStopped);

  // Invoke the delayed start completion; should be ignored due to generation
  // increment.
  saved_completion(YES, nil);
  EXPECT_EQ(conversation_.state, TTCConversationState::kStopped);
}

// Tests that delivering response audio transitions state to talking and passes
// PCM chunks to the audio controller.
TEST_F(TTCConversationTest, TestOnResponseTransitionsToTalking) {
  [conversation_ start];
  EXPECT_EQ(conversation_.state, TTCConversationState::kListening);

  const uint8_t raw_pcm[] = {0x12, 0x34, 0x56, 0x78};
  NSData* chunk = [NSData dataWithBytes:raw_pcm length:sizeof(raw_pcm)];

  [conversation_ playResponseAudio:chunk];

  EXPECT_EQ(conversation_.state, TTCConversationState::kTalking);
  EXPECT_EQ(delegate_.lastState, TTCConversationState::kTalking);
  EXPECT_NSEQ(fake_audio_controller_.lastPlayedChunk, chunk);
  EXPECT_TRUE(fake_audio_controller_.isPlaying);
}

// Tests that natural playback completion transitions state back to listening.
TEST_F(TTCConversationTest, TestPlaybackCompletionTransitionsToListening) {
  [conversation_ start];
  const uint8_t raw_pcm[] = {0x00, 0x01};
  [conversation_ playResponseAudio:[NSData dataWithBytes:raw_pcm length:2]];
  EXPECT_EQ(conversation_.state, TTCConversationState::kTalking);

  // Audio player finishes rendering queued buffers.
  [conversation_ audioControllerDidStopPlayback:fake_audio_controller_];

  EXPECT_EQ(conversation_.state, TTCConversationState::kListening);
  EXPECT_EQ(delegate_.lastState, TTCConversationState::kListening);
}

// Tests that explicitly finishing a turn transitions state from talking back to
// listening, and is a no-op when not in talking state.
TEST_F(TTCConversationTest, TestFinishTurnTransitionsToListening) {
  [conversation_ start];
  const uint8_t raw_pcm[] = {0x00, 0x01};
  [conversation_ playResponseAudio:[NSData dataWithBytes:raw_pcm length:2]];
  EXPECT_EQ(conversation_.state, TTCConversationState::kTalking);

  [conversation_ finishTurn];

  EXPECT_EQ(conversation_.state, TTCConversationState::kListening);
  EXPECT_EQ(delegate_.lastState, TTCConversationState::kListening);

  // Repeated call when already listening is a no-op.
  [conversation_ finishTurn];
  EXPECT_EQ(conversation_.state, TTCConversationState::kListening);
}

// Tests that barge-in is disallowed: loud microphone input does NOT
// interrupt assistant playback.
TEST_F(TTCConversationTest, TestBargeInDisallowedDuringPlayback) {
  [conversation_ start];
  const uint8_t raw_pcm[] = {0x00, 0x01};
  [conversation_ playResponseAudio:[NSData dataWithBytes:raw_pcm length:2]];
  EXPECT_EQ(conversation_.state, TTCConversationState::kTalking);

  // Simulate loud microphone energy while talking.
  [conversation_ audioController:fake_audio_controller_
            didUpdateInputEnergy:0.95f];

  // State must remain talking and playback must NOT be stopped.
  EXPECT_EQ(conversation_.state, TTCConversationState::kTalking);
  EXPECT_FALSE(fake_audio_controller_.didStopPlayback);
  EXPECT_FALSE(fake_audio_controller_.didClearPlaybackQueue);
  EXPECT_FLOAT_EQ(delegate_.lastEnergy, 0.95f);
}

// Tests that microphone PCM chunks are delivered to the delegate.
TEST_F(TTCConversationTest, TestMicAudioChunkForwarding) {
  [conversation_ start];

  const uint8_t mic_pcm[] = {0xAA, 0xBB};
  NSData* chunk = [NSData dataWithBytes:mic_pcm length:sizeof(mic_pcm)];

  [conversation_ audioController:fake_audio_controller_
            didCaptureAudioChunk:chunk];

  EXPECT_NSEQ(delegate_.lastCapturedChunk, chunk);
}

// Tests that audio controller errors halt the conversation and notify the
// delegate.
TEST_F(TTCConversationTest, TestAudioControllerErrorHandling) {
  [conversation_ start];
  EXPECT_EQ(conversation_.state, TTCConversationState::kListening);

  NSError* error = [NSError errorWithDomain:@"TestAudioDomain"
                                       code:-50
                                   userInfo:nil];
  [conversation_ audioController:fake_audio_controller_
               didEncounterError:error];

  EXPECT_EQ(conversation_.state, TTCConversationState::kStopped);
  EXPECT_NSEQ(conversation_.lastError, error);
  EXPECT_NSEQ(delegate_.lastError, error);
}

// Tests that external capture termination stops the conversation.
TEST_F(TTCConversationTest, TestAudioControllerExternalStopCapture) {
  [conversation_ start];
  EXPECT_EQ(conversation_.state, TTCConversationState::kListening);

  [conversation_ audioControllerDidStopCapture:fake_audio_controller_];

  EXPECT_EQ(conversation_.state, TTCConversationState::kStopped);
}

// Tests that disconnect stops capture and playback, clears delegates, and is
// idempotent.
TEST_F(TTCConversationTest, TestDisconnect) {
  [conversation_ start];
  EXPECT_EQ(conversation_.state, TTCConversationState::kListening);
  EXPECT_TRUE(fake_audio_controller_.isCapturing);

  [conversation_ disconnect];
  EXPECT_EQ(conversation_.state, TTCConversationState::kStopped);
  EXPECT_TRUE(fake_audio_controller_.didStopCapture);
  EXPECT_TRUE(fake_audio_controller_.didStopPlayback);
  EXPECT_EQ(conversation_.delegate, nil);
  EXPECT_EQ(fake_audio_controller_.delegate, nil);

  // Repeated calls should be safe and idempotent.
  EXPECT_NO_FATAL_FAILURE([conversation_ disconnect]);
}

// Tests that start connects to the backend.
TEST_F(TTCConversationTest, TestStartConnectsBackend) {
  [conversation_ start];
  EXPECT_TRUE(fake_backend_.didConnect);
}

// Tests that stop disconnects the backend.
TEST_F(TTCConversationTest, TestStopDisconnectsBackend) {
  [conversation_ start];
  EXPECT_TRUE(fake_backend_.didConnect);

  [conversation_ stop];
  EXPECT_TRUE(fake_backend_.didDisconnect);
}

// Tests that microphone chunks captured by the audio controller are forwarded
// to the backend.
TEST_F(TTCConversationTest, TestAudioCaptureForwardsToBackend) {
  [conversation_ start];

  const uint8_t mic_pcm[] = {0xAA, 0xBB};
  NSData* chunk = [NSData dataWithBytes:mic_pcm length:sizeof(mic_pcm)];

  [conversation_ audioController:fake_audio_controller_
            didCaptureAudioChunk:chunk];

  EXPECT_NSEQ(fake_backend_.lastSentAudioChunk, chunk);
}

// Tests that backend initialization forwards to conversation delegate.
TEST_F(TTCConversationTest, TestBackendDidInitializeForwardsToDelegate) {
  EXPECT_FALSE(delegate_.didInitialize);

  [conversation_ backendDidInitialize:fake_backend_];

  EXPECT_TRUE(delegate_.didInitialize);
}

// Tests that receiving audio output from the backend plays audio and
// transitions to talking state.
TEST_F(TTCConversationTest, TestBackendAudioOutputPlaysResponseAudio) {
  [conversation_ start];
  EXPECT_EQ(conversation_.state, TTCConversationState::kListening);

  const uint8_t raw_pcm[] = {0x12, 0x34};
  NSData* chunk = [NSData dataWithBytes:raw_pcm length:sizeof(raw_pcm)];

  [conversation_ backend:fake_backend_
      didReceiveAudioOutput:chunk
             sequenceNumber:1];

  EXPECT_EQ(conversation_.state, TTCConversationState::kTalking);
  EXPECT_NSEQ(fake_audio_controller_.lastPlayedChunk, chunk);
  EXPECT_TRUE(fake_audio_controller_.isPlaying);
}

// Tests that generation interruption clears playback and returns to listening.
TEST_F(TTCConversationTest, TestBackendGenerationInterruptedClearsPlayback) {
  [conversation_ start];

  const uint8_t raw_pcm[] = {0x12, 0x34};
  NSData* chunk = [NSData dataWithBytes:raw_pcm length:sizeof(raw_pcm)];
  [conversation_ backend:fake_backend_
      didReceiveAudioOutput:chunk
             sequenceNumber:1];
  EXPECT_EQ(conversation_.state, TTCConversationState::kTalking);

  [conversation_ backend:fake_backend_
      didChangeGenerationStateStarted:NO
                            completed:NO
                          interrupted:YES];

  EXPECT_TRUE(fake_audio_controller_.didClearPlaybackQueue);
  EXPECT_TRUE(fake_audio_controller_.didStopPlayback);
  EXPECT_EQ(conversation_.state, TTCConversationState::kListening);
}

// Tests that backend errors stop the conversation and notify the delegate.
TEST_F(TTCConversationTest, TestBackendErrorHaltsConversation) {
  [conversation_ start];
  EXPECT_EQ(conversation_.state, TTCConversationState::kListening);

  [conversation_ backend:fake_backend_
        didFailWithError:TTCErrorCode::kNetworkError];

  EXPECT_EQ(conversation_.state, TTCConversationState::kStopped);
  ASSERT_TRUE(conversation_.lastError != nil);
  EXPECT_NSEQ(conversation_.lastError.domain, kTTCErrorDomain);
  EXPECT_EQ(conversation_.lastError.code,
            static_cast<NSInteger>(TTCErrorCode::kNetworkError));
  EXPECT_NSEQ(delegate_.lastError, conversation_.lastError);
}
