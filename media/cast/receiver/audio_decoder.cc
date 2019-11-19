// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/receiver/audio_decoder.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/sys_byteorder.h"
#include "build/build_config.h"
#include "third_party/opus/src/include/opus.h"

namespace media {
namespace cast {

// Base class that handles the common problem of detecting dropped frames, and
// then invoking the Decode() method implemented by the subclasses to convert
// the encoded payload data into usable audio data.
class AudioDecoder::ImplBase
    : public base::RefCountedThreadSafe<AudioDecoder::ImplBase> {
 public:
  ImplBase(const scoped_refptr<CastEnvironment>& cast_environment,
           Codec codec,
           int num_channels,
           int sampling_rate)
      : cast_environment_(cast_environment),
        codec_(codec),
        num_channels_(num_channels),
        operational_status_(STATUS_UNINITIALIZED) {
    if (num_channels_ <= 0 || sampling_rate <= 0 || sampling_rate % 100 != 0)
      operational_status_ = STATUS_INVALID_CONFIGURATION;
  }

  OperationalStatus InitializationResult() const {
    return operational_status_;
  }

  void DecodeFrame(std::unique_ptr<EncodedFrame> encoded_frame,
                   const DecodeFrameCallback& callback) {
    DCHECK_EQ(operational_status_, STATUS_INITIALIZED);

    bool is_continuous = true;
    DCHECK(!encoded_frame->frame_id.is_null());
    if (!last_frame_id_.is_null()) {
      if (encoded_frame->frame_id > (last_frame_id_ + 1)) {
        RecoverBecauseFramesWereDropped();
        is_continuous = false;
      }
    }
    last_frame_id_ = encoded_frame->frame_id;

    std::unique_ptr<AudioBus> decoded_audio =
        Decode(encoded_frame->mutable_bytes(),
               static_cast<int>(encoded_frame->data.size()));
    if (!decoded_audio) {
      VLOG(2) << "Decoding of frame " << encoded_frame->frame_id << " failed.";
      cast_environment_->PostTask(
          CastEnvironment::MAIN, FROM_HERE,
          base::Bind(callback, base::Passed(&decoded_audio), false));
      return;
    }

    std::unique_ptr<FrameEvent> event(new FrameEvent());
    event->timestamp = cast_environment_->Clock()->NowTicks();
    event->type = FRAME_DECODED;
    event->media_type = AUDIO_EVENT;
    event->rtp_timestamp = encoded_frame->rtp_timestamp;
    event->frame_id = encoded_frame->frame_id;
    cast_environment_->logger()->DispatchFrameEvent(std::move(event));

    cast_environment_->PostTask(CastEnvironment::MAIN,
                                FROM_HERE,
                                base::Bind(callback,
                                           base::Passed(&decoded_audio),
                                           is_continuous));
  }

 protected:
  friend class base::RefCountedThreadSafe<ImplBase>;
  virtual ~ImplBase() = default;

  virtual void RecoverBecauseFramesWereDropped() {}

  // Note: Implementation of Decode() is allowed to mutate |data|.
  virtual std::unique_ptr<AudioBus> Decode(uint8_t* data, int len) = 0;

  const scoped_refptr<CastEnvironment> cast_environment_;
  const Codec codec_;
  const int num_channels_;

  // Subclass' ctor is expected to set this to STATUS_INITIALIZED.
  OperationalStatus operational_status_;

 private:
  FrameId last_frame_id_;

  DISALLOW_COPY_AND_ASSIGN(ImplBase);
};

class AudioDecoder::OpusImpl : public AudioDecoder::ImplBase {
 public:
  OpusImpl(const scoped_refptr<CastEnvironment>& cast_environment,
           int num_channels,
           int sampling_rate)
      : ImplBase(cast_environment,
                 CODEC_AUDIO_OPUS,
                 num_channels,
                 sampling_rate),
        decoder_memory_(new uint8_t[opus_decoder_get_size(num_channels)]),
        opus_decoder_(reinterpret_cast<OpusDecoder*>(decoder_memory_.get())),
        max_samples_per_frame_(kOpusMaxFrameDurationMillis * sampling_rate /
                               1000),
        buffer_(new float[max_samples_per_frame_ * num_channels]) {
    if (ImplBase::operational_status_ != STATUS_UNINITIALIZED)
      return;
    if (opus_decoder_init(opus_decoder_, sampling_rate, num_channels) !=
            OPUS_OK) {
      ImplBase::operational_status_ = STATUS_INVALID_CONFIGURATION;
      return;
    }
    ImplBase::operational_status_ = STATUS_INITIALIZED;
  }

 private:
  ~OpusImpl() final = default;

  void RecoverBecauseFramesWereDropped() final {
    // Passing NULL for the input data notifies the decoder of frame loss.
    const opus_int32 result =
        opus_decode_float(
            opus_decoder_, NULL, 0, buffer_.get(), max_samples_per_frame_, 0);
    DCHECK_GE(result, 0);
  }

