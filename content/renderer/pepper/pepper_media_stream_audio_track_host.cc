// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/renderer/pepper/pepper_media_stream_audio_track_host.h"

#include <algorithm>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/numerics/ostream_operators.h"
#include "base/numerics/safe_math.h"
#include "base/task/single_thread_task_runner.h"
#include "media/base/audio_bus.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_audio_buffer.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/media_stream_audio_track_shared.h"
#include "ppapi/shared_impl/media_stream_buffer.h"
#include "ppapi/shared_impl/ppb_audio_config_shared.h"

using media::AudioParameters;
using ppapi::host::HostMessageContext;
using ppapi::MediaStreamAudioTrackShared;

namespace {

// Audio buffer durations in milliseconds.
const uint32_t kMinDuration = 10;
const uint32_t kDefaultDuration = 10;

const int32_t kDefaultNumberOfAudioBuffers = 4;
const int32_t kMaxNumberOfAudioBuffers = 1000;  // 10 sec

// Returns true if the |sample_rate| is supported in
// |PP_AudioBuffer_SampleRate|, otherwise false.
PP_AudioBuffer_SampleRate GetPPSampleRate(int sample_rate) {
  switch (sample_rate) {
    case 8000:
      return PP_AUDIOBUFFER_SAMPLERATE_8000;
    case 16000:
      return PP_AUDIOBUFFER_SAMPLERATE_16000;
    case 22050:
      return PP_AUDIOBUFFER_SAMPLERATE_22050;
    case 32000:
      return PP_AUDIOBUFFER_SAMPLERATE_32000;
    case 44100:
      return PP_AUDIOBUFFER_SAMPLERATE_44100;
    case 48000:
      return PP_AUDIOBUFFER_SAMPLERATE_48000;
    case 96000:
      return PP_AUDIOBUFFER_SAMPLERATE_96000;
    case 192000:
      return PP_AUDIOBUFFER_SAMPLERATE_192000;
    default:
      return PP_AUDIOBUFFER_SAMPLERATE_UNKNOWN;
  }
}

}  // namespace

