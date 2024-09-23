// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/audio/mac/audio_loopback_input_mac_impl.h"

#import <ScreenCaptureKit/ScreenCaptureKit.h>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "media/audio/mac/audio_loopback_input_mac.h"
#include "media/base/audio_timestamp_helper.h"

using SampleCallback = base::RepeatingCallback<
    void(base::apple::ScopedCFTypeRef<CMSampleBufferRef>, const double)>;
using ErrorCallback = base::RepeatingCallback<void()>;

namespace media {

constexpr float kMaxVolume = 1.0;

// Used for synchronized data access between SCKAudioInputStream and
// ScreenCaptureKitAudioHelper. In case callbacks from ScreenCaptureKit are
// invoked after the client no longer wants to receive data, or
// SCKAudioInputStream has already been destroyed, this reference counted class
// outlives both objects and helps prevent use-after-free situations.
class API_AVAILABLE(macos(13.0)) SharedHelper
    : public base::RefCountedThreadSafe<SharedHelper> {
 public:
  SharedHelper(const base::TimeDelta shareable_content_timeout)
      : volume_(kMaxVolume),
        shareable_content_timeout_(shareable_content_timeout) {}
  SharedHelper(SharedHelper&) = delete;
  SharedHelper(SharedHelper&&) = delete;
  SharedHelper& operator=(SharedHelper&) = delete;
  SharedHelper& operator=(SharedHelper&&) = delete;

  // Set the sample and error callback to be invoked by the helper.
  void SetStreamCallbacks(SampleCallback sample_callback,
                          ErrorCallback error_callback) {
    base::AutoLock auto_lock(lock_);
    sample_callback_ = std::move(sample_callback);
    error_callback_ = std::move(error_callback);
  }

  // Reset the sample and error callbacks, resulting in no more samples being
  // delivered.
  void ResetStreamCallbacks() {
    base::AutoLock auto_lock(lock_);
    sample_callback_.Reset();
    error_callback_.Reset();
  }

  void SetVolume(float volume) {
    base::AutoLock auto_lock(lock_);
    volume_ = volume;
  }

  float GetVolume() {
    base::AutoLock auto_lock(lock_);
    return volume_;
  }

  // Invokes the sample callback, if one is set.
  void OnStreamSample(
      base::apple::ScopedCFTypeRef<CMSampleBufferRef> sample_buffer) {
    base::AutoTryLock auto_try_lock(lock_);
    if (auto_try_lock.is_acquired() && !sample_callback_.is_null()) {
      // |volume_| is passed in as a parameter, as it is guarded by |lock_|,
      // which also protects the callbacks and is already acquired.
      sample_callback_.Run(sample_buffer, volume_);
    }
  }

  // Logs the error and reports it via the error callback, if one is set.
  void OnStreamError(NSError* error) {
    if (!error) {
      return;
    }

    LOG(ERROR) << "Stream error: "
               << base::SysNSStringToUTF8([error localizedDescription]);

    base::UmaHistogramSparse("Media.Audio.Capture.SCK.StreamError",
                             [error code]);

    base::AutoTryLock auto_try_lock(lock_);
    if (auto_try_lock.is_acquired() && !error_callback_.is_null()) {
      error_callback_.Run();
    }
  }

  // Synchronously retrieves a stream filter.
  AudioInputStream::OpenOutcome GetContentFilter(SCContentFilter** filter) {
    // We want to avoid any deadlocks due to issues with ScreenCaptureKit. Thus,
    // we use a WaitableEvent with a timeout.
    const auto data = base::MakeRefCounted<ShareableContentData>();
    [SCShareableContent getShareableContentWithCompletionHandler:^(
                            SCShareableContent* content, NSError* error) {
      // |data| is captured by value, so the reference count increases.
      OnShareableContentCreated(data, content, error);
      data->event.Signal();
    }];

    base::TimeTicks enumeration_start = base::TimeTicks::Now();
    // Wait for shareable content to be enumerated.
    const bool timed_out = !data->event.TimedWait(shareable_content_timeout_);
    base::TimeDelta enumeration_time =
        base::TimeTicks::Now() - enumeration_start;

    base::UmaHistogramBoolean(
        "Media.Audio.Capture.SCK.ContentEnumerationTimedOut", timed_out);

    if (timed_out) {
      LOG(ERROR) << "Shareable content enumeration timed out.";
      return AudioInputStream::OpenOutcome::kFailed;
    } else {
      base::UmaHistogramTimes(
          "Media.Audio.Capture.SCK.ContentEnumerationTimeMs", enumeration_time);
    }

    if (data->open_outcome == AudioInputStream::OpenOutcome::kSuccess) {
      *filter = data->filter;
    }

    return data->open_outcome;
  }

 private:
  friend class base::RefCountedThreadSafe<SharedHelper>;

  class ShareableContentData
      : public base::RefCountedThreadSafe<ShareableContentData> {
   public:
    // Event used to signal completion of shareable content enumeration.
    base::WaitableEvent event;

    // Outcome of shareable content enumeration.
    AudioInputStream::OpenOutcome open_outcome =
        AudioInputStream::OpenOutcome::kFailed;

    // Filter to be generated based on the enumerated shareable content.
    SCContentFilter* filter{nil};

   private:
    friend class base::RefCountedThreadSafe<ShareableContentData>;
    ~ShareableContentData() = default;
  };

  ~SharedHelper() = default;

  // Invoked when shareable content (displays, applications, windows) has been
  // enumerated. Generates a filter based on the available content. Runs on a
  // SCK thread.
  static void OnShareableContentCreated(
      scoped_refptr<ShareableContentData> data,
      SCShareableContent* content,
      NSError* error) {
    CHECK(data);

    data->open_outcome = AudioInputStream::OpenOutcome::kFailed;
    if (error) {
      if ([error code] == SCStreamErrorUserDeclined) {
        data->open_outcome =
            AudioInputStream::OpenOutcome::kFailedSystemPermissions;
      }
      return;
    }

    CHECK(content);

    if ([[content displays] count] == 0) {
      VLOG(1) << "No displays found.";
      return;
    }

    // Capturing any display will capture the entire system's audio.
    SCDisplay* display = [[content displays] firstObject];

    VLOG(1) << "Capturing display with ID " << [display displayID]
            << " and resolution " << [display width] << "x" << [display height];

    data->filter = [[SCContentFilter alloc] initWithDisplay:display
                                           excludingWindows:@[]];

    data->open_outcome = AudioInputStream::OpenOutcome::kSuccess;
  }

  // Lock must be used when accessing the callbacks.
  base::Lock lock_;

  // Callbacks registered by SCKAudioInputStream and invoked by
  // ScreenCaptureKitAudioHelper.
  SampleCallback sample_callback_ GUARDED_BY(lock_);
  ErrorCallback error_callback_ GUARDED_BY(lock_);

  // Current volume.
  double volume_ GUARDED_BY(lock_);

  // Timeout for shareable content enumeration.
  const base::TimeDelta shareable_content_timeout_;
};

}  // namespace media

