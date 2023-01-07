// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_output_proxy.h"

#include "base/check_op.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_output_dispatcher.h"

namespace media {

AudioOutputProxy::AudioOutputProxy(
    base::WeakPtr<AudioOutputDispatcher> dispatcher)
    : dispatcher_(std::move(dispatcher)), state_(kCreated), volume_(1.0) {
  DCHECK(dispatcher_);
}

AudioOutputProxy::~AudioOutputProxy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(state_ == kCreated || state_ == kClosed) << "State is: " << state_;
}

bool AudioOutputProxy::Open() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, kCreated);

  if (!dispatcher_ || !dispatcher_->OpenStream()) {
    state_ = kOpenError;
    return false;
  }

  state_ = kOpened;
  return true;
}

void AudioOutputProxy::Start(AudioSourceCallback* callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // We need to support both states since the callback may not handle OnError()
  // immediately (or at all).  It's also possible for subsequent StartStream()
  // calls to succeed after failing, so we allow it to be called again.
  DCHECK(state_ == kOpened || state_ == kStartError);

  if (!dispatcher_ || !dispatcher_->StartStream(callback, this)) {
    state_ = kStartError;
    callback->OnError(AudioSourceCallback::ErrorType::kUnknown);
    return;
  }
  state_ = kPlaying;
}

void AudioOutputProxy::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ != kPlaying)
    return;

  if (dispatcher_)
    dispatcher_->StopStream(this);
  state_ = kOpened;
}

void AudioOutputProxy::SetVolume(double volume) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  volume_ = volume;

  if (dispatcher_)
    dispatcher_->StreamVolumeSet(this, volume);
}

void AudioOutputProxy::GetVolume(double* volume) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  *volume = volume_;
}

void AudioOutputProxy::Close() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(state_ == kCreated || state_ == kOpenError || state_ == kOpened ||
         state_ == kStartError);

  // kStartError means OpenStream() succeeded and the stream must be closed
  // before destruction.
  if (state_ != kCreated && state_ != kOpenError && dispatcher_)
    dispatcher_->CloseStream(this);

  state_ = kClosed;

  // Delete the object now like is done in the Close() implementation of
  // physical stream objects.  If we delete the object via DeleteSoon, we
  // unnecessarily complicate the Shutdown procedure of the
  // dispatcher+audio manager.
  delete this;
}

void AudioOutputProxy::Flush() {
  DCHECK(state_ != kPlaying);

  if (dispatcher_)
    dispatcher_->FlushStream(this);
}

}  // namespace media
