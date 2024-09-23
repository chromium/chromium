// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CHROMEOS_FRAME_RESOURCE_H_
#define MEDIA_GPU_CHROMEOS_FRAME_RESOURCE_H_

#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_layout.h"
#include "media/base/video_frame_metadata.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/linux/native_pixmap_dmabuf.h"

namespace media {

// Forward declare for use in AsVideoFrameResource.
class VideoFrameResource;

// Forward declare for use in AsNativePixmapFrameResource
class NativePixmapFrameResource;

// Base class for holding an object like a VideoFrame. It provides accessors for
// the metadata or data. This can be implemented using different backing types
// e.g. VideoFrame or NativePixmap.
class FrameResource : public base::RefCountedThreadSafe<FrameResource> {
 public:
  FrameResource();
  // FrameResource is not moveable or copyable.
  FrameResource(const FrameResource&) = delete;
  FrameResource& operator=(const FrameResource&) = delete;

  // Allows safe downcasting to an VideoFrameResource, which has a VideoFrame
  // accessor that is needed by encoders. The returned pointer is only valid as
  // long as |this| is alive.
  virtual VideoFrameResource* AsVideoFrameResource();

  // Allows safe downcasting to a NativePixmapFrameResource. The returned
  // pointer is only valid as long as |this| is alive.
  virtual const NativePixmapFrameResource* AsNativePixmapFrameResource() const;

  // Unique identifier for this video frame generated at construction time. The
  // first ID is 1. The identifier is unique within a process % overflows (which
  // should be impossible in practice with a 64-bit unsigned integer).
  //
  // Note: callers may assume that ID will always correspond to a base::IdType
  // but should not blindly assume that the underlying type will always be
  // uint64_t (this is free to change in the future).
  using ID = ::base::IdTypeU64<class FrameResourceIdTag>;
  ID unique_id() const;

  // If the instance IsMappable(), then frame data can be accessed by using
  // data(), writable_data(), visible_data(), and GetWritableVisibleData()
  // accessors. The memory is owned by the FrameResource object and must not be
  // freed by the caller.
  virtual bool IsMappable() const = 0;

  virtual const uint8_t* data(size_t plane) const = 0;

  // For writable variants, |storage_type()| cannot be
  // VideoFrame::STORAGE_SHMEM.
  virtual uint8_t* writable_data(size_t plane) = 0;

  // The visible variants return a pointer that is offsetted into the
  // plane buffer specified by visible_rect().origin().
  virtual const uint8_t* visible_data(size_t plane) const = 0;
  virtual uint8_t* GetWritableVisibleData(size_t plane) = 0;

  // The number of DmaBufs will be equal or less than the number of planes of
  // the frame. If there are less, this means that the last FD contains the
  // remaining planes. Should be > 0 for STORAGE_DMABUFS.
  virtual size_t NumDmabufFds() const = 0;

  // Returns true if |this| has DmaBufs.
  bool HasDmaBufs() const { return NumDmabufFds() > 0; }

  // The returned FDs are still owned by |this|. This means that the caller
  // shall not close them, or use them after the FrameResource is destroyed. For
  // such use cases, use dup() to obtain your own copy of the FDs.
  virtual int GetDmabufFd(size_t i) const = 0;

  // Creates or gets a NativePixmap. If the FrameResource is backed by a
  // NativePixmap, then there is no duplication of file descriptors. The
  // returned pixmap is only a DmaBuf container and should not be used for
  // compositing or scanout.
  virtual scoped_refptr<const gfx::NativePixmapDmaBuf> GetNativePixmapDmaBuf()
      const = 0;

  // Create a shared GPU memory handle to |this|'s data.
  virtual gfx::GpuMemoryBufferHandle CreateGpuMemoryBufferHandle() const = 0;

  // Gets the ScopedMapping object which clients can use to access the CPU
  // visible memory and other metadata for the gpu buffer backing |this|.
  virtual std::unique_ptr<VideoFrame::ScopedMapping> MapGMBOrSharedImage()
      const = 0;

