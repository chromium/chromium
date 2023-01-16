// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/android_image_backing.h"

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "ui/gl/gl_utils.h"

namespace gpu {

AndroidImageBacking::AndroidImageBacking(const Mailbox& mailbox,
                                         viz::SharedImageFormat format,
                                         const gfx::Size& size,
                                         const gfx::ColorSpace& color_space,
                                         GrSurfaceOrigin surface_origin,
                                         SkAlphaType alpha_type,
                                         uint32_t usage,
                                         size_t estimated_size,
                                         bool is_thread_safe,
                                         base::ScopedFD initial_upload_fd)
    : ClearTrackingSharedImageBacking(mailbox,
                                      format,
                                      size,
                                      color_space,
                                      surface_origin,
                                      alpha_type,
                                      usage,
                                      estimated_size,
                                      is_thread_safe),
      write_sync_fd_(std::move(initial_upload_fd)) {}

AndroidImageBacking::~AndroidImageBacking() = default;

bool AndroidImageBacking::BeginWrite(base::ScopedFD* fd_to_wait_on) {
  AutoLock auto_lock(this);

  if (is_writing_ || !active_readers_.empty() || is_overlay_accessing_) {
    LOG(ERROR) << "BeginWrite should only be called when there are no other "
                  "readers or writers";
    return false;
  }

  is_writing_ = true;
  (*fd_to_wait_on) =
      gl::MergeFDs(std::move(read_sync_fd_), std::move(write_sync_fd_));

  return true;
}

void AndroidImageBacking::EndWrite(base::ScopedFD end_write_fd) {
  AutoLock auto_lock(this);

  if (!is_writing_) {
    LOG(ERROR) << "Attempt to end write to a SharedImageBacking without a "
                  "successful begin write";
    return;
  }

  is_writing_ = false;

  write_sync_fd_ = std::move(end_write_fd);
}

bool AndroidImageBacking::BeginRead(const SharedImageRepresentation* reader,
                                    base::ScopedFD* fd_to_wait_on) {
  AutoLock auto_lock(this);

  if (is_writing_) {
    LOG(ERROR) << "BeginRead should only be called when there are no writers";
    return false;
  }

  if (active_readers_.contains(reader)) {
    LOG(ERROR) << "BeginRead was called twice on the same representation";
    return false;
  }

  active_readers_.insert(reader);
  if (write_sync_fd_.is_valid()) {
    (*fd_to_wait_on) = base::ScopedFD(HANDLE_EINTR(dup(write_sync_fd_.get())));
  } else {
    (*fd_to_wait_on) = base::ScopedFD{};
  }

  return true;
}

void AndroidImageBacking::EndRead(const SharedImageRepresentation* reader,
                                  base::ScopedFD end_read_fd) {
  AutoLock auto_lock(this);

  if (!active_readers_.contains(reader)) {
    LOG(ERROR) << "Attempt to end read to a SharedImageBacking without a "
                  "successful begin read";
    return;
  }

  active_readers_.erase(reader);

  read_sync_fd_ =
      gl::MergeFDs(std::move(read_sync_fd_), std::move(end_read_fd));
}

base::ScopedFD AndroidImageBacking::TakeReadFence() {
  AutoLock auto_lock(this);

  return std::move(read_sync_fd_);
}

}  // namespace gpu
