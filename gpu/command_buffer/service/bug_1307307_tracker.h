// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_BUG_1307307_TRACKER_H_
#define GPU_COMMAND_BUFFER_SERVICE_BUG_1307307_TRACKER_H_

#include "base/containers/lru_cache.h"
#include "gpu/command_buffer/common/mailbox.h"

namespace gpu {
// This is helper class to debug https://crbug.com/1307307 and should be removed
// as soon as we get enough diagnosting information.
class Bug1307307Tracker {
 public:
  enum class VideoAccessError {
    kNoError,
    kSurfaceTexture_NoTextureOwner,
    kSurfaceTexture_NotGLContext,
    kSurfaceTexture_CantCreateTexture,
    kSurfaceTexture_CantCreateRepresentation,
    kImageReader_NoTextureOwner,
    kImageReader_CantCreateTexture,
    kImageReader_CantCreateRepresentation,
    kImageReader_NoAHB,
    kImageReader_CantCreateVulkanImage,
    kImageReader_VulkanReadAccessFailed
  };

  Bug1307307Tracker();
  ~Bug1307307Tracker();

  Bug1307307Tracker(const Bug1307307Tracker&) = delete;
  Bug1307307Tracker(Bug1307307Tracker&&) = delete;
  Bug1307307Tracker& operator=(const Bug1307307Tracker&) = delete;
  Bug1307307Tracker& operator=(Bug1307307Tracker&&) = delete;

  void BeforeAccess();
  void CopySubTextureFinished(const Mailbox& source,
                              const Mailbox& destination,
                              bool failed);
  void AccessFailed(const Mailbox& mailbox, bool cleared);

  // Called by SharedImageVideo, thread-safe.
  static void SetLastAccessError(VideoAccessError error);

 private:
  struct CopySubTextureResult {
    Mailbox source;
    bool ever_succeeded;
    bool failed;
    VideoAccessError video_error;
  };

  static void ClearLastAccessError();
  static VideoAccessError GetLastAccessError();

  void GenerateCrashKey(int hops, VideoAccessError video_error, bool cleared);

  base::LRUCache<Mailbox, CopySubTextureResult> copy_sub_texture_results_{50};
};

};  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_BUG_1307307_TRACKER_H_