namespace content {

PepperMediaStreamAudioTrackHost::AudioSink::AudioSink(
    PepperMediaStreamAudioTrackHost* host)
    : host_(host),
      active_buffer_index_(-1),
      active_buffers_generation_(0),
      active_buffer_frame_offset_(0),
      buffers_generation_(0),
      main_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      number_of_buffers_(kDefaultNumberOfAudioBuffers),
      bytes_per_second_(0),
      bytes_per_frame_(0),
      user_buffer_duration_(kDefaultDuration) {}

PepperMediaStreamAudioTrackHost::AudioSink::~AudioSink() {
  DCHECK_EQ(main_task_runner_,
            base::SingleThreadTaskRunner::GetCurrentDefault());
}

void PepperMediaStreamAudioTrackHost::AudioSink::EnqueueBuffer(int32_t index) {
  DCHECK_EQ(main_task_runner_,
            base::SingleThreadTaskRunner::GetCurrentDefault());
  DCHECK_GE(index, 0);
  DCHECK_LT(index, host_->buffer_manager()->number_of_buffers());
  base::AutoLock lock(lock_);
  buffers_.push_back(index);
}

int32_t PepperMediaStreamAudioTrackHost::AudioSink::Configure(
    int32_t number_of_buffers, int32_t duration,
    const ppapi::host::ReplyMessageContext& context) {
  DCHECK_EQ(main_task_runner_,
            base::SingleThreadTaskRunner::GetCurrentDefault());

  if (pending_configure_reply_.is_valid()) {
    return PP_ERROR_INPROGRESS;
  }
  pending_configure_reply_ = context;

  bool changed = false;
  if (number_of_buffers != number_of_buffers_)
    changed = true;
  if (duration != 0 && duration != user_buffer_duration_) {
    user_buffer_duration_ = duration;
    changed = true;
  }
  number_of_buffers_ = number_of_buffers;

  if (changed) {
    // Initialize later in OnSetFormat if bytes_per_second_ is not known yet.
    if (bytes_per_second_ > 0 && bytes_per_frame_ > 0)
      InitBuffers();
  } else {
    SendConfigureReply(PP_OK);
  }
  return PP_OK_COMPLETIONPENDING;
}

void PepperMediaStreamAudioTrackHost::AudioSink::SendConfigureReply(
    int32_t result) {
  if (pending_configure_reply_.is_valid()) {
    pending_configure_reply_.params.set_result(result);
    host_->host()->SendReply(
        pending_configure_reply_,
        PpapiPluginMsg_MediaStreamAudioTrack_ConfigureReply());
    pending_configure_reply_ = ppapi::host::ReplyMessageContext();
  }
}

void PepperMediaStreamAudioTrackHost::AudioSink::SetFormatOnMainThread(
    int bytes_per_second, int bytes_per_frame) {
  bytes_per_second_ = bytes_per_second;
  bytes_per_frame_ = bytes_per_frame;
  InitBuffers();
}

void PepperMediaStreamAudioTrackHost::AudioSink::InitBuffers() {
  DCHECK_EQ(main_task_runner_,
            base::SingleThreadTaskRunner::GetCurrentDefault());
  {
    base::AutoLock lock(lock_);
    // Clear |buffers_|, so the audio thread will drop all incoming audio data.
    buffers_.clear();
    buffers_generation_++;
  }
  int32_t frame_rate = bytes_per_second_ / bytes_per_frame_;
  base::CheckedNumeric<int32_t> frames_per_buffer = user_buffer_duration_;
  frames_per_buffer *= frame_rate;
  frames_per_buffer /= base::Time::kMillisecondsPerSecond;
  base::CheckedNumeric<int32_t> buffer_audio_size =
      frames_per_buffer * bytes_per_frame_;
  // The size is slightly bigger than necessary, because 8 extra bytes are
  // added into the struct. Also see |MediaStreamBuffer|. Also, the size of the
  // buffer may be larger than requested, since the size of each buffer will be
  // 4-byte aligned.
  base::CheckedNumeric<int32_t> buffer_size = buffer_audio_size;
  buffer_size += sizeof(ppapi::MediaStreamBuffer::Audio);
  DCHECK_GT(buffer_size.ValueOrDie(), 0);

  // We don't need to hold |lock_| during |host->InitBuffers()| call, because
  // we just cleared |buffers_| , so the audio thread will drop all incoming
  // audio data, and not use buffers in |host_|.
  bool result = host_->InitBuffers(number_of_buffers_,
                                   buffer_size.ValueOrDie(),
                                   kRead);
  if (!result) {
    SendConfigureReply(PP_ERROR_NOMEMORY);
    return;
  }

  // Fill the |buffers_|, so the audio thread can continue receiving audio data.
  base::AutoLock lock(lock_);
  output_buffer_size_ = buffer_audio_size.ValueOrDie();
  for (int32_t i = 0; i < number_of_buffers_; ++i) {
    int32_t index = host_->buffer_manager()->DequeueBuffer();
    DCHECK_GE(index, 0);
    buffers_.push_back(index);
  }

  SendConfigureReply(PP_OK);
}

void PepperMediaStreamAudioTrackHost::AudioSink::
    SendEnqueueBufferMessageOnMainThread(int32_t index,
                                         int32_t buffers_generation) {
  DCHECK_EQ(main_task_runner_,
            base::SingleThreadTaskRunner::GetCurrentDefault());
  // If |InitBuffers()| is called after this task being posted from the audio
  // thread, the buffer should become invalid already. We should ignore it.
  // And because only the main thread modifies the |buffers_generation_|,
  // so we don't need to lock |lock_| here (main thread).
  if (buffers_generation == buffers_generation_)
    host_->SendEnqueueBufferMessageToPlugin(index);
}

void PepperMediaStreamAudioTrackHost::AudioSink::OnData(
    const media::AudioBus& audio_bus,
    base::TimeTicks estimated_capture_time) {
  DCHECK(audio_thread_checker_.CalledOnValidThread());
  DCHECK(audio_params_.IsValid());
  DCHECK_EQ(audio_bus.channels(), audio_params_.channels());
  // Here, |audio_params_.frames_per_buffer()| refers to the incomming audio
  // buffer. However, this doesn't necessarily equal
  // |buffer->number_of_samples|, which is configured by the user when they set
  // buffer duration.
  DCHECK_EQ(audio_bus.frames(), audio_params_.frames_per_buffer());
  DCHECK(!estimated_capture_time.is_null());

  if (first_frame_capture_time_.is_null())
    first_frame_capture_time_ = estimated_capture_time;

  base::AutoLock lock(lock_);
  for (int frame_offset = 0; frame_offset < audio_bus.frames(); ) {
    if (active_buffers_generation_ != buffers_generation_) {
      // Buffers have changed, so drop the active buffer.
      active_buffer_index_ = -1;
    }
    if (active_buffer_index_ == -1 && !buffers_.empty()) {
      active_buffers_generation_ = buffers_generation_;
      active_buffer_frame_offset_ = 0;
      active_buffer_index_ = buffers_.front();
      buffers_.pop_front();
    }
    if (active_buffer_index_ == -1) {
      // Eek! We're dropping frames. Bad, bad, bad!
      break;
    }

    // TODO(penghuang): Support re-sampling and channel mixing by using
    // media::AudioConverter.
    ppapi::MediaStreamBuffer::Audio* buffer =
        &(host_->buffer_manager()->GetBufferPointer(active_buffer_index_)
          ->audio);
    if (active_buffer_frame_offset_ == 0) {
      // The active buffer is new, so initialise the header and metadata fields.
      buffer->header.size = host_->buffer_manager()->buffer_size();
      buffer->header.type = ppapi::MediaStreamBuffer::TYPE_AUDIO;
      const base::TimeTicks time_at_offset =
          estimated_capture_time +
          frame_offset * base::Seconds(1) / audio_params_.sample_rate();
      buffer->timestamp =
          (time_at_offset - first_frame_capture_time_).InSecondsF();
      buffer->sample_rate =
          static_cast<PP_AudioBuffer_SampleRate>(audio_params_.sample_rate());
      buffer->data_size = output_buffer_size_;
      buffer->number_of_channels = audio_params_.channels();
      buffer->number_of_samples =
          buffer->data_size * audio_params_.channels() / bytes_per_frame_;
    }

    const int frames_per_buffer =
        buffer->number_of_samples / audio_params_.channels();
    const int frames_to_copy =
        std::min(frames_per_buffer - active_buffer_frame_offset_,
                 audio_bus.frames() - frame_offset);
    audio_bus.ToInterleavedPartial<media::SignedInt16SampleTypeTraits>(
        frame_offset, frames_to_copy,
        reinterpret_cast<int16_t*>(buffer->data + active_buffer_frame_offset_ *
                                                      bytes_per_frame_));
    active_buffer_frame_offset_ += frames_to_copy;
    frame_offset += frames_to_copy;

    DCHECK_LE(active_buffer_frame_offset_, frames_per_buffer);
    if (active_buffer_frame_offset_ == frames_per_buffer) {
      main_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&AudioSink::SendEnqueueBufferMessageOnMainThread,
                         weak_factory_.GetWeakPtr(), active_buffer_index_,
                         buffers_generation_));
      active_buffer_index_ = -1;
    }
  }
}