  // Returns an identifier based on the frame data's underlying storage. This
  // returns consistent results even if the frame gets wrapped. Returns an
  // invalid GenericSharedMemoryId if an identifier cannot be determined.
  virtual gfx::GenericSharedMemoryId GetSharedMemoryId() const = 0;

  virtual const VideoFrameLayout& layout() const = 0;

  virtual VideoPixelFormat format() const = 0;

  // Returns the stride in bytes of a plane. Note that stride can be negative if
  // the image layout is bottom-up.
  virtual int stride(size_t plane) const = 0;

  virtual VideoFrame::StorageType storage_type() const = 0;

  // As opposed to stride(), row_bytes() refers to the bytes representing
  // frame data scanlines (coded_size.width() pixels, without stride padding).
  virtual int row_bytes(size_t plane) const = 0;

  // The full dimensions of the video frame data. This might be larger than
  // |this|'s visible_area() size due to macroblock alignment or memory
  // allocation restrictions.
  virtual const gfx::Size& coded_size() const = 0;

  // A subsection of [0, 0, coded_size().width(), coded_size.height()]. This
  // can be set to "soft-apply" a cropping. It determines the pointers into
  // the data returned by visible_data().
  virtual const gfx::Rect& visible_rect() const = 0;

  // Specifies that the visible_rect() section of the frame is supposed to be
  // scaled to this size when being presented. This can be used to represent
  // anamorphic frames, or to "soft-apply" any custom scaling.
  virtual const gfx::Size& natural_size() const = 0;

  // Returns a dictionary of optional metadata. This contains information
  // associated with the frame that downstream clients might use for frame-level
  // logging, quality/performance optimizations, signaling, etc.
  virtual const VideoFrameMetadata& metadata() const = 0;
  virtual VideoFrameMetadata& metadata() = 0;
  virtual void set_metadata(const VideoFrameMetadata& metadata) = 0;

  virtual base::TimeDelta timestamp() const = 0;
  virtual void set_timestamp(base::TimeDelta timestamp) = 0;

  // Returns the color space of this frame's content.
  virtual gfx::ColorSpace ColorSpace() const = 0;
  virtual void set_color_space(const gfx::ColorSpace& color_space) = 0;

  virtual const std::optional<gfx::HDRMetadata>& hdr_metadata() const = 0;
  virtual void set_hdr_metadata(
      const std::optional<gfx::HDRMetadata>& hdr_metadata) = 0;

  // Adds a callback to be run when the FrameResource is about to be destroyed.
  // The callback may be run from ANY THREAD, and so it is up to the client to
  // ensure thread safety.  Although read-only access to the members of |this|
  // is permitted while the callback executes (including VideoFrameMetadata),
  // clients should not assume the data pointers are valid.
  virtual void AddDestructionObserver(base::OnceClosure callback) = 0;

  // Returns a new frame resource that copies the metadata from |this|, but
  // refers to |this|'s frame data without copying it.
  virtual scoped_refptr<FrameResource> CreateWrappingFrame(
      const gfx::Rect& visible_rect,
      const gfx::Size& natural_size) = 0;

  // Overloads CreateWrappingFrame() and uses |this|'s visible_rect() and
  // natural_size() instead of accepting them as arguments.
  scoped_refptr<FrameResource> CreateWrappingFrame() {
    return CreateWrappingFrame(visible_rect(), natural_size());
  }

  // Returns a human-readable string describing |this|.
  virtual std::string AsHumanReadableString() const = 0;

  // Gets the GpuMemoryBufferHandle backing |this|.
  virtual gfx::GpuMemoryBufferHandle GetGpuMemoryBufferHandleForTesting()
      const = 0;

 protected:
  friend class base::RefCountedThreadSafe<FrameResource>;

  virtual ~FrameResource() = default;

 private:
  const ID unique_id_;
};

}  // namespace media

#endif  // MEDIA_GPU_CHROMEOS_FRAME_RESOURCE_H_
