// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/ffmpeg_audio_decoder.h"

#include <stdint.h>

#include <functional>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/single_thread_task_runner.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/audio_discard_helper.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/decoder_buffer.h"
#include "media/base/limits.h"
#include "media/base/timestamp_constants.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/ffmpeg/ffmpeg_decoding_loop.h"
#include "media/filters/ffmpeg_glue.h"

namespace media {

// Return the number of channels from the data in |frame|.
static inline int DetermineChannels(AVFrame* frame) {
  return frame->channels;
}

// Called by FFmpeg's allocation routine to allocate a buffer. Uses
// AVCodecContext.opaque to get the object reference in order to call
// GetAudioBuffer() to do the actual allocation.
static int GetAudioBufferImpl(struct AVCodecContext* s,
                              AVFrame* frame,
                              int flags) {
  FFmpegAudioDecoder* decoder = static_cast<FFmpegAudioDecoder*>(s->opaque);
  return decoder->GetAudioBuffer(s, frame, flags);
}

// Called by FFmpeg's allocation routine to free a buffer. |opaque| is the
// AudioBuffer allocated, so unref it.
static void ReleaseAudioBufferImpl(void* opaque, uint8_t* data) {
  if (opaque)
    static_cast<AudioBuffer*>(opaque)->Release();
}

FFmpegAudioDecoder::FFmpegAudioDecoder(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    MediaLog* media_log)
    : task_runner_(task_runner),
      state_(kUninitialized),
      av_sample_format_(0),
      media_log_(media_log),
      pool_(new AudioBufferMemoryPool()) {}

FFmpegAudioDecoder::~FFmpegAudioDecoder() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (state_ != kUninitialized)
    ReleaseFFmpegResources();
}

std::string FFmpegAudioDecoder::GetDisplayName() const {
  return "FFmpegAudioDecoder";
}

void FFmpegAudioDecoder::Initialize(const AudioDecoderConfig& config,
                                    CdmContext* /* cdm_context */,
                                    InitCB init_cb,
                                    const OutputCB& output_cb,
                                    const WaitingCB& /* waiting_cb */) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(config.IsValidConfig());

  InitCB bound_init_cb = BindToCurrentLoop(std::move(init_cb));

  if (config.is_encrypted()) {
    std::move(bound_init_cb).Run(false);
    return;
  }

  if (!ConfigureDecoder(config)) {
    av_sample_format_ = 0;
    std::move(bound_init_cb).Run(false);
    return;
  }

  // Success!
  config_ = config;
  output_cb_ = BindToCurrentLoop(output_cb);
  state_ = kNormal;
  std::move(bound_init_cb).Run(true);
}

void FFmpegAudioDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                                const DecodeCB& decode_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(decode_cb);
  CHECK_NE(state_, kUninitialized);
  DecodeCB decode_cb_bound = BindToCurrentLoop(decode_cb);

  if (state_ == kError) {
    decode_cb_bound.Run(DecodeStatus::DECODE_ERROR);
    return;
  }

  // Do nothing if decoding has finished.
  if (state_ == kDecodeFinished) {
    decode_cb_bound.Run(DecodeStatus::OK);
    return;
  }

  DecodeBuffer(*buffer, decode_cb_bound);
}

void FFmpegAudioDecoder::Reset(base::OnceClosure closure) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  avcodec_flush_buffers(codec_context_.get());
  state_ = kNormal;
  ResetTimestampState(config_);
  task_runner_->PostTask(FROM_HERE, std::move(closure));
}

void FFmpegAudioDecoder::DecodeBuffer(const DecoderBuffer& buffer,
                                      const DecodeCB& decode_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_NE(state_, kUninitialized);
  DCHECK_NE(state_, kDecodeFinished);
  DCHECK_NE(state_, kError);

  // Make sure we are notified if http://crbug.com/49709 returns.  Issue also
  // occurs with some damaged files.
  if (!buffer.end_of_stream() && buffer.timestamp() == kNoTimestamp) {
    DVLOG(1) << "Received a buffer without timestamps!";
    decode_cb.Run(DecodeStatus::DECODE_ERROR);
    return;
  }

  if (!FFmpegDecode(buffer)) {
    state_ = kError;
    decode_cb.Run(DecodeStatus::DECODE_ERROR);
    return;
  }

  if (buffer.end_of_stream())
    state_ = kDecodeFinished;

  decode_cb.Run(DecodeStatus::OK);
}

