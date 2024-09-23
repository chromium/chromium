// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/voice/ui_bundled/text_to_speech_player.h"

#import <UIKit/UIKit.h>

#import "base/apple/bundle_locations.h"
#import "base/apple/foundation_util.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "ios/chrome/browser/voice/ui_bundled/voice_search_notification_names.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

#pragma mark - TTSPlayerObserver

// Test object that listens for TTS notifications.
@interface TTSPlayerObserver : NSObject

// The TextToSpeechPlayer passed on initialization.
@property(nonatomic, strong) TextToSpeechPlayer* player;

// Whether notifications have been received.
@property(nonatomic, readonly) BOOL readyNotificationReceived;
@property(nonatomic, readonly) BOOL willStartNotificationReceived;
@property(nonatomic, readonly) BOOL didStopNotificationReceived;

// Notification handlers.
- (void)handleReadyNotification:(NSNotification*)notification;
- (void)handleWillStartNotification:(NSNotification*)notification;
- (void)handleDidStopNotification:(NSNotification*)notification;

@end

@implementation TTSPlayerObserver
@synthesize player = _player;
@synthesize readyNotificationReceived = _readyNotificationReceived;
@synthesize willStartNotificationReceived = _willStartNotificationReceived;
@synthesize didStopNotificationReceived = _didStopNotificationReceived;

- (void)setPlayer:(TextToSpeechPlayer*)player {
  NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
  [defaultCenter removeObserver:self];
  _player = player;
  if (player) {
    [defaultCenter addObserver:self
                      selector:@selector(handleReadyNotification:)
                          name:kTTSAudioReadyForPlaybackNotification
                        object:player];
    [defaultCenter addObserver:self
                      selector:@selector(handleWillStartNotification:)
                          name:kTTSWillStartPlayingNotification
                        object:player];
    [defaultCenter addObserver:self
                      selector:@selector(handleDidStopNotification:)
                          name:kTTSDidStopPlayingNotification
                        object:player];
  }
}

- (TextToSpeechPlayer*)player {
  return _player;
}

- (void)handleReadyNotification:(NSNotification*)notification {
  ASSERT_EQ(notification.object, self.player);
  _readyNotificationReceived = YES;
}

- (void)handleWillStartNotification:(NSNotification*)notification {
  ASSERT_EQ(notification.object, self.player);
  _willStartNotificationReceived = YES;
}

- (void)handleDidStopNotification:(NSNotification*)notification {
  ASSERT_EQ(notification.object, self.player);
  _didStopNotificationReceived = YES;
}

@end

#pragma mark - TextToSpeechPlayerTest

class TextToSpeechPlayerTest : public PlatformTest {
 protected:
  void SetUp() override {
    tts_player_ = [[TextToSpeechPlayer alloc] init];
    tts_player_observer_ = [[TTSPlayerObserver alloc] init];
    [tts_player_observer_ setPlayer:tts_player_];
  }

  TextToSpeechPlayer* tts_player_;
  TTSPlayerObserver* tts_player_observer_;
  web::WebTaskEnvironment task_environment_;
};

// Tests that kTTSAudioReadyForPlaybackNotification is received and that
// TTSPlayer state is updated.
TEST_F(TextToSpeechPlayerTest, ReadyForPlayback) {
  NSData* audio_data = [@"audio_data" dataUsingEncoding:NSUTF8StringEncoding];
  [tts_player_ prepareToPlayAudioData:audio_data];
  EXPECT_TRUE([tts_player_observer_ readyNotificationReceived]);
  EXPECT_TRUE([tts_player_ isReadyForPlayback]);
}

// Tests that kTTSAudioReadyForPlaybackNotification is received and that
// TTSPlayer's `-readyForPlayback` is NO for empty data.
TEST_F(TextToSpeechPlayerTest, ReadyForPlaybackEmtpyData) {
  NSData* audio_data = [@"" dataUsingEncoding:NSUTF8StringEncoding];
  [tts_player_ prepareToPlayAudioData:audio_data];
  EXPECT_TRUE([tts_player_observer_ readyNotificationReceived]);
  EXPECT_FALSE([tts_player_ isReadyForPlayback]);
}

// Tests that kTTSWillStartPlayingNotification is received when playback begins
// and kTTSDidStopPlayingNotification is received when it is cancelled.
// TODO(crbug.com/333113564): Disabled because the bots do not have a valid
// sound output device.
TEST_F(TextToSpeechPlayerTest, DISABLED_ValidPlaybackNotifications) {
  NSString* path = [base::apple::FrameworkBundle()
      pathForResource:@"test_sound"
               ofType:@"m4a"
          inDirectory:@"ios/chrome/test/data/voice"];
  NSData* audio_data = [[NSData alloc] initWithContentsOfFile:path];
  [tts_player_ prepareToPlayAudioData:audio_data];
  [tts_player_ beginPlayback];
  EXPECT_TRUE([tts_player_observer_ willStartNotificationReceived]);
  EXPECT_TRUE([tts_player_ isPlayingAudio]);
  [tts_player_ cancelPlayback];
  EXPECT_TRUE([tts_player_observer_ didStopNotificationReceived]);
  EXPECT_FALSE([tts_player_ isPlayingAudio]);
}

// Tests that playback is cancelled when the application enters the background
// while playback is occurring.
// TODO(crbug.com/333113564): Disabled because the bots do not have a valid
// sound output device.
TEST_F(TextToSpeechPlayerTest, DISABLED_BackgroundNotification) {
  NSString* path = [base::apple::FrameworkBundle()
      pathForResource:@"test_sound"
               ofType:@"m4a"
          inDirectory:@"ios/chrome/test/data/voice"];
  NSData* audio_data = [[NSData alloc] initWithContentsOfFile:path];
  [tts_player_ prepareToPlayAudioData:audio_data];
  [tts_player_ beginPlayback];
  EXPECT_TRUE([tts_player_observer_ willStartNotificationReceived]);
  EXPECT_TRUE([tts_player_ isPlayingAudio]);
  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIApplicationDidEnterBackgroundNotification
                    object:[UIApplication sharedApplication]];
  EXPECT_TRUE([tts_player_observer_ didStopNotificationReceived]);
  EXPECT_FALSE([tts_player_ isPlayingAudio]);
}
