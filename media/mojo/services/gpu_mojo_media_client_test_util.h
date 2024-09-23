// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_GPU_MOJO_MEDIA_CLIENT_TEST_UTIL_H_
#define MEDIA_MOJO_SERVICES_GPU_MOJO_MEDIA_CLIENT_TEST_UTIL_H_

#include "base/task/single_thread_task_runner.h"
#include "gpu/config/gpu_preferences.h"

namespace media {

// Uses the standard MojoMediaClient logic to determine which additional
// platform-dependent codecs are supported and registers them so that they are
// considered in the answer provided by `IsSupportedAudioType()` and
// `IsSupportedVideoType()`.
// Providing `gpu_preferences` might be necessary to get realistic results with
// respect to video codecs. For example, if
// `gpu_preferences.disable_accelerated_video_decode` is true, then
// `IsSupportedVideoType()` will return false for all video types.
void AddSupplementalCodecsForTesting(gpu::GpuPreferences gpu_preferences = {});

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_GPU_MOJO_MEDIA_CLIENT_TEST_UTIL_H_