void PepperMediaStreamAudioTrackHost::AudioSink::OnSetFormat(
    const AudioParameters& params) {
  DCHECK(params.IsValid());
  // TODO(amistry): How do you handle the case where the user configures a
  // duration that's shorter than the received buffer duration? One option is to
  // double buffer, where the size of the intermediate ring buffer is at least
  // max(user requested duration, received buffer duration). There are other
  // ways of dealing with it, but which one is "correct"?
  DCHECK_LE(params.GetBufferDuration().InMilliseconds(), kMinDuration);
  DCHECK_NE(GetPPSampleRate(params.sample_rate()),
            PP_AUDIOBUFFER_SAMPLERATE_UNKNOWN);

  // TODO(penghuang): support setting format more than once.
  if (audio_params_.IsValid()) {
    DCHECK_EQ(params.sample_rate(), audio_params_.sample_rate());
    DCHECK_EQ(params.channels(), audio_params_.channels());
  } else {
    audio_thread_checker_.DetachFromThread();
    audio_params_ = params;

    static_assert(ppapi::kBitsPerAudioOutputSample == 16,
                  "Data must be pcm_s16le.");
    int bytes_per_frame = params.GetBytesPerFrame(media::kSampleFormatS16);
    int bytes_per_second = params.sample_rate() * bytes_per_frame;
    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&AudioSink::SetFormatOnMainThread,
                                  weak_factory_.GetWeakPtr(), bytes_per_second,
                                  bytes_per_frame));
  }
}

