// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/apple/audio_input.h"

#include <CoreServices/CoreServices.h>

#include <memory>

#include "base/apple/osstatus_logging.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"

#if BUILDFLAG(IS_MAC)
#include "media/audio/mac/audio_manager_mac.h"
#else
#include "media/audio/ios/audio_manager_ios.h"
#endif

namespace media {

namespace {
// A one-shot timer is created and started in Start() and it triggers
// CheckInputStartupSuccess() after this amount of time. UMA stats marked
// Media.Audio.InputStartupSuccessMacHighLatency is then updated where true is
// added if input callbacks have started, and false otherwise. This constant
// should ideally be set to about the same value as in
// audio_low_latency_input_mac.cc, to make comparing them reasonable.
const int kInputCallbackStartTimeoutInSeconds = 8;
}  // namespace

PCMQueueInAudioInputStream::PCMQueueInAudioInputStream(
    AudioManagerApple* manager,
    const AudioParameters& params)
    : manager_(manager),
      callback_(nullptr),
      audio_queue_(NULL),
      buffer_size_bytes_(0),
      started_(false),
      input_callback_is_active_(false),
      audio_bus_(media::AudioBus::Create(params)) {
  // We must have a manager.
  DCHECK(manager_);

  const SampleFormat kSampleFormat = kSampleFormatS16;

  // A frame is one sample across all channels. In interleaved audio the per
  // frame fields identify the set of n |channels|. In uncompressed audio, a
  // packet is always one frame.
  format_.mSampleRate = params.sample_rate();
  format_.mFormatID = kAudioFormatLinearPCM;
  format_.mFormatFlags =
      kLinearPCMFormatFlagIsPacked | kLinearPCMFormatFlagIsSignedInteger;
  format_.mBitsPerChannel = SampleFormatToBitsPerChannel(kSampleFormat);
  format_.mChannelsPerFrame = params.channels();
  format_.mFramesPerPacket = 1;
  format_.mBytesPerPacket = format_.mBytesPerFrame =
      params.GetBytesPerFrame(kSampleFormat);
  format_.mReserved = 0;

  buffer_size_bytes_ = params.GetBytesPerBuffer(kSampleFormat);
}

PCMQueueInAudioInputStream::~PCMQueueInAudioInputStream() {
  DCHECK(!callback_);
  DCHECK(!audio_queue_);
}

AudioInputStream::OpenOutcome PCMQueueInAudioInputStream::Open() {
  OSStatus err = AudioQueueNewInput(&format_,
                                    &HandleInputBufferStatic,
                                    this,
                                    NULL,  // Use OS CFRunLoop for |callback|
                                    kCFRunLoopCommonModes,
                                    0,  // Reserved
                                    &audio_queue_);
  if (err != noErr) {
    HandleError(err);
    return AudioInputStream::OpenOutcome::kFailed;
  }
  return SetupBuffers() ? AudioInputStream::OpenOutcome::kSuccess
                        : AudioInputStream::OpenOutcome::kFailed;
}

void PCMQueueInAudioInputStream::Start(AudioInputCallback* callback) {
  DCHECK(callback);
  DLOG_IF(ERROR, !audio_queue_) << "Open() has not been called successfully";
  if (callback_ || !audio_queue_)
    return;

#if BUILDFLAG(IS_MAC)
  // Check if we should defer Start() for http://crbug.com/160920.
  base::TimeDelta defer_start = manager_->GetDeferStreamStartTimeout();
  if (!defer_start.is_zero()) {
    // Use a cancellable closure so that if Stop() is called before Start()
    // actually runs, we can cancel the pending start.
    deferred_start_cb_.Reset(base::BindOnce(&PCMQueueInAudioInputStream::Start,
                                            base::Unretained(this), callback));
    manager_->GetTaskRunner()->PostDelayedTask(
        FROM_HERE, deferred_start_cb_.callback(), defer_start);
    return;
  }
#endif

  callback_ = callback;
  OSStatus err = AudioQueueStart(audio_queue_, NULL);
  if (err != noErr) {
    HandleError(err);
  } else {
    started_ = true;
  }

  // For UMA stat purposes, start a one-shot timer which detects when input
  // callbacks starts indicating if input audio recording starts as intended.
  // CheckInputStartupSuccess() will check if |input_callback_is_active_| is
  // true when the timer expires.
  input_callback_timer_ = std::make_unique<base::OneShotTimer>();
  input_callback_timer_->Start(
      FROM_HERE, base::Seconds(kInputCallbackStartTimeoutInSeconds), this,
      &PCMQueueInAudioInputStream::CheckInputStartupSuccess);
  DCHECK(input_callback_timer_->IsRunning());
}

void PCMQueueInAudioInputStream::Stop() {
  deferred_start_cb_.Cancel();
  if (input_callback_timer_ != nullptr) {
    input_callback_timer_->Stop();
    input_callback_timer_.reset();
  }
  if (!audio_queue_ || !started_)
    return;

  // We request a synchronous stop, so the next call can take some time. In
  // the windows implementation we block here as well.
  OSStatus err = AudioQueueStop(audio_queue_, true);
  if (err != noErr)
    HandleError(err);

  SetInputCallbackIsActive(false);
  started_ = false;
  callback_ = nullptr;
}

void PCMQueueInAudioInputStream::Close() {
  Stop();

  // It is valid to call Close() before calling Open() or Start(), thus
  // |audio_queue_| and |callback_| might be NULL.
  if (audio_queue_) {
    OSStatus err = AudioQueueDispose(audio_queue_, true);
    audio_queue_ = NULL;
    if (err != noErr)
      HandleError(err);
  }

  manager_->ReleaseInputStream(this);
  // CARE: This object may now be destroyed.
}

double PCMQueueInAudioInputStream::GetMaxVolume() {
  NOTIMPLEMENTED();
  return 1.0;
}

void PCMQueueInAudioInputStream::SetVolume(double volume) {
#if BUILDFLAG(IS_MAC)
  NOTIMPLEMENTED();
#else
  manager_->SetInputVolume(kAudioObjectUnknown, volume);
#endif
}

double PCMQueueInAudioInputStream::GetVolume() {
#if BUILDFLAG(IS_MAC)
  NOTIMPLEMENTED();
  return 1.0;
#else
  return manager_->GetInputVolume(kAudioObjectUnknown);
#endif
}

bool PCMQueueInAudioInputStream::IsMuted() {
#if BUILDFLAG(IS_MAC)
  NOTIMPLEMENTED();
  return false;
#else
  return manager_->IsInputMuted(kAudioObjectUnknown);
#endif
}

bool PCMQueueInAudioInputStream::SetAutomaticGainControl(bool enabled) {
  NOTIMPLEMENTED();
  return false;
}

bool PCMQueueInAudioInputStream::GetAutomaticGainControl() {
  NOTIMPLEMENTED();
  return false;
}

void PCMQueueInAudioInputStream::SetOutputDeviceForAec(
    const std::string& output_device_id) {
  // Not supported. Do nothing.
}

void PCMQueueInAudioInputStream::HandleError(OSStatus err) {
  if (callback_)
    callback_->OnError();
  // This point should never be reached.
  OSSTATUS_DCHECK(0, err);
}

bool PCMQueueInAudioInputStream::SetupBuffers() {
  DCHECK(buffer_size_bytes_);
  for (int i = 0; i < kNumberBuffers; ++i) {
    AudioQueueBufferRef buffer;
    OSStatus err = AudioQueueAllocateBuffer(audio_queue_,
                                            buffer_size_bytes_,
                                            &buffer);
    if (err == noErr)
      err = QueueNextBuffer(buffer);
    if (err != noErr) {
      HandleError(err);
      return false;
    }
    // |buffer| will automatically be freed when |audio_queue_| is released.
  }
  return true;
}

OSStatus PCMQueueInAudioInputStream::QueueNextBuffer(
    AudioQueueBufferRef audio_buffer) {
  // Only the first 2 params are needed for recording.
  return AudioQueueEnqueueBuffer(audio_queue_, audio_buffer, 0, NULL);
}

// static
void PCMQueueInAudioInputStream::HandleInputBufferStatic(
    void* data,
    AudioQueueRef audio_queue,
    AudioQueueBufferRef audio_buffer,
    const AudioTimeStamp* start_time,
    UInt32 num_packets,
    const AudioStreamPacketDescription* desc) {
  reinterpret_cast<PCMQueueInAudioInputStream*>(data)->
      HandleInputBuffer(audio_queue, audio_buffer, start_time,
                        num_packets, desc);
}

void PCMQueueInAudioInputStream::HandleInputBuffer(
    AudioQueueRef audio_queue,
    AudioQueueBufferRef audio_buffer,
    const AudioTimeStamp* start_time,
    UInt32 num_packets,
    const AudioStreamPacketDescription* packet_desc) {
  DCHECK_EQ(audio_queue_, audio_queue);
  DCHECK(audio_buffer->mAudioData);
  TRACE_EVENT0("audio", "PCMQueueInAudioInputStream::HandleInputBuffer");
  if (!callback_) {
    // This can happen if Stop() was called without start.
    DCHECK_EQ(0U, audio_buffer->mAudioDataByteSize);
    return;
  }

  // Indicate that input callbacks have started.
  SetInputCallbackIsActive(true);

  if (audio_buffer->mAudioDataByteSize) {
    // The AudioQueue API may use a large internal buffer and repeatedly call us
    // back to back once that internal buffer is filled.  When this happens the
    // renderer client does not have enough time to read data back from the
    // shared memory before the next write comes along.  If HandleInputBuffer()
    // is called too frequently, Sleep() at least 5ms to ensure the shared
    // memory doesn't get trampled.
    // TODO(dalecurtis): This is a HACK.  Long term the AudioQueue path is going
    // away in favor of the AudioUnit based AUAudioInputStream().  Tracked by
    // http://crbug.com/161383.
    // TODO(dalecurtis): Delete all this. It shouldn't be necessary now that we
    // have a ring buffer and FIFO on the actual shared memory.
    base::TimeDelta elapsed = base::TimeTicks::Now() - last_fill_;
    const base::TimeDelta kMinDelay = base::Milliseconds(5);
    if (elapsed < kMinDelay) {
      TRACE_EVENT0("audio",
                   "PCMQueueInAudioInputStream::HandleInputBuffer sleep");
      base::PlatformThread::Sleep(kMinDelay - elapsed);
    }

    // TODO(dalecurtis): This should be updated to include the device latency,
    // but really since Pepper (which ignores the delay value) is on the only
    // one creating AUDIO_PCM_LINEAR input devices, it doesn't matter.
    // https://lists.apple.com/archives/coreaudio-api/2017/Jul/msg00035.html
    const base::TimeTicks capture_time =
        start_time->mFlags & kAudioTimeStampHostTimeValid
            ? base::TimeTicks::FromMachAbsoluteTime(start_time->mHostTime)
            : base::TimeTicks::Now();

    uint8_t* audio_data = reinterpret_cast<uint8_t*>(audio_buffer->mAudioData);
    DCHECK_EQ(format_.mBitsPerChannel, 16u);
    audio_bus_->FromInterleaved<SignedInt16SampleTypeTraits>(
        reinterpret_cast<int16_t*>(audio_data), audio_bus_->frames());
    callback_->OnData(audio_bus_.get(), capture_time, 0.0, {});

    last_fill_ = base::TimeTicks::Now();
  }
  // Recycle the buffer.
  OSStatus err = QueueNextBuffer(audio_buffer);
  if (err != noErr) {
    if (err == kAudioQueueErr_EnqueueDuringReset) {
      // This is the error you get if you try to enqueue a buffer and the
      // queue has been closed. Not really a problem if indeed the queue
      // has been closed.
      // TODO(joth): PCMQueueOutAudioOutputStream uses callback_ to provide an
      // extra guard for this situation, but it seems to introduce more
      // complications than it solves (memory barrier issues accessing it from
      // multiple threads, looses the means to indicate OnClosed to client).
      // Should determine if we need to do something equivalent here.
      return;
    }
    HandleError(err);
  }
}

void PCMQueueInAudioInputStream::SetInputCallbackIsActive(bool enabled) {
  base::subtle::Release_Store(&input_callback_is_active_, enabled);
}

bool PCMQueueInAudioInputStream::GetInputCallbackIsActive() {
  return (base::subtle::Acquire_Load(&input_callback_is_active_) != false);
}

void PCMQueueInAudioInputStream::CheckInputStartupSuccess() {
  // Check if we have called Start() and input callbacks have actually
  // started in time as they should. If that is not the case, we have a
  // problem and the stream is considered dead.
  const bool input_callback_is_active = GetInputCallbackIsActive();
  UMA_HISTOGRAM_BOOLEAN("Media.Audio.InputStartupSuccessMac_HighLatency",
                        input_callback_is_active);
  DVLOG(1) << "high_latency_input_callback_is_active: "
           << input_callback_is_active;
}

}  // namespace media