API_AVAILABLE(macos(13.0))
@interface ScreenCaptureKitAudioHelper
    : NSObject <SCStreamDelegate, SCStreamOutput> {
  scoped_refptr<media::SharedHelper> _sharedHelper;
}

- (instancetype)initWithSharedHelper:
    (scoped_refptr<media::SharedHelper>)sharedHelper;
- (void)stream:(SCStream*)stream didStopWithError:(NSError*)error;
- (void)stream:(SCStream*)stream
    didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
                   ofType:(SCStreamOutputType)type;

@end

@implementation ScreenCaptureKitAudioHelper

- (instancetype)initWithSharedHelper:
    (scoped_refptr<media::SharedHelper>)sharedHelper {
  self = [super init];
  if (self) {
    _sharedHelper = sharedHelper;
  }

  return self;
}

- (void)stream:(SCStream*)stream didStopWithError:(NSError*)error {
  _sharedHelper->OnStreamError(error);
}

- (void)stream:(SCStream*)stream
    didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
                   ofType:(SCStreamOutputType)type {
  if (type != SCStreamOutputTypeAudio || !sampleBuffer) {
    return;
  }

  _sharedHelper->OnStreamSample(base::apple::ScopedCFTypeRef<CMSampleBufferRef>(
      sampleBuffer, base::scoped_policy::RETAIN));
}

