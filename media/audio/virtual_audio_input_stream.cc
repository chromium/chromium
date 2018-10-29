// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/virtual_audio_input_stream.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "media/audio/virtual_audio_output_stream.h"
#include "media/base/loopback_audio_converter.h"

namespace media {

VirtualAudioInputStream::VirtualAudioInputStream(
    const AudioParameters& params,
    const scoped_refptr<base::SingleThreadTaskRunner>& worker_task_runner,
    const AfterCloseCallback& after_close_cb)
    : worker_task_runner_(worker_task_runner),
      after_close_cb_(after_close_cb),
      callback_(NULL),
      params_(params),
      mixer_(params_, params_, false),
      num_attached_output_streams_(0),
      fake_worker_(worker_task_runner_, params_),
      audio_bus_(AudioBus::Create(params)) {
  DCHECK(params_.IsValid());
  DCHECK(worker_task_runner_.get());

  // VAIS can be constructed on any thread, but will DCHECK that all
  // AudioInputStream methods are called from the same thread.
  thread_checker_.DetachFromThread();
}

VirtualAudioInputStream::~VirtualAudioInputStream() {
  DCHECK(!callback_);

  // Sanity-check: Contract for Add/RemoveOutputStream() requires that all
  // output streams be removed before VirtualAudioInputStream is destroyed.
  DCHECK_EQ(0, num_attached_output_streams_);

  for (auto it = converters_.begin(); it != converters_.end(); ++it) {
    delete it->second;
  }
}

bool VirtualAudioInputStream::Open() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return true;
}

void VirtualAudioInputStream::Start(AudioInputCallback* callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  callback_ = callback;
  fake_worker_.Start(base::Bind(
      &VirtualAudioInputStream::PumpAudio, base::Unretained(this)));
}

void VirtualAudioInputStream::Stop() {
  DCHECK(thread_checker_.CalledOnValidThread());
  fake_worker_.Stop();
  callback_ = NULL;
}

void VirtualAudioInputStream::AddInputProvider(
    AudioConverter::InputCallback* input,
    const AudioParameters& params) {
  DCHECK(thread_checker_.CalledOnValidThread());

  base::AutoLock scoped_lock(converter_network_lock_);

  auto converter = converters_.find(params);
  if (converter == converters_.end()) {
    std::pair<AudioConvertersMap::iterator, bool> result =
        converters_.insert(std::make_pair(
            params, new LoopbackAudioConverter(params, params_, false)));
    converter = result.first;

    // Add to main mixer if we just added a new AudioTransform.
    mixer_.AddInput(converter->second);
  }
  converter->second->AddInput(input);
  ++num_attached_output_streams_;
}

void VirtualAudioInputStream::RemoveInputProvider(
    AudioConverter::InputCallback* input,
    const AudioParameters& params) {
  DCHECK(thread_checker_.CalledOnValidThread());

  base::AutoLock scoped_lock(converter_network_lock_);

  DCHECK(converters_.find(params) != converters_.end());
  converters_[params]->RemoveInput(input);

  --num_attached_output_streams_;
  DCHECK_LE(0, num_attached_output_streams_);
}

void VirtualAudioInputStream::PumpAudio() {
  DCHECK(worker_task_runner_->BelongsToCurrentThread());

  {
    base::AutoLock scoped_lock(converter_network_lock_);
    // Because the audio is being looped-back, the delay until it will be played
    // out is zero.
    mixer_.ConvertWithDelay(0, audio_bus_.get());
  }
  // Because the audio is being looped-back, the delay since since it was
  // recorded is zero.
  callback_->OnData(audio_bus_.get(), base::TimeTicks::Now(), 1.0);
}

void VirtualAudioInputStream::Close() {
  DCHECK(thread_checker_.CalledOnValidThread());

  Stop();  // Make sure callback_ is no longer being used.

  // If a non-null AfterCloseCallback was provided to the constructor, invoke it
  // here.  The callback is moved to a stack-local first since |this| could be
  // destroyed during Run().
  if (after_close_cb_) {
    const AfterCloseCallback cb = after_close_cb_;
    after_close_cb_.Reset();
    cb.Run(this);
  }
}

double VirtualAudioInputStream::GetMaxVolume() {
  return 1.0;
}

void VirtualAudioInputStream::SetVolume(double volume) {}

double VirtualAudioInputStream::GetVolume() {
  return 1.0;
}

bool VirtualAudioInputStream::SetAutomaticGainControl(bool enabled) {
  return false;
}

bool VirtualAudioInputStream::GetAutomaticGainControl() {
  return false;
}

bool VirtualAudioInputStream::IsMuted() {
  return false;
}

void VirtualAudioInputStream::SetOutputDeviceForAec(
    const std::string& output_device_id) {
  // Not supported. Do nothing.
}

}  // namespace media
