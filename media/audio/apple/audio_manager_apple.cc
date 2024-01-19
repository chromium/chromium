// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/apple/audio_manager_apple.h"

namespace media {

AudioManagerApple::AudioManagerApple(std::unique_ptr<AudioThread> audio_thread,
                                     AudioLogFactory* audio_log_factory)
    : AudioManagerBase(std::move(audio_thread), audio_log_factory) {}

AudioManagerApple::~AudioManagerApple() = default;

}  // namespace media
