// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/audio/ttc_audio_player.h"

#import <AVFAudio/AVFAudio.h>
#import <Accelerate/Accelerate.h>

#import <algorithm>
#import <cmath>

#import "base/apple/foundation_util.h"
#import "base/compiler_specific.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/logging.h"
#import "base/sequence_checker.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/bind_post_task.h"
#import "base/task/sequenced_task_runner.h"

namespace {

// Standard sample rate for backend response audio playback (24kHz).
constexpr double kPlaybackSampleRate = 24000.0;

// Extra frame capacity headroom allocated during sample rate conversion.
constexpr AVAudioFrameCount kConverterFrameHeadroom = 16;

// Maximum number of pending loopback audio buffers allowed in the player node
// before purging stale backlog. In steady-state live loopback, double-buffering
// maintains at most 2 buffers (1 playing, 1 queued). Any higher backlog
// indicates output hardware stalling (e.g. during a route switch, Bluetooth
// negotiation, or device selector change) and should be purged immediately so
// loopback latency remains minimal (~128ms) rather than accumulating seconds of
// lag.
constexpr NSInteger kMaxPendingLoopbackBuffers = 1;

// Error domain for TTCAudioPlayer errors.
NSString* const kTTCAudioPlayerErrorDomain = @"org.chromium.ttc.player";

// Error codes for TTCAudioPlayer.
constexpr NSInteger kErrorCodePlayerAttachmentFailed = -1;

}  // namespace

namespace ttc {

void ConvertInt16ToFloat32(base::span<const int16_t> source,
                           base::span<float> destination) {
  size_t count = std::min(source.size(), destination.size());
  if (count == 0) {
    return;
  }
  const float scale = 32768.0f;
  vDSP_vflt16(source.data(), 1, destination.data(), 1, count);
  vDSP_vsdiv(destination.data(), 1, &scale, destination.data(), 1, count);
}

}  // namespace ttc

@implementation TTCAudioPlayer {
  // Audio player node attached to the shared AVAudioEngine graph.
  AVAudioPlayerNode* _playerNode;

  // Output format for synthesized response playback (24kHz mono Float32).
  AVAudioFormat* _playbackFormat;

  // Audio converter used for resampling arbitrary input buffers (e.g. 16kHz
  // loopback audio) to the 24kHz playback format.
  AVAudioConverter* _converter;

  // Whether response audio is actively playing through the player node.
  BOOL _isPlaying;

  // Monotonic token incremented on every playback stop or reset to invalidate
  // completion callbacks queued from prior sessions.
  uint64_t _playbackSessionId;

  // Number of audio buffers currently scheduled on the player node and
  // awaiting playback completion on the UI thread.
  NSInteger _pendingBuffersCount;

  // Used to ensure the object is accessed from the correct sequence.
  SEQUENCE_CHECKER(_sequenceChecker);
}

- (BOOL)isPlaying {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  return _isPlaying;
}

- (NSInteger)pendingBuffersCount {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  return _pendingBuffersCount;
}

- (instancetype)initWithPlayerNode:(AVAudioPlayerNode*)playerNode
                    playbackFormat:(AVAudioFormat*)playbackFormat {
  self = [super init];
  if (self) {
    _playerNode = playerNode;
    _playbackFormat = playbackFormat;
    _isPlaying = NO;
    _playbackSessionId = 0;
    _pendingBuffersCount = 0;
  }
  return self;
}

- (instancetype)init {
  AVAudioPlayerNode* playerNode = [[AVAudioPlayerNode alloc] init];
  AVAudioFormat* playbackFormat = [[AVAudioFormat alloc]
      initStandardFormatWithSampleRate:kPlaybackSampleRate
                              channels:1];
  return [self initWithPlayerNode:playerNode playbackFormat:playbackFormat];
}

#pragma mark - Public

- (BOOL)attachToAudioEngine:(AVAudioEngine*)audioEngine error:(NSError**)error {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (!audioEngine) {
    if (error) {
      *error =
          [NSError errorWithDomain:kTTCAudioPlayerErrorDomain
                              code:kErrorCodePlayerAttachmentFailed
                          userInfo:@{
                            NSLocalizedDescriptionKey : @"Audio engine is nil."
                          }];
    }
    return NO;
  }

  @try {
    if (_playerNode.engine != audioEngine) {
      [audioEngine attachNode:_playerNode];
    }
    [audioEngine connect:_playerNode
                      to:audioEngine.mainMixerNode
                  format:_playbackFormat];
    return YES;
  } @catch (NSException* exception) {
    @try {
      [audioEngine detachNode:_playerNode];
    } @catch (NSException* detachException) {
    }
    if (error) {
      *error = [NSError errorWithDomain:kTTCAudioPlayerErrorDomain
                                   code:kErrorCodePlayerAttachmentFailed
                               userInfo:@{
                                 NSLocalizedDescriptionKey : exception.reason
                                     ?: @"Failed to attach audio player node."
                               }];
    }
    return NO;
  }
}

