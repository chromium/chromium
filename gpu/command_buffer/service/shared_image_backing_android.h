// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_ANDROID_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_ANDROID_H_

#include "base/containers/flat_set.h"
#include "base/files/scoped_file.h"
#include "gpu/command_buffer/service/shared_image_backing.h"

namespace gpu {

class SharedImageBackingAndroid : public ClearTrackingSharedImageBacking {
 public:
  SharedImageBackingAndroid(const Mailbox& mailbox,
                            viz::ResourceFormat format,
                            const gfx::Size& size,
                            const gfx::ColorSpace& color_space,
                            GrSurfaceOrigin surface_origin,
                            SkAlphaType alpha_type,
                            uint32_t usage,
                            size_t estimated_size,
                            bool is_thread_safe,
                            base::ScopedFD initial_upload_fd);

  ~SharedImageBackingAndroid() override;

  virtual bool BeginWrite(base::ScopedFD* fd_to_wait_on);
  virtual void EndWrite(base::ScopedFD end_write_fd);
  virtual bool BeginRead(const SharedImageRepresentation* reader,
                         base::ScopedFD* fd_to_wait_on);
  virtual void EndRead(const SharedImageRepresentation* reader,
                       base::ScopedFD end_read_fd);
  base::ScopedFD TakeReadFence();

 protected:
  // All reads and writes must wait for exiting writes to complete.
  base::ScopedFD write_sync_fd_ GUARDED_BY(lock_);
  bool is_writing_ GUARDED_BY(lock_) = false;

  // All writes must wait for existing reads to complete.
  base::ScopedFD read_sync_fd_ GUARDED_BY(lock_);
  base::flat_set<const SharedImageRepresentation*> active_readers_
      GUARDED_BY(lock_);

  bool is_overlay_accessing_ GUARDED_BY(lock_) = false;

  SharedImageBackingAndroid(const SharedImageBackingAndroid&) = delete;
  SharedImageBackingAndroid& operator=(const SharedImageBackingAndroid&) =
      delete;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_ANDROID_H_
