// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/ipc/service/media_gpu_channel.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "gpu/ipc/service/command_buffer_stub.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "ipc/ipc_mojo_bootstrap.h"

namespace media {

MediaGpuChannel::MediaGpuChannel(
    gpu::GpuChannel* channel,
    const AndroidOverlayMojoFactoryCB& overlay_factory_cb)
    : channel_(channel), overlay_factory_cb_(overlay_factory_cb) {}

MediaGpuChannel::~MediaGpuChannel() = default;

}  // namespace media