@end

namespace media {

SCKAudioInputStream::SCKAudioInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const AudioManager::LogCallback log_callback,
    const NotifyOnCloseCallback close_callback)
    : SCKAudioInputStream(params,
                          device_id,
                          std::move(log_callback),
                          std::move(close_callback),
                          StartSCStreamMockingCallback(),
                          base::Seconds(5)) {}

SCKAudioInputStream::SCKAudioInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const AudioManager::LogCallback log_callback,
    const NotifyOnCloseCallback close_callback,
    StartSCStreamMockingCallback start_scstream_mocking_callback,
    base::TimeDelta shareable_content_timeout)
    : params_(params),
      device_id_(device_id),
      audio_bus_(AudioBus::CreateWrapper(params.channels())),
      sink_(nullptr),
      log_callback_(std::move(log_callback)),
      close_callback_(std::move(close_callback)),
      start_scstream_mocking_callback_(start_scstream_mocking_callback),
      shared_helper_(
          base::MakeRefCounted<SharedHelper>(shareable_content_timeout)),
      sck_helper_([[ScreenCaptureKitAudioHelper alloc]
          initWithSharedHelper:shared_helper_]),
      buffer_frames_duration_(
          AudioTimestampHelper::FramesToTime(params_.frames_per_buffer(),
                                             params_.sample_rate())) {
  CHECK(AudioDeviceDescription::IsLoopbackDevice(device_id_));
  CHECK(!log_callback_.is_null());
  // TODO(crbug.com/40281254): Update getDisplayMedia to handle sample rate
  // constraints
  // ScreenCaptureKit supports only certain sample rates:
  // https://developer.apple.com/documentation/screencapturekit/scstreamconfiguration/3931903-samplerate
  CHECK(params_.sample_rate() == 48000 || params_.sample_rate() == 24000 ||
        params_.sample_rate() == 16000 || params_.sample_rate() == 8000);
  // Only mono and stereo audio is supported:
  // https://developer.apple.com/documentation/screencapturekit/scstreamconfiguration/3931901-channelcount
  CHECK(params_.channels() == 1 || params_.channels() == 2);

  // We need a wrapper because we set the channel data to point directly into
  // the sample buffer.
  audio_bus_->set_frames(params_.frames_per_buffer());
}

SCKAudioInputStream::~SCKAudioInputStream() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

