// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/audio/ttc_audio_recorder.h"

#import <AVFAudio/AVFAudio.h>

#import <cmath>

#import "base/check.h"
#import "base/containers/span.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/logging.h"
#import "base/sequence_checker.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/bind_post_task.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/audio/ttc_audio_metrics.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/audio/ttc_audio_recorder+testing.h"

namespace {

// In AVAudioEngine, AVAudioInputNode represents the audio input hardware and
// exposes a single input bus at index 0.
constexpr AVAudioNodeBus kAudioInputBus = 0;

// Tap buffer size of 2048 frames corresponds to ~128ms of audio at 16kHz.
constexpr AVAudioFrameCount kMicTapBufferSize = 2048;
constexpr double kMicSampleRate = 16000.0;

// Extra frame headroom to account for sample rate conversion filter delay.
constexpr AVAudioFrameCount kConverterFrameHeadroom = 16;

// Domain for errors originated by TTCAudioRecorder.
NSString* const kTTCAudioRecorderErrorDomain =
    @"org.chromium.ttc.audio_recorder";

// Error codes for TTCAudioRecorder.
constexpr NSInteger kErrorCodeTapInstallationFailed = -1;
constexpr NSInteger kErrorCodeInvalidInputFormat = -2;

// Output structure containing the resampled Float32 audio buffer and its
// calculated perceptual RMS energy level.
struct ProcessedAudioBuffer {
  float perceptual_energy = 0.0f;
  AVAudioPCMBuffer* resampled_buffer = nil;
};

// Pure, reentrant audio processing helper. Resamples incoming `buffer` to
// `target_format` using `converter` (if needed), computes RMS energy, and
// maps it to perceptual energy [0.0, 1.0].
// Safe to execute on the real-time audio thread without touching any shared
// Objective-C instance state.
ProcessedAudioBuffer ProcessInputBuffer(AVAudioFormat* target_format,
                                        AVAudioConverter* converter,
                                        AVAudioPCMBuffer* buffer,
                                        AVAudioTime* when) {
  if (!buffer || buffer.frameLength == 0 || buffer.format.sampleRate <= 0.0) {
    return {};
  }

  AVAudioPCMBuffer* resampled_buffer = buffer;

  // Resample hardware input buffer to 16kHz mono Float32 if necessary.
  if (![buffer.format isEqual:target_format]) {
    AVAudioConverter* active_converter = converter;
    if (!active_converter ||
        ![active_converter.inputFormat isEqual:buffer.format]) {
      active_converter =
          [[AVAudioConverter alloc] initFromFormat:buffer.format
                                          toFormat:target_format];
    }

    if (active_converter) {
      double ratio = target_format.sampleRate / buffer.format.sampleRate;
      AVAudioFrameCount outputCapacity =
          static_cast<AVAudioFrameCount>(
              std::ceil(buffer.frameLength * ratio)) +
          kConverterFrameHeadroom;
      AVAudioPCMBuffer* convertedBuffer =
          [[AVAudioPCMBuffer alloc] initWithPCMFormat:target_format
                                        frameCapacity:outputCapacity];

      __block BOOL inputConsumed = NO;
      NSError* conversionError = nil;
      AVAudioConverterOutputStatus status =
          [active_converter convertToBuffer:convertedBuffer
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
        DLOG(WARNING) << "Audio resampling failed: "
                      << (conversionError
                              ? base::SysNSStringToUTF8(
                                    conversionError.localizedDescription)
                              : "Unknown error");
      } else if (convertedBuffer.frameLength > 0) {
        resampled_buffer = convertedBuffer;
      }
    }
  }

  float* const* channelData = resampled_buffer.floatChannelData;
  if (!channelData || !channelData[0]) {
    return {};
  }

  AVAudioFrameCount frameCount = resampled_buffer.frameLength;
  // SAFETY: `channelData[0]` points to the Float32 audio channel buffer of
  // length `frameCount` maintained by `resampled_buffer`.
  auto samples = UNSAFE_BUFFERS(base::span(channelData[0], frameCount));

  // Calculate RMS energy and map to perceptual energy level for UI visualizer
  // using the modular ttc audio metrics utility.
  float rms = ttc::CalculateRMS(samples);
  float perceptualEnergy = ttc::LinearRmsToPerceptualLevel(rms);

  // If buffer was not resampled, clone the hardware tap buffer before returning
  // since CoreAudio recycles tap buffer (and TTCAudioRecorder will dispatch the
  // result to another thread).
  if (resampled_buffer == buffer) {
    resampled_buffer = [buffer copy];
  }

  return {
      .perceptual_energy = perceptualEnergy,
      .resampled_buffer = resampled_buffer,
  };
}

}  // namespace

@implementation TTCAudioRecorder {
  // Sequence checker ensuring all lifecycle methods are called on the creating
  // sequence.
  SEQUENCE_CHECKER(_sequenceChecker);

  // Target standard deinterleaved Float32 16kHz mono format for microphone
  // audio processing.
  AVAudioFormat* _micTapFormat;

  // Audio converter for resampling hardware input to 16kHz mono Float32.
  AVAudioConverter* _inputConverter;

  // Weak reference to the input node on which the tap is currently installed.
  __weak AVAudioInputNode* _tappedInputNode;

  // Format of `_tappedInputNode` when the tap was installed.
  AVAudioFormat* _tappedInputFormat;

  // Tracks whether the audio tap has been installed on the input node.
  BOOL _hasInstalledTap;

  // Flag indicating whether microphone capture is active.
  BOOL _isRecording;
}

@synthesize delegate = _delegate;

