// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/voice/ui_bundled/text_to_speech_player.h"

#import <AVFoundation/AVFoundation.h>
#import <UIKit/UIKit.h>

#import "ios/chrome/browser/voice/ui_bundled/text_to_speech_player+subclassing.h"
#import "ios/chrome/browser/voice/ui_bundled/voice_search_notification_names.h"

@interface TextToSpeechPlayer ()<AVAudioPlayerDelegate> {
  // The audio data to be played.
  NSData* _audioData;
  // The AVAudioPlayer playing TTS audio data.
  AVAudioPlayer* _player;
  // Whether playback has finished.
  BOOL _playbackFinished;
}

// Cancels TTS audio playback, and sends a kTTSDidStopPlayingNotification
// notification if `sendNotification` is YES.
- (void)cancelPlaybackAndSendNotification:(BOOL)sendNotification;

@end

@implementation TextToSpeechPlayer

- (instancetype)init {
  if ((self = [super init])) {
    SEL handler = @selector(cancelPlayback);
    NSString* notificationName = UIApplicationDidEnterBackgroundNotification;
    id sender = [UIApplication sharedApplication];
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:handler
                                                 name:notificationName
                                               object:sender];
  }
  return self;
}

- (void)dealloc {
  [self cancelPlayback];
}

#pragma mark - Accessors

- (BOOL)isReadyForPlayback {
  return [_audioData length] > 0;
}

- (BOOL)isPlayingAudio {
  return [_player isPlaying];
}

- (AVAudioPlayer*)player {
  return _player;
}

#pragma mark - Public

- (void)prepareToPlayAudioData:(NSData*)audioData {
  if (self.playingAudio)
    [self cancelPlayback];
  _audioData = audioData;
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kTTSAudioReadyForPlaybackNotification
                    object:self];
}

- (void)beginPlayback {
  // no-op when audio is already playing.
  if (self.playingAudio || !self.readyForPlayback)
    return;
  // Create the AVAudioPlayer and initiate playback.
  _player = [[AVAudioPlayer alloc] initWithData:_audioData error:nil];
  [_player setMeteringEnabled:YES];
  [_player setDelegate:self];
  [_player setNumberOfLoops:0];
  [_player setVolume:1.0];
  if ([_player prepareToPlay] && [_player play]) {
    [[NSNotificationCenter defaultCenter]
        postNotificationName:kTTSWillStartPlayingNotification
                      object:self];
  } else {
    _player = nil;
  }
}

- (void)cancelPlayback {
  [self cancelPlaybackAndSendNotification:[_player isPlaying]];
}

#pragma mark - AVAudioPlayerDelegate

- (void)audioPlayerDecodeErrorDidOccur:(AVAudioPlayer*)player
                                 error:(NSError*)error {
  [self cancelPlayback];
}

- (void)audioPlayerDidFinishPlaying:(AVAudioPlayer*)player
                       successfully:(BOOL)flag {
  [self cancelPlaybackAndSendNotification:flag];
}

- (void)audioPlayerBeginInterruption:(AVAudioPlayer*)player {
  [self cancelPlayback];
}

#pragma mark -

- (void)cancelPlaybackAndSendNotification:(BOOL)sendNotification {
  if (_playbackFinished)
    return;
  _playbackFinished = YES;
  [_player stop];
  _audioData = nil;
  [_player setDelegate:nil];
  _player = nil;
  if (sendNotification) {
    [[NSNotificationCenter defaultCenter]
        postNotificationName:kTTSDidStopPlayingNotification
                      object:self];
  }
}

@end