AudioInputStream::OpenOutcome SCKAudioInputStream::Open() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (stream_) {
    return OpenOutcome::kAlreadyOpen;
  }

  SCContentFilter* filter = nil;
  OpenOutcome open_outcome = shared_helper_->GetContentFilter(&filter);

  if (open_outcome != OpenOutcome::kSuccess) {
    if (open_outcome == OpenOutcome::kFailedSystemPermissions) {
      SendLogMessage("%s => Missing screen capture permissions.", __func__);
    } else {
      SendLogMessage("%s => Failed to retrieve shareable content.", __func__);
    }

    return open_outcome;
  }

  // All settings related to video capture must remain at their default values,
  // otherwise a video sample stream output must also be added.
  SCStreamConfiguration* config = [[SCStreamConfiguration alloc] init];
  [config setCapturesAudio:YES];
  [config setSampleRate:params_.sample_rate()];
  [config setChannelCount:params_.channels()];
  if (device_id_ == AudioDeviceDescription::kLoopbackWithoutChromeId) {
    // Excludes audio from all browser subprocesses outputting audio through
    // the audio service.
    [config setExcludesCurrentProcessAudio:YES];
  }

  stream_ = [[SCStream alloc] initWithFilter:filter
                               configuration:config
                                    delegate:sck_helper_];

  // Notifies the test so that it can start mocking the new SCStream object.
  if (!start_scstream_mocking_callback_.is_null()) {
    start_scstream_mocking_callback_.Run(stream_, filter, config, sck_helper_);
  }

  // |queue_| is used internally by the API to store |config.queueDepth| number
  // of frames (default is 3).
  queue_ = dispatch_queue_create("org.chromium.SCKAudioInputStream",
                                 DISPATCH_QUEUE_SERIAL);

  NSError* add_stream_output_error = nil;
  const bool stream_output_added =
      [stream_ addStreamOutput:sck_helper_
                          type:SCStreamOutputTypeAudio
            sampleHandlerQueue:queue_
                         error:&add_stream_output_error];
  if (!stream_output_added) {
    SendLogMessage(
        "%s => Failed to add stream output: %s", __func__,
        base::SysNSStringToUTF8([add_stream_output_error localizedDescription])
            .c_str());
    stream_ = nil;
    queue_ = nil;
    return OpenOutcome::kFailed;
  }

  return OpenOutcome::kSuccess;
}

void SCKAudioInputStream::Start(AudioInputCallback* callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(callback);

  // Don't do anything if the stream isn't open and ignore any consecutive
  // Start() calls.
  if (!stream_ || sink_) {
    return;
  }

  sink_ = callback;

  // Sample and error callbacks are set and reset by SCKAudioInputStream when
  // starting and stopping the stream, respectively. Thus, |this| will always be
  // valid if the callback is not null.
  shared_helper_->SetStreamCallbacks(
      base::BindRepeating(&SCKAudioInputStream::OnStreamSample,
                          base::Unretained(this)),
      base::BindRepeating(&SCKAudioInputStream::OnStreamError,
                          base::Unretained(this)));

  // Make a local copy of the shared_refptr in case the error handler is called
  // after `this` is destroyed.
  auto local_shared_helper = shared_helper_;
  [stream_ startCaptureWithCompletionHandler:^(NSError* error) {
    if (!error) {
      return;
    }

    local_shared_helper->OnStreamError(error);
  }];
}

void SCKAudioInputStream::Close() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!sink_);

  // It is valid to call Close() before calling Open().
  if (stream_) {
    NSError* error = nil;
    bool stream_output_removed =
        [stream_ removeStreamOutput:sck_helper_
                               type:SCStreamOutputTypeAudio
                              error:&error];
    if (!stream_output_removed) {
      SendLogMessage(
          "%s => Failed to remove stream output: %s", __func__,
          base::SysNSStringToUTF8([error localizedDescription]).c_str());
    }
  }

  // Notify the owner that the stream can be deleted.
  close_callback_.Run(this);
}

void SCKAudioInputStream::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Reset the callbacks since the client is asking to stop the stream and
  // does not want to receive any more samples and errors. This must be done
  // before setting |sink_| to null, to avoid a race.
  shared_helper_->ResetStreamCallbacks();

  if (!sink_) {
    return;
  }

  // An attempt to stop an already stopped stream (due to an error) will
  // simply result in an error, which can be ignored.
  [stream_ stopCaptureWithCompletionHandler:^(NSError* error) {
    if (!error) {
      return;
    }

    LOG(ERROR) << "Error while stopping the stream: "
               << base::SysNSStringToUTF8([error localizedDescription]);
  }];

  sink_ = nullptr;
}

double SCKAudioInputStream::GetMaxVolume() {
  return kMaxVolume;
}

