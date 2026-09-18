// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_audio_recorder.h"

#import <AVFAudio/AVFAudio.h>

#import <cmath>

#import "base/check.h"
#import "base/compiler_specific.h"
#import "base/containers/span.h"
#import "base/functional/bind.h"
#import "base/logging.h"
#import "base/sequence_checker.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_audio_metrics.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"

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
ProcessedAudioBuffer ProcessInputBuffer(AVAudioPCMBuffer* buffer,
                                        AVAudioFormat* target_format,
                                        AVAudioConverter* converter) {
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
  if (_isRecording) {
    return YES;
  }

  if (!_hasInstalledTap) {
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

    AVAudioFormat* micTapFormat = _micTapFormat;
    AVAudioConverter* inputConverter = nil;
    if (![inputFormat isEqual:micTapFormat]) {
      inputConverter = [[AVAudioConverter alloc] initFromFormat:inputFormat
                                                       toFormat:micTapFormat];
    }
    _inputConverter = inputConverter;

    __weak TTCAudioRecorder* weakSelf = self;
    @try {
      [inputNode
          installTapOnBus:kAudioInputBus
               bufferSize:kMicTapBufferSize
                   format:inputFormat
                    block:^(AVAudioPCMBuffer* buffer, AVAudioTime* when) {
                      [weakSelf handleInputBuffer:buffer
                                     targetFormat:micTapFormat
                                        converter:inputConverter];
                    }];
      _hasInstalledTap = YES;
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
  }

  _isRecording = YES;
  return YES;
}

- (void)removeTapFromInputNode:(AVAudioInputNode*)inputNode {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (!_isRecording && !_hasInstalledTap) {
    return;
  }

  if (_hasInstalledTap && inputNode) {
    @try {
      [inputNode removeTapOnBus:kAudioInputBus];
    } @catch (NSException* exception) {
      // Tap was already detached or node was invalidated.
    }
    _hasInstalledTap = NO;
  }

  _inputConverter = nil;
  _isRecording = NO;
}

- (void)reset {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  _inputConverter = nil;
  DCHECK(!_hasInstalledTap)
      << "reset called while tap was still installed on input node.";
  _hasInstalledTap = NO;
  _isRecording = NO;
}

#pragma mark - Private

// Processes incoming audio buffers from unit tests on the UI thread,
// forwarding with current instance formats.
- (void)handleInputBuffer:(AVAudioPCMBuffer*)buffer {
  [self handleInputBuffer:buffer
             targetFormat:_micTapFormat
                converter:_inputConverter];
}

// Processes incoming audio buffers from the microphone tap (or unit tests),
// resamples to 16kHz mono Float32 via AVAudioConverter if needed, computes
// perceptual energy, and dispatches to the delegate on the UI thread.
// Operates strictly on passed parameters without reading instance variables,
// ensuring thread safety when invoked from the CoreAudio thread.
- (void)handleInputBuffer:(AVAudioPCMBuffer*)buffer
             targetFormat:(AVAudioFormat*)targetFormat
                converter:(AVAudioConverter*)converter {
  @autoreleasepool {
    if (!web::WebThread::IsThreadInitialized(web::WebThread::UI)) {
      return;
    }

    ProcessedAudioBuffer processed =
        ProcessInputBuffer(buffer, targetFormat, converter);
    if (!processed.resampled_buffer ||
        processed.resampled_buffer.frameLength == 0) {
      return;
    }

    // If buffer was not resampled, clone the hardware tap buffer before
    // dispatching across threads since CoreAudio recycles tap buffers.
    AVAudioPCMBuffer* capturedBuffer = (processed.resampled_buffer == buffer)
                                           ? [buffer copy]
                                           : processed.resampled_buffer;

    __weak TTCAudioRecorder* weakSelf = self;
    web::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(^{
          [weakSelf deliverCapturedEnergy:processed.perceptual_energy
                                   buffer:capturedBuffer];
        }));
  }
}

// Dispatches the computed perceptual energy and resampled buffer to the
// delegate on the main thread.
- (void)deliverCapturedEnergy:(float)energy buffer:(AVAudioPCMBuffer*)buffer {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if ([self.delegate
          respondsToSelector:@selector(audioRecorder:didUpdateInputEnergy:)]) {
    [self.delegate audioRecorder:self didUpdateInputEnergy:energy];
  }
  if (buffer &&
      [self.delegate
          respondsToSelector:@selector(audioRecorder:didCaptureBuffer:)]) {
    [self.delegate audioRecorder:self didCaptureBuffer:buffer];
  }
}

@end
