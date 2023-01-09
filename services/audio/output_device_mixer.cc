// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/output_device_mixer.h"

#include "base/task/single_thread_task_runner.h"
#include "services/audio/mixing_graph.h"
#include "services/audio/output_device_mixer_impl.h"

namespace audio {

// static
std::unique_ptr<OutputDeviceMixer> OutputDeviceMixer::Create(
    const std::string& device_id,
    const media::AudioParameters& output_params,
    CreateStreamCallback create_stream_callback,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  return std::make_unique<OutputDeviceMixerImpl>(
      device_id, output_params, base::BindOnce(&MixingGraph::Create),
      std::move(create_stream_callback));
}

OutputDeviceMixer::OutputDeviceMixer(const std::string& device_id)
    : device_id_(device_id) {}

}  // namespace audio
