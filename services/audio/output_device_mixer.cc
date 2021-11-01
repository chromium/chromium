// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/output_device_mixer.h"

#include "base/notreached.h"

namespace audio {

// static
std::unique_ptr<OutputDeviceMixer> OutputDeviceMixer::Create(
    const std::string& device_id,
    const media::AudioParameters& output_params,
    CreateStreamCallback create_stream_callback,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  NOTIMPLEMENTED();
  return nullptr;
}

OutputDeviceMixer::OutputDeviceMixer(const std::string& device_id)
    : device_id_(device_id) {}

}  // namespace audio