bool FFmpegAudioDecoder::FFmpegDecode(const DecoderBuffer& buffer) {
  AVPacket packet;
  av_init_packet(&packet);
  if (buffer.end_of_stream()) {
    packet.data = NULL;
    packet.size = 0;
  } else {
    packet.data = const_cast<uint8_t*>(buffer.data());
    packet.size = buffer.data_size();

    DCHECK(packet.data);
    DCHECK_GT(packet.size, 0);
  }

  bool decoded_frame_this_loop = false;
  // base::Unretained and std::cref are safe to use with the callback given
  // to DecodePacket() since that callback is only used the function call.
  switch (decoding_loop_->DecodePacket(
      &packet, base::BindRepeating(&FFmpegAudioDecoder::OnNewFrame,
                                   base::Unretained(this), std::cref(buffer),
                                   &decoded_frame_this_loop))) {
    case FFmpegDecodingLoop::DecodeStatus::kSendPacketFailed:
      MEDIA_LOG(ERROR, media_log_)
          << "Failed to send audio packet for decoding: "
          << buffer.AsHumanReadableString();
      return false;
    case FFmpegDecodingLoop::DecodeStatus::kFrameProcessingFailed:
      // OnNewFrame() should have already issued a MEDIA_LOG for this.
      return false;
    case FFmpegDecodingLoop::DecodeStatus::kDecodeFrameFailed:
      DCHECK(!buffer.end_of_stream())
          << "End of stream buffer produced an error! "
          << "This is quite possibly a bug in the audio decoder not handling "
          << "end of stream AVPackets correctly.";

      MEDIA_LOG(DEBUG, media_log_)
          << GetDisplayName() << " failed to decode an audio buffer: "
          << AVErrorToString(decoding_loop_->last_averror_code()) << ", at "
          << buffer.AsHumanReadableString();
      break;
    case FFmpegDecodingLoop::DecodeStatus::kOkay:
      break;
  }

  // Even if we didn't decode a frame this loop, we should still send the packet
  // to the discard helper for caching.
  if (!decoded_frame_this_loop && !buffer.end_of_stream()) {
    const bool result = discard_helper_->ProcessBuffers(buffer, nullptr);
    DCHECK(!result);
  }

  return true;
}

bool FFmpegAudioDecoder::OnNewFrame(const DecoderBuffer& buffer,
                                    bool* decoded_frame_this_loop,
                                    AVFrame* frame) {
  const int channels = DetermineChannels(frame);

  // Translate unsupported into discrete layouts for discrete configurations;
  // ffmpeg does not have a labeled discrete configuration internally.
  ChannelLayout channel_layout = ChannelLayoutToChromeChannelLayout(
      codec_context_->channel_layout, codec_context_->channels);
  if (channel_layout == CHANNEL_LAYOUT_UNSUPPORTED &&
      config_.channel_layout() == CHANNEL_LAYOUT_DISCRETE) {
    channel_layout = CHANNEL_LAYOUT_DISCRETE;
  }

  const bool is_sample_rate_change =
      frame->sample_rate != config_.samples_per_second();
  const bool is_config_change = is_sample_rate_change ||
                                channels != config_.channels() ||
                                channel_layout != config_.channel_layout();
  if (is_config_change) {
    // Sample format is never expected to change.
    if (frame->format != av_sample_format_) {
      MEDIA_LOG(ERROR, media_log_)
          << "Unsupported midstream configuration change!"
          << " Sample Rate: " << frame->sample_rate << " vs "
          << config_.samples_per_second()
          << " ChannelLayout: " << channel_layout << " vs "
          << config_.channel_layout() << " << Channels: " << channels << " vs "
          << config_.channels() << ", Sample Format: " << frame->format
          << " vs " << av_sample_format_;
      // This is an unrecoverable error, so bail out.
      return false;
    }

    MEDIA_LOG(DEBUG, media_log_)
        << " Detected midstream configuration change"
        << " PTS:" << buffer.timestamp().InMicroseconds()
        << " Sample Rate: " << frame->sample_rate << " vs "
        << config_.samples_per_second() << ", ChannelLayout: " << channel_layout
        << " vs " << config_.channel_layout() << ", Channels: " << channels
        << " vs " << config_.channels();
    config_.Initialize(config_.codec(), config_.sample_format(), channel_layout,
                       frame->sample_rate, config_.extra_data(),
                       config_.encryption_scheme(), config_.seek_preroll(),
                       config_.codec_delay());
    if (is_sample_rate_change)
      ResetTimestampState(config_);
  }

  // Get the AudioBuffer that the data was decoded into. Adjust the number
  // of frames, in case fewer than requested were actually decoded.
  scoped_refptr<AudioBuffer> output =
      reinterpret_cast<AudioBuffer*>(av_buffer_get_opaque(frame->buf[0]));

  DCHECK_EQ(config_.channels(), output->channel_count());
  const int unread_frames = output->frame_count() - frame->nb_samples;
  DCHECK_GE(unread_frames, 0);
  if (unread_frames > 0)
    output->TrimEnd(unread_frames);

  *decoded_frame_this_loop = true;
  if (discard_helper_->ProcessBuffers(buffer, output.get())) {
    if (is_config_change &&
        output->sample_rate() != config_.samples_per_second()) {
      // At the boundary of the config change, FFmpeg's AAC decoder gives the
      // previous sample rate when calling our GetAudioBuffer. Set the correct
      // sample rate before sending the buffer along.
      // TODO(chcunningham): Fix FFmpeg and upstream it.
      output->AdjustSampleRate(config_.samples_per_second());
    }
    output_cb_.Run(output);
  }

  return true;
}

