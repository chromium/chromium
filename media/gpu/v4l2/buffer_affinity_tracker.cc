// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/buffer_affinity_tracker.h"
#include "base/synchronization/lock.h"
#include "media/gpu/macros.h"
#include "ui/gfx/generic_shared_memory_id.h"

namespace media {

BufferAffinityTracker::BufferAffinityTracker(size_t nb_buffers) {
  resize(0);
}

BufferAffinityTracker::~BufferAffinityTracker() = default;

void BufferAffinityTracker::resize(size_t nb_buffers) {
  base::AutoLock lock(lock_);

  id_to_buffer_map_.clear();
  nb_buffers_ = nb_buffers;
  DVLOGF(4) << this << " resize: " << nb_buffers;
}

absl::optional<size_t> BufferAffinityTracker::get_buffer_for_id(
    gfx::GenericSharedMemoryId id) {
  base::AutoLock lock(lock_);

  auto it = id_to_buffer_map_.find(id);
  // If the handle is already bound to a buffer, return it.
  if (it != id_to_buffer_map_.end()) {
    DVLOGF(4) << this << " match for " << it->second;
    return it->second;
  }

  // Try to assign a new buffer for this handle...

  // No buffer available? No luck then.
  if (id_to_buffer_map_.size() == nb_buffers()) {
    DVLOGF(4) << this << " tracker is full!";
    return absl::nullopt;
  }

  const size_t v4l2_id = id_to_buffer_map_.size();
  id_to_buffer_map_.emplace(id, v4l2_id);

  DVLOGF(4) << this << " add " << v4l2_id;
  return v4l2_id;
}

}  // namespace media