PepperMediaStreamAudioTrackHost::PepperMediaStreamAudioTrackHost(
    RendererPpapiHost* host,
    PP_Instance instance,
    PP_Resource resource,
    const blink::WebMediaStreamTrack& track)
    : PepperMediaStreamTrackHostBase(host, instance, resource),
      track_(track),
      connected_(false),
      audio_sink_(this) {
  DCHECK(!track_.IsNull());
}

PepperMediaStreamAudioTrackHost::~PepperMediaStreamAudioTrackHost() {
  OnClose();
}

int32_t PepperMediaStreamAudioTrackHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    HostMessageContext* context) {
  PPAPI_BEGIN_MESSAGE_MAP(PepperMediaStreamAudioTrackHost, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_MediaStreamAudioTrack_Configure, OnHostMsgConfigure)
  PPAPI_END_MESSAGE_MAP()
  return PepperMediaStreamTrackHostBase::OnResourceMessageReceived(msg,
                                                                   context);
}

int32_t PepperMediaStreamAudioTrackHost::OnHostMsgConfigure(
    HostMessageContext* context,
    const MediaStreamAudioTrackShared::Attributes& attributes) {
  if (!MediaStreamAudioTrackShared::VerifyAttributes(attributes))
    return PP_ERROR_BADARGUMENT;

  int32_t buffers = attributes.buffers
                        ? std::min(kMaxNumberOfAudioBuffers, attributes.buffers)
                        : kDefaultNumberOfAudioBuffers;
  return audio_sink_.Configure(buffers, attributes.duration,
                               context->MakeReplyMessageContext());
}

void PepperMediaStreamAudioTrackHost::OnClose() {
  if (connected_) {
    blink::WebMediaStreamAudioSink::RemoveFromAudioTrack(&audio_sink_, track_);
    connected_ = false;
  }
  audio_sink_.SendConfigureReply(PP_ERROR_ABORTED);
}

void PepperMediaStreamAudioTrackHost::OnNewBufferEnqueued() {
  int32_t index = buffer_manager()->DequeueBuffer();
  DCHECK_GE(index, 0);
  audio_sink_.EnqueueBuffer(index);
}

void PepperMediaStreamAudioTrackHost::DidConnectPendingHostToResource() {
  if (!connected_) {
    media::AudioParameters format =
        blink::WebMediaStreamAudioSink::GetFormatFromAudioTrack(track_);
    // Although this should only be called on the audio capture thread, that
    // can't happen until the sink is added to the audio track below.
    if (format.IsValid())
      audio_sink_.OnSetFormat(format);

    blink::WebMediaStreamAudioSink::AddToAudioTrack(&audio_sink_, track_);
    connected_ = true;
  }
}

}  // namespace content
