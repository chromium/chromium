// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/audio/audio_player_android.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "build/build_config.h"

namespace remoting {

const int kFrameSizeMs = 40;
const int kNumOfBuffers = 1;

static_assert(AudioPlayer::kChannels == 2,
              "AudioPlayer must be feeding 2 channels data.");
const int kChannelMask = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;

// TODO(nicholss): Update legacy audio player to use new audio buffer code.

AudioPlayerAndroid::AudioPlayerAndroid() {
  if (slCreateEngine(&engine_object_, 0, nullptr, 0, nullptr, nullptr) !=
          SL_RESULT_SUCCESS ||
      (*engine_object_)->Realize(engine_object_, SL_BOOLEAN_FALSE) !=
          SL_RESULT_SUCCESS ||
      (*engine_object_)
              ->GetInterface(engine_object_, SL_IID_ENGINE, &engine_) !=
          SL_RESULT_SUCCESS ||
      (*engine_)->CreateOutputMix(engine_, &output_mix_object_, 0, nullptr,
                                  nullptr) != SL_RESULT_SUCCESS ||
      (*output_mix_object_)->Realize(output_mix_object_, SL_BOOLEAN_FALSE) !=
          SL_RESULT_SUCCESS) {
    LOG(ERROR) << "Failed to initialize OpenSL ES.";
  }
}

AudioPlayerAndroid::~AudioPlayerAndroid() {
  DestroyPlayer();
  if (output_mix_object_) {
    (*output_mix_object_)->Destroy(output_mix_object_);
  }
  if (engine_object_) {
    (*engine_object_)->Destroy(engine_object_);
  }
}

base::WeakPtr<AudioPlayerAndroid> AudioPlayerAndroid::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

uint32_t AudioPlayerAndroid::GetSamplesPerFrame() {
  return sample_per_frame_;
}

bool AudioPlayerAndroid::ResetAudioPlayer(
    AudioPacket::SamplingRate sampling_rate) {
  if (!output_mix_object_) {
    // output mixer not successfully created in ctor.
    return false;
  }
  DestroyPlayer();
  sample_per_frame_ =
      kFrameSizeMs * sampling_rate / base::Time::kMillisecondsPerSecond;
  buffer_size_ = kChannels * kSampleSizeBytes * sample_per_frame_;
  frame_buffer_.reset(new uint8_t[buffer_size_]);
  FillWithSamples(frame_buffer_.get(), buffer_size_);
  SLDataLocator_AndroidSimpleBufferQueue locator_bufqueue;
  locator_bufqueue.locatorType = SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE;
  locator_bufqueue.numBuffers = kNumOfBuffers;
  SLDataFormat_PCM format = CreatePcmFormat(sampling_rate);
  SLDataSource source = {&locator_bufqueue, &format};
  SLDataLocator_OutputMix locator_out;
  locator_out.locatorType = SL_DATALOCATOR_OUTPUTMIX;
  locator_out.outputMix = output_mix_object_;
  SLDataSink sink;
  sink.pLocator = &locator_out;
  sink.pFormat = nullptr;

  const SLInterfaceID ids[] = {SL_IID_BUFFERQUEUE};
  const SLboolean reqs[] = {SL_BOOLEAN_TRUE};

  if ((*engine_)->CreateAudioPlayer(engine_, &player_object_, &source, &sink,
                                    base::size(ids), ids,
                                    reqs) != SL_RESULT_SUCCESS ||
      (*player_object_)->Realize(player_object_, SL_BOOLEAN_FALSE) !=
          SL_RESULT_SUCCESS ||
      (*player_object_)->GetInterface(player_object_, SL_IID_PLAY, &player_) !=
          SL_RESULT_SUCCESS ||
      (*player_object_)
              ->GetInterface(player_object_, SL_IID_BUFFERQUEUE,
                             &buffer_queue_) != SL_RESULT_SUCCESS ||
      (*buffer_queue_)
              ->RegisterCallback(buffer_queue_,
                                 &AudioPlayerAndroid::BufferQueueCallback,
                                 this) != SL_RESULT_SUCCESS ||
      (*player_)->SetPlayState(player_, SL_PLAYSTATE_PLAYING) !=
          SL_RESULT_SUCCESS ||

      // The player will only ask for more data after it consumes all its
      // buffers. Having an empty queue will not trigger it to ask for more
      // data.
      (*buffer_queue_)
              ->Enqueue(buffer_queue_, frame_buffer_.get(), buffer_size_) !=
          SL_RESULT_SUCCESS) {
    LOG(ERROR) << "Failed to initialize the player.";
    return false;
  }
  return true;
}

// static
void AudioPlayerAndroid::BufferQueueCallback(
    SLAndroidSimpleBufferQueueItf caller,
    void* args) {
  AudioPlayerAndroid* player = static_cast<AudioPlayerAndroid*>(args);
  player->FillWithSamples(player->frame_buffer_.get(), player->buffer_size_);
  if ((*caller)->Enqueue(caller, player->frame_buffer_.get(),
                         player->buffer_size_) != SL_RESULT_SUCCESS) {
    LOG(ERROR) << "Failed to enqueue the frame.";
  }
}

// static
SLDataFormat_PCM AudioPlayerAndroid::CreatePcmFormat(int sampling_rate) {
  SLDataFormat_PCM format;
  format.formatType = SL_DATAFORMAT_PCM;
  format.numChannels = kChannels;
  switch (sampling_rate) {
    case AudioPacket::SAMPLING_RATE_44100:
      format.samplesPerSec = SL_SAMPLINGRATE_44_1;
      break;
    case AudioPacket::SAMPLING_RATE_48000:
      format.samplesPerSec = SL_SAMPLINGRATE_48;
      break;
    default:
      LOG(FATAL) << "Unsupported audio sampling rate: " << sampling_rate;
  }  // samplesPerSec is in mHz. OpenSL doesn't name this field well.
  format.bitsPerSample = kSampleSizeBytes * 8;
  format.containerSize = kSampleSizeBytes * 8;
#if defined(ARCH_CPU_LITTLE_ENDIAN)
  format.endianness = SL_BYTEORDER_LITTLEENDIAN;
#else
  format.endianness = SL_BYTEORDER_BIGENDIAN;
#endif
  format.channelMask = kChannelMask;
  return format;
}

void AudioPlayerAndroid::DestroyPlayer() {
  if (player_object_) {
    (*player_object_)->Destroy(player_object_);
    player_object_ = nullptr;
  }
  frame_buffer_.reset();
  buffer_size_ = 0;
  player_ = nullptr;
  buffer_queue_ = nullptr;
}

}  // namespace remoting
