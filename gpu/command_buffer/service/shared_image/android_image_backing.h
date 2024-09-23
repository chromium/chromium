// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_ANDROID_IMAGE_BACKING_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_ANDROID_IMAGE_BACKING_H_

#include "base/containers/flat_set.h"
#include "base/files/scoped_file.h"
#include "base/memory/raw_ptr.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"

namespace gpu {

class AndroidImageBacking : public ClearTrackingSharedImageBacking {
 public:
  AndroidImageBacking(const Mailbox& mailbox,
                      viz::SharedImageFormat format,
                      const gfx::Size& size,
                      const gfx::ColorSpace& color_space,
                      GrSurfaceOrigin surface_origin,
                      SkAlphaType alpha_type,
                      gpu::SharedImageUsageSet usage,
                      std::string debug_label,
                      size_t estimated_size,
                      bool is_thread_safe,
                      base::ScopedFD initial_upload_fd);

  ~AndroidImageBacking() override;
  AndroidImageBacking(const AndroidImageBacking&) = delete;
  AndroidImageBacking& operator=(const AndroidImageBacking&) = delete;

  virtual bool BeginWrite(base::ScopedFD* fd_to_wait_on);
  virtual void EndWrite(base::ScopedFD end_write_fd);
  virtual bool BeginRead(const SharedImageRepresentation* reader,
                         base::ScopedFD* fd_to_wait_on);
  virtual void EndRead(const SharedImageRepresentation* reader,
                       base::ScopedFD end_read_fd);
  base::ScopedFD TakeReadFence();

 protected:
  bool allow_concurrent_read_write() const {
    return usage().Has(SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE);
  }

  // All reads and writes must wait for exiting writes to complete.
  base::ScopedFD write_sync_fd_ GUARDED_BY(lock_);
  bool is_writing_ GUARDED_BY(lock_) = false;

  // All writes must wait for existing reads to complete.
  base::ScopedFD read_sync_fd_ GUARDED_BY(lock_);
  base::flat_set<raw_ptr<const SharedImageRepresentation, CtnExperimental>>
      active_readers_ GUARDED_BY(lock_);

  bool is_overlay_accessing_ GUARDED_BY(lock_) = false;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_ANDROID_IMAGE_BACKING_H_