void FFmpegAudioDecoder::ReleaseFFmpegResources() {
  decoding_loop_.reset();
  codec_context_.reset();
}

bool FFmpegAudioDecoder::ConfigureDecoder(const AudioDecoderConfig& config) {
  DCHECK(config.IsValidConfig());
  DCHECK(!config.is_encrypted());

  // Release existing decoder resources if necessary.
  ReleaseFFmpegResources();

  // Initialize AVCodecContext structure.
  codec_context_.reset(avcodec_alloc_context3(NULL));
  AudioDecoderConfigToAVCodecContext(config, codec_context_.get());

  codec_context_->opaque = this;
  codec_context_->get_buffer2 = GetAudioBufferImpl;

  if (!config.should_discard_decoder_delay())
    codec_context_->flags2 |= AV_CODEC_FLAG2_SKIP_MANUAL;

  AVDictionary* codec_options = NULL;
  if (config.codec() == kCodecOpus) {
    codec_context_->request_sample_fmt = AV_SAMPLE_FMT_FLT;

    // Disable phase inversion to avoid artifacts in mono downmix. See
    // http://crbug.com/806219
    if (config.target_output_channel_layout() == CHANNEL_LAYOUT_MONO) {
      int result = av_dict_set(&codec_options, "apply_phase_inv", "0", 0);
      DCHECK_GE(result, 0);
    }
  }

  AVCodec* codec = avcodec_find_decoder(codec_context_->codec_id);
  if (!codec ||
      avcodec_open2(codec_context_.get(), codec, &codec_options) < 0) {
    DLOG(ERROR) << "Could not initialize audio decoder: "
                << codec_context_->codec_id;
    ReleaseFFmpegResources();
    state_ = kUninitialized;
    return false;
  }
  // Verify avcodec_open2() used all given options.
  DCHECK_EQ(0, av_dict_count(codec_options));

  // Success!
  av_sample_format_ = codec_context_->sample_fmt;

  if (codec_context_->channels != config.channels()) {
    MEDIA_LOG(ERROR, media_log_)
        << "Audio configuration specified " << config.channels()
        << " channels, but FFmpeg thinks the file contains "
        << codec_context_->channels << " channels";
    ReleaseFFmpegResources();
    state_ = kUninitialized;
    return false;
  }

  decoding_loop_.reset(new FFmpegDecodingLoop(codec_context_.get(), true));
  ResetTimestampState(config);
  return true;
}

void FFmpegAudioDecoder::ResetTimestampState(const AudioDecoderConfig& config) {
  // Opus codec delay is handled by ffmpeg.
  const int codec_delay =
      config.codec() == kCodecOpus ? 0 : config.codec_delay();
  discard_helper_.reset(new AudioDiscardHelper(config.samples_per_second(),
                                               codec_delay,
                                               config.codec() == kCodecVorbis));
  discard_helper_->Reset(codec_delay);
}

