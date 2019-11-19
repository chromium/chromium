// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/audio_pump.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_sample_types.h"
#include "media/base/channel_layout.h"
#include "media/base/channel_mixer.h"
#include "remoting/codec/audio_encoder.h"
#include "remoting/proto/audio.pb.h"
#include "remoting/protocol/audio_source.h"
#include "remoting/protocol/audio_stub.h"

namespace {

int CalculateFrameCount(const remoting::AudioPacket& packet) {
  return packet.data(0).size() / packet.channels() / packet.bytes_per_sample();
}

std::unique_ptr<media::AudioBus> AudioPacketToAudioBus(
    const remoting::AudioPacket& packet) {
  const int frame_count = CalculateFrameCount(packet);
  DCHECK_GT(frame_count, 0);
  std::unique_ptr<media::AudioBus> result =
      media::AudioBus::Create(packet.channels(), frame_count);
  result->FromInterleaved<media::SignedInt16SampleTypeTraits>(
      reinterpret_cast<const int16_t*>(packet.data(0).data()), frame_count);
  return result;
}

std::unique_ptr<remoting::AudioPacket> AudioBusToAudioPacket(
    const media::AudioBus& packet) {
  std::unique_ptr<remoting::AudioPacket> result =
      std::make_unique<remoting::AudioPacket>();
  result->add_data()->resize(
      packet.channels() * packet.frames() * sizeof(int16_t));
  packet.ToInterleaved<media::SignedInt16SampleTypeTraits>(
      packet.frames(),
      reinterpret_cast<int16_t*>(&(result->mutable_data(0)->at(0))));
  result->set_encoding(remoting::AudioPacket::ENCODING_RAW);
  result->set_channels(
      static_cast<remoting::AudioPacket::Channels>(packet.channels()));
  result->set_bytes_per_sample(remoting::AudioPacket::BYTES_PER_SAMPLE_2);
  return result;
}

media::ChannelLayout RetrieveLayout(const remoting::AudioPacket& packet) {
  // This switch should match AudioPacket::Channels enum in audio.proto.
  switch (packet.channels()) {
    case remoting::AudioPacket::CHANNELS_INVALID:
      return media::CHANNEL_LAYOUT_UNSUPPORTED;
    case remoting::AudioPacket::CHANNELS_MONO:
      return media::CHANNEL_LAYOUT_MONO;
    case remoting::AudioPacket::CHANNELS_STEREO:
      return media::CHANNEL_LAYOUT_STEREO;
    case remoting::AudioPacket::CHANNELS_SURROUND:
      return media::CHANNEL_LAYOUT_SURROUND;
    case remoting::AudioPacket::CHANNELS_4_0:
      return media::CHANNEL_LAYOUT_4_0;
    case remoting::AudioPacket::CHANNELS_4_1:
      return media::CHANNEL_LAYOUT_4_1;
    case remoting::AudioPacket::CHANNELS_5_1:
      return media::CHANNEL_LAYOUT_5_1;
    case remoting::AudioPacket::CHANNELS_6_1:
      return media::CHANNEL_LAYOUT_6_1;
    case remoting::AudioPacket::CHANNELS_7_1:
      return media::CHANNEL_LAYOUT_7_1;
  }
  NOTREACHED() << "Invalid AudioPacket::Channels";
  return media::CHANNEL_LAYOUT_UNSUPPORTED;
}

}  // namespace

namespace remoting {
namespace protocol {

// Limit the data stored in the pending send buffers to 250ms.
const int kMaxBufferedIntervalMs = 250;

class AudioPump::Core {
 public:
  Core(base::WeakPtr<AudioPump> pump,
       std::unique_ptr<AudioSource> audio_source,
       std::unique_ptr<AudioEncoder> audio_encoder);
  ~Core();

  void Start();
  void Pause(bool pause);

  void OnPacketSent(int size);

 private:
  std::unique_ptr<AudioPacket> Downmix(std::unique_ptr<AudioPacket> packet);

  void EncodeAudioPacket(std::unique_ptr<AudioPacket> packet);

  base::ThreadChecker thread_checker_;

  base::WeakPtr<AudioPump> pump_;

  scoped_refptr<base::SingleThreadTaskRunner> pump_task_runner_;

  std::unique_ptr<AudioSource> audio_source_;
  std::unique_ptr<AudioEncoder> audio_encoder_;

  bool enabled_;

  // Number of bytes in the queue that have been encoded but haven't been sent
  // yet.
  int bytes_pending_;

