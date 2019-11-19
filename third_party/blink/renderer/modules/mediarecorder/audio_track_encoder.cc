// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediarecorder/audio_track_encoder.h"

namespace blink {

AudioTrackEncoder::AudioTrackEncoder(OnEncodedAudioCB on_encoded_audio_cb)
    : paused_(false), on_encoded_audio_cb_(std::move(on_encoded_audio_cb)) {
  // AudioTrackEncoder is constructed on the thread that ATR lives on, but
  // should operate only on the encoder thread after that. Reset
  // |encoder_thread_checker_| here, as the next call to CalledOnValidThread()
  // will be from the encoder thread.
  DETACH_FROM_THREAD(encoder_thread_checker_);
}

AudioTrackEncoder::~AudioTrackEncoder() {}

}  // namespace blink