int FFmpegAudioDecoder::GetAudioBuffer(struct AVCodecContext* s,
                                       AVFrame* frame,
                                       int flags) {
  DCHECK(s->codec->capabilities & AV_CODEC_CAP_DR1);
  DCHECK_EQ(s->codec_type, AVMEDIA_TYPE_AUDIO);

  // Since this routine is called by FFmpeg when a buffer is required for
  // audio data, use the values supplied by FFmpeg (ignoring the current
  // settings). FFmpegDecode() gets to determine if the buffer is useable or
  // not.
  AVSampleFormat format = static_cast<AVSampleFormat>(frame->format);
  SampleFormat sample_format =
      AVSampleFormatToSampleFormat(format, s->codec_id);
  int channels = DetermineChannels(frame);
  if (channels <= 0 || channels >= limits::kMaxChannels) {
    DLOG(ERROR) << "Requested number of channels (" << channels
                << ") exceeds limit.";
    return AVERROR(EINVAL);
  }

  int bytes_per_channel = SampleFormatToBytesPerChannel(sample_format);
  if (frame->nb_samples <= 0)
    return AVERROR(EINVAL);

  if (s->channels != channels) {
    DLOG(ERROR) << "AVCodecContext and AVFrame disagree on channel count.";
    return AVERROR(EINVAL);
  }

  if (s->sample_rate != frame->sample_rate) {
    DLOG(ERROR) << "AVCodecContext and AVFrame disagree on sample rate."
                << s->sample_rate << " vs " << frame->sample_rate;
    return AVERROR(EINVAL);
  }
  if (s->sample_rate < limits::kMinSampleRate ||
      s->sample_rate > limits::kMaxSampleRate) {
    DLOG(ERROR) << "Requested sample rate (" << s->sample_rate
                << ") is outside supported range (" << limits::kMinSampleRate
                << " to " << limits::kMaxSampleRate << ").";
    return AVERROR(EINVAL);
  }

  // Determine how big the buffer should be and allocate it. FFmpeg may adjust
  // how big each channel data is in order to meet the alignment policy, so
  // we need to take this into consideration.
  int buffer_size_in_bytes = av_samples_get_buffer_size(
      &frame->linesize[0], channels, frame->nb_samples, format,
      0 /* align, use ffmpeg default */);
  // Check for errors from av_samples_get_buffer_size().
  if (buffer_size_in_bytes < 0)
    return buffer_size_in_bytes;
  int frames_required = buffer_size_in_bytes / bytes_per_channel / channels;
  DCHECK_GE(frames_required, frame->nb_samples);

  ChannelLayout channel_layout =
      config_.channel_layout() == CHANNEL_LAYOUT_DISCRETE
          ? CHANNEL_LAYOUT_DISCRETE
          : ChannelLayoutToChromeChannelLayout(s->channel_layout, s->channels);

  if (channel_layout == CHANNEL_LAYOUT_UNSUPPORTED) {
    DLOG(ERROR) << "Unsupported channel layout.";
    return AVERROR(EINVAL);
  }

  scoped_refptr<AudioBuffer> buffer =
      AudioBuffer::CreateBuffer(sample_format, channel_layout, channels,
                                s->sample_rate, frames_required, pool_);

  // Initialize the data[] and extended_data[] fields to point into the memory
  // allocated for AudioBuffer. |number_of_planes| will be 1 for interleaved
  // audio and equal to |channels| for planar audio.
  int number_of_planes = buffer->channel_data().size();
  if (number_of_planes <= AV_NUM_DATA_POINTERS) {
    DCHECK_EQ(frame->extended_data, frame->data);
    for (int i = 0; i < number_of_planes; ++i)
      frame->data[i] = buffer->channel_data()[i];
  } else {
    // There are more channels than can fit into data[], so allocate
    // extended_data[] and fill appropriately.
    frame->extended_data = static_cast<uint8_t**>(
        av_malloc(number_of_planes * sizeof(*frame->extended_data)));
    int i = 0;
    for (; i < AV_NUM_DATA_POINTERS; ++i)
      frame->extended_data[i] = frame->data[i] = buffer->channel_data()[i];
    for (; i < number_of_planes; ++i)
      frame->extended_data[i] = buffer->channel_data()[i];
  }

  // Now create an AVBufferRef for the data just allocated. It will own the
  // reference to the AudioBuffer object.
  AudioBuffer* opaque = buffer.get();
  opaque->AddRef();
  frame->buf[0] = av_buffer_create(frame->data[0], buffer_size_in_bytes,
                                   ReleaseAudioBufferImpl, opaque, 0);
  return 0;
}

}  // namespace media
