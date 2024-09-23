// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/filters/ffmpeg_audio_decoder.h"

#include <stdint.h>

#include <functional>
#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/audio_discard_helper.h"
#include "media/base/decoder_buffer.h"
#include "media/base/limits.h"
#include "media/base/media_switches.h"
#include "media/base/timestamp_constants.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/ffmpeg/ffmpeg_decoding_loop.h"
#include "media/filters/ffmpeg_glue.h"

namespace media {

// Return the number of channels from the data in |frame|.
static inline int DetermineChannels(AVFrame* frame) {
  return frame->ch_layout.nb_channels;
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
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    MediaLog* media_log)
    : task_runner_(task_runner),
      state_(DecoderState::kUninitialized),
      av_sample_format_(0),
      media_log_(media_log),
      pool_(base::MakeRefCounted<AudioBufferMemoryPool>(
          kFFmpegBufferAddressAlignment)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

FFmpegAudioDecoder::~FFmpegAudioDecoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (state_ != DecoderState::kUninitialized)
    ReleaseFFmpegResources();
}

AudioDecoderType FFmpegAudioDecoder::GetDecoderType() const {
  return AudioDecoderType::kFFmpeg;
}

void FFmpegAudioDecoder::Initialize(const AudioDecoderConfig& config,
                                    CdmContext* /* cdm_context */,
                                    InitCB init_cb,
                                    const OutputCB& output_cb,
                                    const WaitingCB& /* waiting_cb */) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(config.IsValidConfig());

  InitCB bound_init_cb = base::BindPostTaskToCurrentDefault(std::move(init_cb));

  if (config.is_encrypted()) {
    std::move(bound_init_cb)
        .Run(DecoderStatus(
            DecoderStatus::Codes::kUnsupportedEncryptionMode,
            "FFmpegAudioDecoder does not support encrypted content"));
    return;
  }

  // TODO(dalecurtis): Remove this if ffmpeg ever gets xHE-AAC support.
  if (config.profile() == AudioCodecProfile::kXHE_AAC) {
    std::move(bound_init_cb)
        .Run(DecoderStatus(DecoderStatus::Codes::kUnsupportedProfile)
                 .WithData("decoder", "FFmpegAudioDecoder")
                 .WithData("profile", config.profile()));
    return;
  }

  if (!ConfigureDecoder(config)) {
    av_sample_format_ = 0;
    std::move(bound_init_cb).Run(DecoderStatus::Codes::kUnsupportedConfig);
    return;
  }

  // Success!
  config_ = config;
  output_cb_ = base::BindPostTaskToCurrentDefault(output_cb);
  state_ = DecoderState::kNormal;
  std::move(bound_init_cb).Run(DecoderStatus::Codes::kOk);
}

void FFmpegAudioDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                                DecodeCB decode_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(decode_cb);
  CHECK_NE(state_, DecoderState::kUninitialized);
  DecodeCB decode_cb_bound =
      base::BindPostTaskToCurrentDefault(std::move(decode_cb));

  if (state_ == DecoderState::kError) {
    std::move(decode_cb_bound).Run(DecoderStatus::Codes::kFailed);
    return;
  }

  // Do nothing if decoding has finished.
  if (state_ == DecoderState::kDecodeFinished) {
    std::move(decode_cb_bound).Run(DecoderStatus::Codes::kOk);
    return;
  }

  DecodeBuffer(*buffer, std::move(decode_cb_bound));
}

void FFmpegAudioDecoder::Reset(base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  avcodec_flush_buffers(codec_context_.get());
  state_ = DecoderState::kNormal;
  ResetTimestampState(config_);
  task_runner_->PostTask(FROM_HERE, std::move(closure));
}

void FFmpegAudioDecoder::DecodeBuffer(const DecoderBuffer& buffer,
                                      DecodeCB decode_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(state_, DecoderState::kUninitialized);
  DCHECK_NE(state_, DecoderState::kDecodeFinished);
  DCHECK_NE(state_, DecoderState::kError);

  // Make sure we are notified if http://crbug.com/49709 returns.  Issue also
  // occurs with some damaged files.
  if (!buffer.end_of_stream() && buffer.timestamp() == kNoTimestamp) {
    DVLOG(1) << "Received a buffer without timestamps!";
    std::move(decode_cb).Run(DecoderStatus::Codes::kFailed);
    return;
  }

  if (!buffer.end_of_stream() && buffer.is_encrypted()) {
    DLOG(ERROR) << "Encrypted buffer not supported";
    std::move(decode_cb).Run(DecoderStatus::Codes::kUnsupportedEncryptionMode);
    return;
  }

  if (!FFmpegDecode(buffer)) {
    state_ = DecoderState::kError;
    std::move(decode_cb).Run(DecoderStatus::Codes::kFailed);
    return;
  }

  if (buffer.end_of_stream())
    state_ = DecoderState::kDecodeFinished;

  std::move(decode_cb).Run(DecoderStatus::Codes::kOk);
}

