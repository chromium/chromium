// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PARKABLE_IMAGE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PARKABLE_IMAGE_H_

#include "base/debug/stack_trace.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/renderer/platform/disk_data_metadata.h"
#include "third_party/blink/renderer/platform/graphics/rw_buffer.h"
#include "third_party/blink/renderer/platform/image-decoders/segment_reader.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"

namespace blink {

class SegmentReader;
class ParkableImageManager;

// Wraps a RWBuffer containing encoded image data. This buffer can be written
// to/read from disk when not needed, to improve memory usage.
class PLATFORM_EXPORT ParkableImage final
    : public ThreadSafeRefCounted<ParkableImage> {
 public:
  // |initial_capacity| reserves space in the internal buffer, if you know how
  // much data you'll be appending in advance.
  explicit ParkableImage(size_t initial_capacity = 0);

  ~ParkableImage();

  ParkableImage& operator=(const ParkableImage&) = delete;
  ParkableImage(const ParkableImage&) = delete;

  // Factory method to construct a ParkableImage.
  static scoped_refptr<ParkableImage> Create(size_t initial_capacity = 0);

  // Freezes the ParkableImage. This changes the following:
  // (1) We are no longer allowed to mutate the internal buffer (e.g. via
  // Append);
  // (2) The image may now be parked to disk.
  void Freeze() LOCKS_EXCLUDED(lock_);

  // Adds data to the internal buffer of ParkableImage. Cannot be called after
  // the ParkableImage has been frozen (see Freeze()). |offset| is the offset
  // from the start of |buffer| that we want to start copying the data from.
  void Append(WTF::SharedBuffer* buffer, size_t offset = 0)
      LOCKS_EXCLUDED(lock_);

  // Make a Read-Only snapshot of the data within ParkableImage. This may be a
  // view into the internal buffer of ParkableImage, or a copy of the data. It
  // is guaranteed to be safe to read this data from another thread at any time.
  scoped_refptr<SegmentReader> MakeROSnapshot();

  // Returns the data in the internal buffer. It should not be modified after
  // the ParkableImage has been frozen.
  scoped_refptr<SharedBuffer> Data() LOCKS_EXCLUDED(lock_);

  // Returns the size of the internal buffer. Can be called even when
  // ParkableImage has been parked.
  size_t size() const;

  bool is_frozen() const { return frozen_; }
  bool is_on_disk() const EXCLUSIVE_LOCKS_REQUIRED(lock_) {
    return !rw_buffer_ && on_disk_metadata_;
  }
  bool CanParkNow() const EXCLUSIVE_LOCKS_REQUIRED(lock_);

 private:
  friend class ParkableImageManager;
  friend class ParkableImageBaseTest;

  scoped_refptr<SegmentReader> GetSegmentReader() LOCKS_EXCLUDED(lock_);

  // Attempt to park to disk. Returns false if it cannot be parked right now for
  // whatever reason, true if we will _attempt_ to park it to disk.
  bool MaybePark() LOCKS_EXCLUDED(lock_);

  // Unpark the data from disk. This is blocking, on the same thread (since we
  // cannot expect to continue with anything that needs the data until we have
  // unparked it).
  void Unpark() EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Tries to write the data from |rw_buffer_| to disk. Then, if the data is
  // successfully written to disk, posts a task to discard |rw_buffer_|.
  static void WriteToDiskInBackground(
      scoped_refptr<ParkableImage>,
      scoped_refptr<base::SingleThreadTaskRunner> callback_task_runner)
      LOCKS_EXCLUDED(lock_);

  // Attempt to discard the data. This should only be called after we've written
  // the data to disk. Fails if the image can not be parked at the time this is
  // called for whatever reason.
  void MaybeDiscardData() LOCKS_EXCLUDED(lock_);

  // Discards the data in |rw_buffer_|. Caller is responsible for making sure
  // this is only called when the image can be parked.
  void DiscardData() EXCLUSIVE_LOCKS_REQUIRED(lock_);

  mutable Mutex lock_;

  std::unique_ptr<RWBuffer> rw_buffer_ GUARDED_BY(lock_);

  // Non-null iff we have the data from |rw_buffer_| saved to disk.
  std::unique_ptr<DiskDataMetadata> on_disk_metadata_ GUARDED_BY(lock_) =
      nullptr;
  // |size_| is only modified on the main thread.
  size_t size_ = 0;
  // |frozen_| is only modified on the main thread.
  bool frozen_ = false;
  bool background_task_in_progress_ GUARDED_BY(lock_) = false;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PARKABLE_IMAGE_H_
