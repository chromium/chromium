// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/ios/audio/audio_playback_sink_ios.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "remoting/client/audio/async_audio_data_supplier.h"
#include "remoting/client/audio/audio_stream_format.h"

namespace remoting {

namespace {

// Once we receive the stream format, we create
// |kRequiredBufferCountForPlayback| buffers from the audio queue, each with a
// duration of |kBufferLength|. The buffers will then be transferred to the
// supplier for priming. Once a buffer is filled up, we put it back to the audio
// queue and start running the queue. Buffer that has been consumed by the audio
// queue will be transferred back to the supplier for priming new audio data. We
// stop running the audio queue once all buffers have been transferred to the
// supplier.

constexpr base::TimeDelta kBufferLength = base::Milliseconds(10);
constexpr int kRequiredBufferCountForPlayback = 5;

class AudioQueueGetDataRequest : public AsyncAudioDataSupplier::GetDataRequest {
 public:
  AudioQueueGetDataRequest(
      AudioQueueBufferRef buffer,
      base::OnceCallback<void(AudioQueueBufferRef)> on_data_received);
  ~AudioQueueGetDataRequest() override;

  void OnDataFilled() override;

 private:
  AudioQueueBufferRef buffer_;
  base::OnceCallback<void(AudioQueueBufferRef)> on_data_received_;
};

AudioQueueGetDataRequest::AudioQueueGetDataRequest(
    AudioQueueBufferRef buffer,
    base::OnceCallback<void(AudioQueueBufferRef)> on_data_received)
    : GetDataRequest(buffer->mAudioData, buffer->mAudioDataBytesCapacity),
      buffer_(buffer),
      on_data_received_(std::move(on_data_received)) {
  buffer_->mAudioDataByteSize = buffer_->mAudioDataBytesCapacity;
}

// Note that disposing of the output queue also disposes all of its buffers,
// so no cleanup is needed here.
AudioQueueGetDataRequest::~AudioQueueGetDataRequest() = default;

void AudioQueueGetDataRequest::OnDataFilled() {
  std::move(on_data_received_).Run(buffer_);
}

}  // namespace

AudioPlaybackSinkIos::AudioPlaybackSinkIos() : weak_factory_(this) {
  DETACH_FROM_THREAD(thread_checker_);
}

AudioPlaybackSinkIos::~AudioPlaybackSinkIos() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DisposeOutputQueue();
}

void AudioPlaybackSinkIos::SetDataSupplier(AsyncAudioDataSupplier* supplier) {
  DCHECK(supplier);
  supplier_ = supplier;
}

void AudioPlaybackSinkIos::ResetStreamFormat(const AudioStreamFormat& format) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  stream_format_.mSampleRate = format.sample_rate;
  stream_format_.mFormatID = kAudioFormatLinearPCM;
  stream_format_.mFormatFlags =
      kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
  stream_format_.mBitsPerChannel = 8 * format.bytes_per_sample;
  stream_format_.mChannelsPerFrame = format.channels;
  stream_format_.mBytesPerPacket = format.bytes_per_sample * format.channels;
  stream_format_.mBytesPerFrame = stream_format_.mBytesPerPacket;
  stream_format_.mFramesPerPacket = 1;
  stream_format_.mReserved = 0;

  ResetOutputQueue();
}

void AudioPlaybackSinkIos::AsyncGetAudioData(AudioQueueBufferRef buffer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(supplier_);

  priming_buffers_count_++;

  supplier_->AsyncGetData(std::make_unique<AudioQueueGetDataRequest>(
      buffer, base::BindOnce(&AudioPlaybackSinkIos::OnAudioDataReceived,
                             weak_factory_.GetWeakPtr())));

  if (state_ == State::RUNNING &&
      priming_buffers_count_ == kRequiredBufferCountForPlayback) {
    // Buffer underrun. Stop playback immediately.
    StopPlayback();
    return;
  }
}

void AudioPlaybackSinkIos::OnAudioDataReceived(AudioQueueBufferRef buffer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  AudioQueueEnqueueBuffer(output_queue_, buffer, 0, nullptr);
  priming_buffers_count_--;

  if (state_ == State::STOPPED) {
    StartPlayback();
  }
}

// static
void AudioPlaybackSinkIos::OnBufferDequeued(void* context,
                                            AudioQueueRef outAQ,
                                            AudioQueueBufferRef buffer) {
  AudioPlaybackSinkIos* instance =
      reinterpret_cast<AudioPlaybackSinkIos*>(context);
  DCHECK(instance);
  instance->AsyncGetAudioData(buffer);
}

void AudioPlaybackSinkIos::StartPlayback() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (state_ != State::STOPPED) {
    return;
  }
  DCHECK(output_queue_);
  OSStatus err = AudioQueueStart(output_queue_, nullptr);

  if (err) {
    // This could be a transient failure when we try to start playback while the
    // app is resuming from the background. We can reset the queue for now and
    // wait for new audio data to trigger StartPlayback() again.
    LOG(ERROR) << "AudioQueueStart failed: " << err;

    // StartPlayback() may be called from inside GetDataRequest::OnDataFilled().
    // In this case ResetOutputQueue() must be called in a separate task because
    // it alters the pending requests in |supplier_|.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&AudioPlaybackSinkIos::ResetOutputQueue,
                                  weak_factory_.GetWeakPtr()));
    state_ = State::SCHEDULED_TO_RESET;
  } else {
    state_ = State::RUNNING;
  }
}

void AudioPlaybackSinkIos::StopPlayback() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(output_queue_);

  if (state_ != State::RUNNING) {
    return;
  }

  // Note that AudioQueueStop() will immediately return all enqueued buffers to
  // us, which calls AsyncGetAudioData(). We change the state to STOPPED before
  // AudioQueueStop() so that the buffers are immediately transferred to the
  // supplier.
  state_ = State::STOPPED;

  OSStatus err = AudioQueueStop(output_queue_, /* Immediate */ true);
  HandleError(err, "AudioQueueStop");
}

void AudioPlaybackSinkIos::ResetOutputQueue() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DisposeOutputQueue();

  {
    OSStatus err = AudioQueueNewOutput(
        &stream_format_, OnBufferDequeued, this, CFRunLoopGetCurrent(),
        kCFRunLoopCommonModes, 0, &output_queue_);
    if (HandleError(err, "AudioQueueNewOutput")) {
      return;
    }
  }

  // Create buffers.
  size_t buffer_byte_size = stream_format_.mSampleRate *
                            stream_format_.mBytesPerPacket *
                            kBufferLength.InSecondsF();
  for (int i = 0; i < kRequiredBufferCountForPlayback; i++) {
    AudioQueueBufferRef buffer;
    OSStatus err =
        AudioQueueAllocateBuffer(output_queue_, buffer_byte_size, &buffer);
    if (HandleError(err, "AudioQueueAllocateBuffer")) {
      return;
    }

    // Immediately request for audio data.
    AsyncGetAudioData(buffer);
  }
}

void AudioPlaybackSinkIos::DisposeOutputQueue() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(supplier_);
  if (!output_queue_) {
    return;
  }

  AudioQueueDispose(output_queue_, /* Immediate */ true);
  supplier_->ClearGetDataRequests();
  priming_buffers_count_ = 0;
  output_queue_ = nullptr;

  state_ = State::STOPPED;
}

bool AudioPlaybackSinkIos::HandleError(OSStatus err,
                                       const char* function_name) {
  if (err) {
    LOG(DFATAL) << "Failed to call " << function_name
                << ", error code: " << err;
    DisposeOutputQueue();
    return true;
  }
  return false;
}

}  // namespace remoting