void SCKAudioInputStream::SetVolume(double volume) {
  CHECK_GE(volume, 0.0);
  CHECK_LE(volume, kMaxVolume);
  shared_helper_->SetVolume(volume);
}

double SCKAudioInputStream::GetVolume() {
  return shared_helper_->GetVolume();
}

bool SCKAudioInputStream::IsMuted() {
  return GetVolume() == 0.0;
}

void SCKAudioInputStream::SetOutputDeviceForAec(
    const std::string& output_device_id) {
  return;
}

void SCKAudioInputStream::OnStreamSample(
    base::apple::ScopedCFTypeRef<CMSampleBufferRef> sample_buffer,
    const double volume) {
  const CMBlockBufferRef block_buffer =
      CMSampleBufferGetDataBuffer(sample_buffer.get());
  if (!block_buffer) {
    VLOG(1) << "Sample buffer is empty.";
    return;
  }

  char* buffer = nullptr;
  if (CMBlockBufferGetDataPointer(block_buffer, 0, nullptr, nullptr, &buffer) !=
      kCMBlockBufferNoErr) {
    VLOG(1) << "Cannot access block buffer data.";
    return;
  }

  const CMTime time_stamp =
      CMSampleBufferGetPresentationTimeStamp(sample_buffer.get());
  const CMFormatDescriptionRef format_description =
      CMSampleBufferGetFormatDescription(sample_buffer.get());
  const AudioStreamBasicDescription* audio_description =
      CMAudioFormatDescriptionGetStreamBasicDescription(format_description);

  CHECK(static_cast<int>(audio_description->mChannelsPerFrame) ==
            params_.channels() &&
        audio_description->mSampleRate == params_.sample_rate());
  CHECK_EQ(audio_description->mBytesPerFrame, sizeof(float))
      << "Expected non-interleaved data.";

  const size_t total_frame_count =
      CMSampleBufferGetNumSamples(sample_buffer.get());

  base::TimeTicks capture_time;
  capture_time += base::Seconds(CMTimeGetSeconds(time_stamp));

  // |sample_buffer| can deliver more frames than specified in |params_|, but
  // the amount should always be a multiple of the one specified.
  CHECK(total_frame_count % params_.frames_per_buffer() == 0);

  for (size_t frames_delivered = 0; frames_delivered < total_frame_count;
       frames_delivered += params_.frames_per_buffer()) {
    // Data in |buffer| is non-interleaved and immutable. Since we don't copy
    // the audio data, we must retain a reference to |sample_buffer| until
    // |audio_bus_| is no longer used.
    for (int channel = 0; channel < params_.channels(); channel++) {
      float* channel_data = reinterpret_cast<float*>(buffer) +
                            channel * total_frame_count + frames_delivered;
      audio_bus_->SetChannelData(channel, channel_data);
    }

    // Adjust the volume.
    audio_bus_->Scale(volume);

    // OnStreamSample() is only called if |shared_helper_| has callbacks set.
    sink_->OnData(audio_bus_.get(), capture_time, volume, {});

    capture_time += buffer_frames_duration_;
  }
}

void SCKAudioInputStream::OnStreamError() {
  CHECK(sink_);
  // |sink_| is safe to access, as OnStreamError() is called from
  // |shared_helper_| with the lock acquired.
  sink_->OnError();
}

void SCKAudioInputStream::SendLogMessage(const char* format, ...) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  va_list args;
  va_start(args, format);
  log_callback_.Run("SCKAudioInputStream::" + base::StringPrintV(format, args));
  va_end(args);
}

AudioInputStream* CreateSCKAudioInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    AudioManager::LogCallback log_callback,
    const base::RepeatingCallback<void(AudioInputStream*)> close_callback) {
  if (@available(macOS 13.0, *)) {
    return new SCKAudioInputStream(params, device_id, std::move(log_callback),
                                   std::move(close_callback));
  }

  return nullptr;
}

}  // namespace media