bool FFmpegAudioDecoder::FFmpegDecode(const DecoderBuffer& buffer) {
  AVPacket* packet = av_packet_alloc();
  if (buffer.end_of_stream() || buffer.size() == 0) {
    packet->data = nullptr;
    packet->size = 0;
  } else {
    packet->data = const_cast<uint8_t*>(buffer.data());
    packet->size = buffer.size();
    packet->pts =
        ConvertToTimeBase(codec_context_->time_base, buffer.timestamp());

    DCHECK(packet->data);
    DCHECK_GT(packet->size, 0);
  }

  bool decoded_frame_this_loop = false;
  // base::Unretained and std::cref are safe to use with the callback given
  // to DecodePacket() since that callback is only used the function call.
  FFmpegDecodingLoop::DecodeStatus decode_status = decoding_loop_->DecodePacket(
      packet, base::BindRepeating(&FFmpegAudioDecoder::OnNewFrame,
                                  base::Unretained(this), std::cref(buffer),
                                  &decoded_frame_this_loop));
  av_packet_free(&packet);
  switch (decode_status) {
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
          << GetDecoderType() << " failed to decode an audio buffer: "
          << AVErrorToString(decoding_loop_->last_averror_code()) << ", at "
          << buffer.AsHumanReadableString();
      break;
    case FFmpegDecodingLoop::DecodeStatus::kOkay:
      break;
  }

  // Even if we didn't decode a frame this loop, we should still send the packet
  // to the discard helper for caching.
  if (!decoded_frame_this_loop && !buffer.end_of_stream()) {
    const bool result =
        discard_helper_->ProcessBuffers(buffer.time_info(), nullptr);
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
      codec_context_->ch_layout.u.mask, codec_context_->ch_layout.nb_channels);
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
  if (discard_helper_->ProcessBuffers(buffer.time_info(), output.get())) {
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
  codec_context_.reset(avcodec_alloc_context3(nullptr));
  AudioDecoderConfigToAVCodecContext(config, codec_context_.get());

  codec_context_->opaque = this;
  codec_context_->get_buffer2 = GetAudioBufferImpl;

  if (base::FeatureList::IsEnabled(kFFmpegAllowLists)) {
    // Note: FFmpeg will try to free this string, so we must duplicate it.
    codec_context_->codec_whitelist =
        av_strdup(FFmpegGlue::GetAllowedAudioDecoders());
  }

  if (!config.should_discard_decoder_delay())
    codec_context_->flags2 |= AV_CODEC_FLAG2_SKIP_MANUAL;

  AVDictionary* codec_options = nullptr;
  if (config.codec() == AudioCodec::kOpus) {
    codec_context_->request_sample_fmt = AV_SAMPLE_FMT_FLT;

    // Disable phase inversion to avoid artifacts in mono downmix. See
    // http://crbug.com/806219
    if (config.target_output_channel_layout() == CHANNEL_LAYOUT_MONO) {
      int result = av_dict_set(&codec_options, "apply_phase_inv", "0", 0);
      DCHECK_GE(result, 0);
    }
  }

  const AVCodec* codec = avcodec_find_decoder(codec_context_->codec_id);
  if (!codec ||
      avcodec_open2(codec_context_.get(), codec, &codec_options) < 0) {
    DLOG(ERROR) << "Could not initialize audio decoder: "
                << codec_context_->codec_id;
    ReleaseFFmpegResources();
    state_ = DecoderState::kUninitialized;
    return false;
  }
  // Verify avcodec_open2() used all given options.
  DCHECK_EQ(0, av_dict_count(codec_options));

  // Success!
  av_sample_format_ = codec_context_->sample_fmt;

  if (codec_context_->ch_layout.nb_channels != config.channels()) {
    MEDIA_LOG(ERROR, media_log_)
        << "Audio configuration specified " << config.channels()
        << " channels, but FFmpeg thinks the file contains "
        << codec_context_->ch_layout.nb_channels << " channels";
    ReleaseFFmpegResources();
    state_ = DecoderState::kUninitialized;
    return false;
  }

  decoding_loop_ =
      std::make_unique<FFmpegDecodingLoop>(codec_context_.get(), true);
  ResetTimestampState(config);
  return true;
}

void FFmpegAudioDecoder::ResetTimestampState(const AudioDecoderConfig& config) {
  // Opus codec delay is handled by ffmpeg.
  const int codec_delay =
      config.codec() == AudioCodec::kOpus ? 0 : config.codec_delay();
  discard_helper_ = std::make_unique<AudioDiscardHelper>(
      config.samples_per_second(), codec_delay,
      config.codec() == AudioCodec::kVorbis);
  discard_helper_->Reset(codec_delay);
}

int FFmpegAudioDecoder::GetAudioBuffer(struct AVCodecContext* s,
                                       AVFrame* frame,
                                       int flags) {
  DCHECK(s->codec->capabilities & AV_CODEC_CAP_DR1);
  DCHECK_EQ(s->codec_type, AVMEDIA_TYPE_AUDIO);

  // Since this routine is called by FFmpeg when a buffer is required for
  // audio data, use the values supplied by FFmpeg (ignoring the current
  // settings). FFmpegDecode() gets to determine if the buffer is usable or not.
  AVSampleFormat format = static_cast<AVSampleFormat>(frame->format);
  SampleFormat sample_format =
      AVSampleFormatToSampleFormat(format, s->codec_id);
  if (sample_format == kUnknownSampleFormat) {
    DLOG(ERROR) << "Unknown sample format: " << format;
    return AVERROR(EINVAL);
  }

  int channels = DetermineChannels(frame);
  if (channels <= 0 || channels >= limits::kMaxChannels) {
    DLOG(ERROR) << "Requested number of channels (" << channels
                << ") exceeds limit.";
    return AVERROR(EINVAL);
  }

  int bytes_per_channel = SampleFormatToBytesPerChannel(sample_format);
  if (frame->nb_samples <= 0)
    return AVERROR(EINVAL);

  if (s->ch_layout.nb_channels != channels) {
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
          : ChannelLayoutToChromeChannelLayout(s->ch_layout.u.mask,
                                               s->ch_layout.nb_channels);

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
