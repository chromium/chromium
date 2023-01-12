// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_GPU_CLIENT_IDS_H_
#define GPU_IPC_COMMON_GPU_CLIENT_IDS_H_

namespace gpu {

// The list of client id constants used to identify unique GPU clients. In the
// general case, GPU clients are assigned unique IDs in the browser. But these
// special constants are used for particular clients that should always be
// assigned the same ID.

// The ID used by the display compositor running in the GPU process.
constexpr int32_t kDisplayCompositorClientId = -1;

// The ID used for storing shaders created by skia in the GPU process. Note that
// this ID doesn't correspond to a real Gpu client/channel, but is required so
// we can use the same disk caching system for shaders and use a unique
// namespace for these shaders.
constexpr int32_t kGrShaderCacheClientId = -2;

inline bool IsReservedClientId(int32_t client_id) {
  return client_id < 0;
}

}  // namespace gpu

#endif  // GPU_IPC_COMMON_GPU_CLIENT_IDS_H_
