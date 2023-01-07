// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_BUFFER_AFFINITY_TRACKER_H_
#define MEDIA_GPU_V4L2_BUFFER_AFFINITY_TRACKER_H_

#include <cstddef>
#include <map>

#include "base/synchronization/lock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/generic_shared_memory_id.h"

namespace media {

/**
 * Maintains affinity between native handles and V4L2 buffers.
 *
 * Give a handle ID, `get_buffer_for_id()` will attempt to always return the
 * same V4L2 buffer ID so handles are always used with the same buffer. This
 * is both beneficial for performance, and necessary in some cases like the
 * stateful decoder.
 *
 * All the methods of this class are thread-safe.
 */
class BufferAffinityTracker {
 public:
  explicit BufferAffinityTracker(size_t nb_buffers);
  ~BufferAffinityTracker();
  size_t nb_buffers() const { return nb_buffers_; }
  // Resize this tracker and reset its state.
  void resize(size_t nb_buffers);

  /**
   * Return the V4L2 buffer index suitable for this buffer ID.
   *
   * If it is the first time this method is called with a given id, return the
   * first available buffer it can find, and memorize the association between
   * the id and the V4L2 buffer.
   *
   * On subsequent calls with the same id, that same V4L2 buffer will be
   * returned.
   */
  absl::optional<size_t> get_buffer_for_id(gfx::GenericSharedMemoryId id);

 private:
  base::Lock lock_;
  std::map<gfx::GenericSharedMemoryId, size_t> id_to_buffer_map_;
  // Maximum number of buffers we are allowed to track.
  size_t nb_buffers_;
};

}  // namespace media

#endif  // MEDIA_GPU_V4L2_BUFFER_AFFINITY_TRACKER_H_