- (BOOL)isRecording {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  return _isRecording;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _micTapFormat =
        [[AVAudioFormat alloc] initStandardFormatWithSampleRate:kMicSampleRate
                                                       channels:1];
    _hasInstalledTap = NO;
    _isRecording = NO;
  }
  return self;
}

#pragma mark - Public

- (BOOL)installTapOnInputNode:(AVAudioInputNode*)inputNode
                        error:(NSError**)error {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  AVAudioFormat* inputFormat = [inputNode inputFormatForBus:kAudioInputBus];
  if (!inputFormat || inputFormat.sampleRate <= 0.0 ||
      inputFormat.channelCount == 0) {
    inputFormat = [inputNode outputFormatForBus:kAudioInputBus];
  }
  if (!inputFormat || inputFormat.sampleRate <= 0.0 ||
      inputFormat.channelCount == 0) {
    if (error) {
      *error = [NSError
          errorWithDomain:kTTCAudioRecorderErrorDomain
                     code:kErrorCodeInvalidInputFormat
                 userInfo:@{
                   NSLocalizedDescriptionKey : @"Audio input node format is "
                                               @"invalid or transiently zero."
                 }];
    }
    return NO;
  }

  if (_hasInstalledTap && _tappedInputNode == inputNode &&
      [_tappedInputFormat isEqual:inputFormat]) {
    _isRecording = YES;
    return YES;
  }

  AVAudioFormat* micTapFormat = _micTapFormat;
  AVAudioConverter* inputConverter = nil;
  if (![inputFormat isEqual:micTapFormat]) {
    inputConverter = [[AVAudioConverter alloc] initFromFormat:inputFormat
                                                     toFormat:micTapFormat];
#if TARGET_OS_SIMULATOR
    // `AVAudioConverter` on Simulator requires an explicit channel map during
    // downmixing to prevent outputting a silent buffer.
    if (inputFormat.channelCount > micTapFormat.channelCount) {
      NSMutableArray<NSNumber*>* channelMap = [NSMutableArray array];
      for (uint32_t i = 0; i < micTapFormat.channelCount; ++i) {
        [channelMap addObject:@(i)];
      }
      inputConverter.channelMap = channelMap;
    }
#endif  // TARGET_OS_SIMULATOR
  }
  _inputConverter = inputConverter;

  // Create a block that processes the input buffer on the CoreAudio thread
  // and then hops to the current sequence (which is likely different) to
  // invoke -bufferWasProcessed: to ensure the TTCAudioRecorder is never
  // accessed from a CoreAudio background thread.
  __weak TTCAudioRecorder* weakSelf = self;
  void (^block)(AVAudioPCMBuffer*, AVAudioTime*) = base::CallbackToBlock(
      base::BindRepeating(&ProcessInputBuffer, _micTapFormat, _inputConverter)
          .Then(base::BindPostTask(
              base::SequencedTaskRunner::GetCurrentDefault(),
              base::BindRepeating(^(ProcessedAudioBuffer processed) {
                [weakSelf bufferWasProcessed:std::move(processed)];
              }))));

  @try {
    [inputNode removeTapOnBus:kAudioInputBus];
    [inputNode installTapOnBus:kAudioInputBus
                    bufferSize:kMicTapBufferSize
                        format:inputFormat
                         block:block];
    _hasInstalledTap = YES;
    _tappedInputNode = inputNode;
    _tappedInputFormat = inputFormat;
  } @catch (NSException* exception) {
    if (error) {
      *error =
          [NSError errorWithDomain:kTTCAudioRecorderErrorDomain
                              code:kErrorCodeTapInstallationFailed
                          userInfo:@{
                            NSLocalizedDescriptionKey : exception.reason
                                ?: @"Failed to install microphone audio tap."
                          }];
    }
    return NO;
  }

  _isRecording = YES;
  return YES;
}

- (void)removeTapFromInputNode:(AVAudioInputNode*)inputNode {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (!_isRecording && !_hasInstalledTap) {
    return;
  }

  AVAudioInputNode* targetNode = inputNode ?: _tappedInputNode;
  if (_hasInstalledTap && targetNode) {
    @try {
      [targetNode removeTapOnBus:kAudioInputBus];
    } @catch (NSException* exception) {
      // Tap was already detached or node was invalidated.
    }
  }

  _hasInstalledTap = NO;
  _tappedInputNode = nil;
  _tappedInputFormat = nil;
  _inputConverter = nil;
  _isRecording = NO;
}

- (void)reset {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_hasInstalledTap && _tappedInputNode) {
    [self removeTapFromInputNode:_tappedInputNode];
  }
  _inputConverter = nil;
  _tappedInputNode = nil;
  _tappedInputFormat = nil;
  _hasInstalledTap = NO;
  _isRecording = NO;
}

#pragma mark - Private

- (void)handleInputBuffer:(AVAudioPCMBuffer*)buffer {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [self bufferWasProcessed:ProcessInputBuffer(_micTapFormat, _inputConverter,
                                              buffer, /*when=*/nil)];
}

// Invoked asynchronously on the TTCAudioRecorder's sequence after the input
// buffer has been processed on the CoreAudio thread (by ProcessInputBuffer).
// Dispatches the computed perceptual energy and resampled buffer to the
// delegate on the main thread.
- (void)bufferWasProcessed:(ProcessedAudioBuffer)processed {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (processed.resampled_buffer.frameLength == 0) {
    return;
  }

  if ([_delegate
          respondsToSelector:@selector(audioRecorder:didUpdateInputEnergy:)]) {
    [_delegate audioRecorder:self
        didUpdateInputEnergy:processed.perceptual_energy];
  }

  if ([_delegate
          respondsToSelector:@selector(audioRecorder:didCaptureBuffer:)]) {
    [_delegate audioRecorder:self didCaptureBuffer:processed.resampled_buffer];
  }
}

@end