- (void)detachFromAudioEngine:(AVAudioEngine*)audioEngine {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [self stopPlaybackImmediately];
  if (!audioEngine) {
    return;
  }

  @try {
    [audioEngine disconnectNodeOutput:_playerNode];
    [audioEngine detachNode:_playerNode];
  } @catch (NSException* exception) {
  }
}

- (void)playStreamingAudioChunk:(NSData*)pcm24kData {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  base::span<const uint8_t> byteSpan = base::apple::NSDataToSpan(pcm24kData);
  size_t sampleCount = byteSpan.size() / sizeof(int16_t);
  if (sampleCount == 0) {
    return;
  }

  AVAudioPCMBuffer* buffer =
      [[AVAudioPCMBuffer alloc] initWithPCMFormat:_playbackFormat
                                    frameCapacity:sampleCount];
  if (!buffer || !buffer.floatChannelData || !buffer.floatChannelData[0]) {
    return;
  }
  buffer.frameLength = sampleCount;

  // SAFETY: `byteSpan` is bounds-checked by `NSDataToSpan` to contain at least
  // `sampleCount * sizeof(int16_t)` bytes.
  auto sourceSpan = UNSAFE_BUFFERS(base::span(
      reinterpret_cast<const int16_t*>(byteSpan.data()), sampleCount));
  // SAFETY: `buffer.floatChannelData[0]` has capacity for `sampleCount` floats
  // guaranteed by `initWithPCMFormat:frameCapacity:` above.
  auto destSpan =
      UNSAFE_BUFFERS(base::span(buffer.floatChannelData[0], sampleCount));
  ttc::ConvertInt16ToFloat32(sourceSpan, destSpan);

  [self scheduleBuffer:buffer];
}

- (void)playPCMBuffer:(AVAudioPCMBuffer*)buffer {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (!buffer || buffer.frameLength == 0 || buffer.format.sampleRate <= 0.0) {
    return;
  }

  // Live mic loopback should never accumulate latency. If the output stalled
  // (e.g. during a route switch, Bluetooth negotiation, or device selector
  // change) and more than one buffer is already pending in the player queue,
  // purge the stale backlog so latency remains minimal.
  if (_pendingBuffersCount > kMaxPendingLoopbackBuffers) {
    [self purgePendingBuffers];
  }

  AVAudioPCMBuffer* playbackBuffer = buffer;
  if (![buffer.format isEqual:_playbackFormat]) {
    if (!_converter || ![_converter.inputFormat isEqual:buffer.format]) {
      _converter = [[AVAudioConverter alloc] initFromFormat:buffer.format
                                                   toFormat:_playbackFormat];
    }

    if (!_converter) {
      return;
    }

    double ratio = _playbackFormat.sampleRate / buffer.format.sampleRate;
    AVAudioFrameCount outputCapacity =
        static_cast<AVAudioFrameCount>(std::ceil(buffer.frameLength * ratio)) +
        kConverterFrameHeadroom;
    AVAudioPCMBuffer* convertedBuffer =
        [[AVAudioPCMBuffer alloc] initWithPCMFormat:_playbackFormat
                                      frameCapacity:outputCapacity];

    __block BOOL inputConsumed = NO;
    NSError* conversionError = nil;
    AVAudioConverterOutputStatus status =
        [_converter convertToBuffer:convertedBuffer
                              error:&conversionError
                 withInputFromBlock:^AVAudioBuffer*(
                     AVAudioPacketCount inNumberOfPackets,
                     AVAudioConverterInputStatus* outStatus) {
                   if (!inputConsumed) {
                     inputConsumed = YES;
                     *outStatus = AVAudioConverterInputStatus_HaveData;
                     return buffer;
                   }
                   *outStatus = AVAudioConverterInputStatus_NoDataNow;
                   return nil;
                 }];

    if (status == AVAudioConverterOutputStatus_Error) {
      DLOG(WARNING) << "Audio player resampling failed: "
                    << (conversionError
                            ? base::SysNSStringToUTF8(
                                  conversionError.localizedDescription)
                            : "Unknown error");
      if ([self.delegate
              respondsToSelector:@selector(audioPlayer:didEncounterError:)]) {
        [self.delegate audioPlayer:self didEncounterError:conversionError];
      }
      return;
    }

    if (convertedBuffer.frameLength == 0) {
      return;
    }
    playbackBuffer = convertedBuffer;
  }

  [self scheduleBuffer:playbackBuffer];
}