  std::unique_ptr<media::ChannelMixer> mixer_;
  media::ChannelLayout mixer_input_layout_ = media::CHANNEL_LAYOUT_NONE;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

AudioPump::Core::Core(base::WeakPtr<AudioPump> pump,
                      std::unique_ptr<AudioSource> audio_source,
                      std::unique_ptr<AudioEncoder> audio_encoder)
    : pump_(pump),
      pump_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      audio_source_(std::move(audio_source)),
      audio_encoder_(std::move(audio_encoder)),
      enabled_(true),
      bytes_pending_(0) {
  thread_checker_.DetachFromThread();
}

AudioPump::Core::~Core() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void AudioPump::Core::Start() {
  DCHECK(thread_checker_.CalledOnValidThread());

  audio_source_->Start(
      base::Bind(&Core::EncodeAudioPacket, base::Unretained(this)));
}

void AudioPump::Core::Pause(bool pause) {
  DCHECK(thread_checker_.CalledOnValidThread());

  enabled_ = !pause;
}

void AudioPump::Core::OnPacketSent(int size) {
  DCHECK(thread_checker_.CalledOnValidThread());

  bytes_pending_ -= size;
  DCHECK_GE(bytes_pending_, 0);
}

void AudioPump::Core::EncodeAudioPacket(std::unique_ptr<AudioPacket> packet) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(packet);

  int max_buffered_bytes =
      audio_encoder_->GetBitrate() * kMaxBufferedIntervalMs / 1000 / 8;
  if (!enabled_ || bytes_pending_ > max_buffered_bytes) {
    return;
  }

  if (packet->channels() > AudioPacket::CHANNELS_STEREO) {
    packet = Downmix(std::move(packet));
  }

  std::unique_ptr<AudioPacket> encoded_packet =
      audio_encoder_->Encode(std::move(packet));

  // The audio encoder returns a null audio packet if there's no audio to send.
  if (!encoded_packet) {
    return;
  }

  int packet_size = encoded_packet->ByteSize();
  bytes_pending_ += packet_size;

  pump_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AudioPump::SendAudioPacket, pump_,
                                std::move(encoded_packet), packet_size));
}

std::unique_ptr<AudioPacket> AudioPump::Core::Downmix(
    std::unique_ptr<AudioPacket> packet) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(packet);
  DCHECK_EQ(packet->data_size(), 1);
  DCHECK_EQ(packet->bytes_per_sample(), AudioPacket::BYTES_PER_SAMPLE_2);

  const media::ChannelLayout input_layout = RetrieveLayout(*packet);
  DCHECK_NE(input_layout, media::CHANNEL_LAYOUT_UNSUPPORTED);
  DCHECK_NE(input_layout, media::CHANNEL_LAYOUT_MONO);
  DCHECK_NE(input_layout, media::CHANNEL_LAYOUT_STEREO);

  if (!mixer_ || mixer_input_layout_ != input_layout) {
    mixer_input_layout_ = input_layout;
    mixer_ = std::make_unique<media::ChannelMixer>(
        input_layout, media::CHANNEL_LAYOUT_STEREO);
  }

  std::unique_ptr<media::AudioBus> input = AudioPacketToAudioBus(*packet);
  DCHECK(input);
  std::unique_ptr<media::AudioBus> output =
      media::AudioBus::Create(AudioPacket::CHANNELS_STEREO, input->frames());
  mixer_->Transform(input.get(), output.get());

  std::unique_ptr<AudioPacket> result = AudioBusToAudioPacket(*output);
  result->set_sampling_rate(packet->sampling_rate());
  return result;
}

AudioPump::AudioPump(
    scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner,
    std::unique_ptr<AudioSource> audio_source,
    std::unique_ptr<AudioEncoder> audio_encoder,
    AudioStub* audio_stub)
    : audio_task_runner_(audio_task_runner), audio_stub_(audio_stub) {
  DCHECK(audio_stub_);

  core_.reset(new Core(weak_factory_.GetWeakPtr(), std::move(audio_source),
                       std::move(audio_encoder)));

  audio_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Core::Start, base::Unretained(core_.get())));
}

AudioPump::~AudioPump() {
  DCHECK(thread_checker_.CalledOnValidThread());

  audio_task_runner_->DeleteSoon(FROM_HERE, core_.release());
}

void AudioPump::Pause(bool pause) {
  DCHECK(thread_checker_.CalledOnValidThread());

  audio_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Core::Pause, base::Unretained(core_.get()), pause));
}

void AudioPump::SendAudioPacket(std::unique_ptr<AudioPacket> packet, int size) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(packet);

  audio_stub_->ProcessAudioPacket(
      std::move(packet),
      base::Bind(&AudioPump::OnPacketSent, weak_factory_.GetWeakPtr(), size));
}

void AudioPump::OnPacketSent(int size) {
  audio_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Core::OnPacketSent, base::Unretained(core_.get()), size));
}

}  // namespace protocol
}  // namespace remoting