  std::unique_ptr<AudioBus> Decode(uint8_t* data, int len) final {
    std::unique_ptr<AudioBus> audio_bus;
    const opus_int32 num_samples_decoded = opus_decode_float(
        opus_decoder_, data, len, buffer_.get(), max_samples_per_frame_, 0);
    if (num_samples_decoded <= 0)
      return audio_bus;  // Decode error.

    // Copy interleaved samples from |buffer_| into a new AudioBus (where
    // samples are stored in planar format, for each channel).
    audio_bus = AudioBus::Create(num_channels_, num_samples_decoded);
    // TODO(miu): This should be moved into AudioBus::FromInterleaved().
    for (int ch = 0; ch < num_channels_; ++ch) {
      const float* src = buffer_.get() + ch;
      const float* const src_end = src + num_samples_decoded * num_channels_;
      float* dest = audio_bus->channel(ch);
      for (; src < src_end; src += num_channels_, ++dest)
        *dest = *src;
    }
    return audio_bus;
  }

  const std::unique_ptr<uint8_t[]> decoder_memory_;
  OpusDecoder* const opus_decoder_;
  const int max_samples_per_frame_;
  const std::unique_ptr<float[]> buffer_;

  // According to documentation in third_party/opus/src/include/opus.h, we must
  // provide enough space in |buffer_| to contain 120ms of samples.  At 48 kHz,
  // then, that means 5760 samples times the number of channels.
  static const int kOpusMaxFrameDurationMillis = 120;

  DISALLOW_COPY_AND_ASSIGN(OpusImpl);
};

class AudioDecoder::Pcm16Impl : public AudioDecoder::ImplBase {
 public:
  Pcm16Impl(const scoped_refptr<CastEnvironment>& cast_environment,
            int num_channels,
            int sampling_rate)
      : ImplBase(cast_environment,
                 CODEC_AUDIO_PCM16,
                 num_channels,
                 sampling_rate) {
    if (ImplBase::operational_status_ != STATUS_UNINITIALIZED)
      return;
    ImplBase::operational_status_ = STATUS_INITIALIZED;
  }

 private:
  ~Pcm16Impl() final = default;

  std::unique_ptr<AudioBus> Decode(uint8_t* data, int len) final {
    std::unique_ptr<AudioBus> audio_bus;
    const int num_samples = len / sizeof(int16_t) / num_channels_;
    if (num_samples <= 0)
      return audio_bus;

    int16_t* const pcm_data = reinterpret_cast<int16_t*>(data);
#if defined(ARCH_CPU_LITTLE_ENDIAN)
    // Convert endianness.
    const int num_elements = num_samples * num_channels_;
    for (int i = 0; i < num_elements; ++i)
      pcm_data[i] = static_cast<int16_t>(base::NetToHost16(pcm_data[i]));
#endif
    audio_bus = AudioBus::Create(num_channels_, num_samples);
    audio_bus->FromInterleaved<SignedInt16SampleTypeTraits>(pcm_data,
                                                            num_samples);
    return audio_bus;
  }

  DISALLOW_COPY_AND_ASSIGN(Pcm16Impl);
};

AudioDecoder::AudioDecoder(
    const scoped_refptr<CastEnvironment>& cast_environment,
    int channels,
    int sampling_rate,
    Codec codec)
    : cast_environment_(cast_environment) {
  switch (codec) {
    case CODEC_AUDIO_OPUS:
      impl_ = new OpusImpl(cast_environment, channels, sampling_rate);
      break;
    case CODEC_AUDIO_PCM16:
      impl_ = new Pcm16Impl(cast_environment, channels, sampling_rate);
      break;
    default:
      NOTREACHED() << "Unknown or unspecified codec.";
      break;
  }
}

AudioDecoder::~AudioDecoder() = default;

OperationalStatus AudioDecoder::InitializationResult() const {
  if (impl_.get())
    return impl_->InitializationResult();
  return STATUS_UNSUPPORTED_CODEC;
}

void AudioDecoder::DecodeFrame(std::unique_ptr<EncodedFrame> encoded_frame,
                               const DecodeFrameCallback& callback) {
  DCHECK(encoded_frame.get());
  DCHECK(!callback.is_null());
  if (!impl_.get() || impl_->InitializationResult() != STATUS_INITIALIZED) {
    callback.Run(base::WrapUnique<AudioBus>(NULL), false);
    return;
  }
  cast_environment_->PostTask(CastEnvironment::AUDIO,
                              FROM_HERE,
                              base::Bind(&AudioDecoder::ImplBase::DecodeFrame,
                                         impl_,
                                         base::Passed(&encoded_frame),
                                         callback));
}

}  // namespace cast
}  // namespace media