- (void)stopPlaybackImmediately {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  BOOL wasPlaying = _isPlaying || (_playerNode.engine && _playerNode.isPlaying);
  _isPlaying = NO;
  [self purgePendingBuffers];

  if (wasPlaying &&
      [self.delegate
          respondsToSelector:@selector(audioPlayerDidStopPlayback:)]) {
    [self.delegate audioPlayerDidStopPlayback:self];
  }
}

- (void)resumePlaybackAfterEngineRestart {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_isPlaying && _playerNode.engine && _playerNode.engine.isRunning &&
      !_playerNode.isPlaying) {
    @try {
      [_playerNode play];
    } @catch (NSException* exception) {
      DLOG(WARNING) << "Failed to resume player node: "
                    << base::SysNSStringToUTF8(exception.reason ?: @"Unknown");
      _isPlaying = NO;
    }
  }
}

- (void)reset {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  _isPlaying = NO;
  [self purgePendingBuffers];
  _converter = nil;
}

// Purges pending audio buffers from the player node, invalidating any queued
// completion callbacks and resetting the pending buffer count.
- (void)purgePendingBuffers {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  _playbackSessionId++;
  _pendingBuffersCount = 0;
  if (_playerNode.engine) {
    @try {
      if (_playerNode.isPlaying) {
        [_playerNode stop];
      }
      [_playerNode reset];
    } @catch (NSException* exception) {
      // Swallowing the exception is safe here: AVAudioPlayerNode can throw
      // an NSInternalInconsistencyException if the underlying AVAudioEngine
      // graph is stopped, in the middle of route reconfiguration, or if the
      // node was already detached. In all of these cases, playback is already
      // inactive or cannot continue, so ignoring the exception safely avoids
      // crashing the process during teardown.
      DLOG(WARNING) << "Ignoring exception while purging player node: "
                    << base::SysNSStringToUTF8(exception.reason ?: @"Unknown");
    }
  }
}

- (void)setIsPlayingForTesting:(BOOL)isPlaying {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  _isPlaying = isPlaying;
}

#pragma mark - Private

// Schedules `buffer` on the internal player node, starts node playback if
// needed, and notifies the delegate when buffer drain occurs on the UI thread.
- (void)scheduleBuffer:(AVAudioPCMBuffer*)buffer {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (!buffer || buffer.frameLength == 0 || !_playerNode.engine) {
    return;
  }

  uint64_t currentSessionId = _playbackSessionId;

  // Create a block that hop to the correct sequence and invoke the method
  // -handleScheduledBufferCompletionForSession:.
  __weak TTCAudioPlayer* weakSelf = self;
  void (^completion)(void) = base::CallbackToBlock(base::BindPostTask(
      base::SequencedTaskRunner::GetCurrentDefault(), base::BindOnce(^{
        [weakSelf handleScheduledBufferCompletionForSession:currentSessionId];
      })));

  @try {
    [_playerNode scheduleBuffer:buffer
                         atTime:nil
                        options:0
              completionHandler:completion];
  } @catch (NSException* exception) {
    DLOG(WARNING) << "Failed to schedule buffer on player node: "
                  << base::SysNSStringToUTF8(exception.reason ?: @"Unknown");
    return;
  }

  _pendingBuffersCount++;
  BOOL wasPlaying = _isPlaying;
  _isPlaying = YES;
  if (_playerNode.engine && _playerNode.engine.isRunning) {
    @try {
      if (!_playerNode.isPlaying) {
        [_playerNode play];
      }
    } @catch (NSException* exception) {
      DLOG(WARNING) << "Failed to start player node: "
                    << base::SysNSStringToUTF8(exception.reason ?: @"Unknown");
      _isPlaying = NO;
      if (_pendingBuffersCount > 0) {
        _pendingBuffersCount--;
      }
      return;
    }
  }

  if (!wasPlaying &&
      [self.delegate
          respondsToSelector:@selector(audioPlayerDidStartPlayback:)]) {
    [self.delegate audioPlayerDidStartPlayback:self];
  }
}

// Handles buffer completion on the main thread, updating the pending buffer
// count and notifying the delegate when all queued buffers finish.
- (void)handleScheduledBufferCompletionForSession:(uint64_t)sessionId {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (sessionId != _playbackSessionId) {
    return;
  }

  if (_pendingBuffersCount > 0) {
    _pendingBuffersCount--;
    if (_pendingBuffersCount == 0) {
      _isPlaying = NO;
      if (_playerNode.engine && _playerNode.isPlaying) {
        [_playerNode pause];
      }
      if ([self.delegate
              respondsToSelector:@selector(audioPlayerDidStopPlayback:)]) {
        [self.delegate audioPlayerDidStopPlayback:self];
      }
    }
  }
}

@end
