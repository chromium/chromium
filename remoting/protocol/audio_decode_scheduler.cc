// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/audio_decode_scheduler.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "remoting/codec/audio_decoder.h"
#include "remoting/proto/audio.pb.h"
#include "remoting/protocol/audio_stub.h"

namespace remoting::protocol {

AudioDecodeScheduler::AudioDecodeScheduler(
    scoped_refptr<base::SingleThreadTaskRunner> audio_decode_task_runner,
    base::WeakPtr<protocol::AudioStub> audio_consumer)
    : audio_decode_task_runner_(audio_decode_task_runner),
      audio_consumer_(audio_consumer) {}

AudioDecodeScheduler::~AudioDecodeScheduler() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  audio_decode_task_runner_->DeleteSoon(FROM_HERE, decoder_.release());
}

void AudioDecodeScheduler::Initialize(const protocol::SessionConfig& config) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!decoder_);
  decoder_ = AudioDecoder::CreateAudioDecoder(config);
}

void AudioDecodeScheduler::ProcessAudioPacket(
    std::unique_ptr<AudioPacket> packet,
    base::OnceClosure done) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  audio_decode_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&AudioDecoder::Decode, base::Unretained(decoder_.get()),
                     std::move(packet)),
      base::BindOnce(&AudioDecodeScheduler::ProcessDecodedPacket,
                     weak_factory_.GetWeakPtr(), std::move(done)));
}

void AudioDecodeScheduler::ProcessDecodedPacket(
    base::OnceClosure done,
    std::unique_ptr<AudioPacket> packet) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!packet || !audio_consumer_) {
    std::move(done).Run();
    return;
  }

  audio_consumer_->ProcessAudioPacket(std::move(packet), std::move(done));
}

}  // namespace remoting::protocol
