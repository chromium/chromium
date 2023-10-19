// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_STATELESS_DEVICE_H_
#define MEDIA_GPU_V4L2_STATELESS_DEVICE_H_

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/files/scoped_file.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "media/base/video_codecs.h"
#include "media/gpu/media_gpu_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/size.h"

namespace media {

enum class BufferType { kCompressedData, kRawFrames, kInvalid };
enum class MemoryType { kMemoryMapped, kDmaBuf, kInvalid };

// Abstraction to hold compressed and uncompressed buffers. This class
// provides a structure that does not include and V4L2 references. This is done
// so that users of the driver do not need to care about V4L2 specific
// structures
class Buffer {
 public:
  Buffer(BufferType buffer_type,
         MemoryType memory_type,
         uint32_t index,
         uint32_t plane_count);
  ~Buffer();
  Buffer(const Buffer&);

  void SetupPlane(uint32_t plane, size_t offset, size_t size);
  uint32_t PlaneCount() const { return planes_.size(); }

  void* MappedAddress(uint32_t plane) const;
  void SetMappedAddress(uint32_t plane, void* address);

  uint32_t PlaneMemOffset(uint32_t plane) const {
    return planes_[plane].mem_offset;
  }
  size_t PlaneLength(uint32_t plane) const { return planes_[plane].length; }
  size_t PlaneBytesUsed(uint32_t plane) const {
    return planes_[plane].bytes_used;
  }

  uint32_t GetIndex() const { return index_; }
  BufferType GetBufferType() const { return buffer_type_; }
  MemoryType GetMemoryType() const { return memory_type_; }

  // Method for copying compressed input data into a Buffer's backing store. It
  // is limited to destination buffers that have a single plane and are memory
  // mapped.
  bool CopyDataIn(const void* data, size_t length);

 private:
  class Plane {
   public:
    size_t length;
    raw_ptr<void> mapped_address;
    size_t bytes_used;
    uint32_t mem_offset;
  };

  const BufferType buffer_type_;
  const MemoryType memory_type_;
  const uint32_t index_;
  std::vector<Plane> planes_;
};

// Encapsulates the v4l2 subsystem and prevents <linux/videodev2.h> from
// being included elsewhere with the possible exception of the codec specific
// delegates. This keeps all of the v4l2 driver specific structures in one
// place.
class MEDIA_GPU_EXPORT Device : public base::RefCountedThreadSafe<Device> {
 public:
  Device();
  virtual bool Open() = 0;
  void Close();

  // Walks through the list of formats returned by the VIDIOC_ENUM_FMT ioctl.
  // These are all of the compressed formats that the driver will accept.
  std::set<VideoCodec> EnumerateInputFormats();

  // Configures the driver to the requested |codec|, |resolution|, and
  // |encoded_buffer_size| using the VIDIOC_S_FMT ioctl.
  bool SetInputFormat(VideoCodec codec,
                      gfx::Size resolution,
                      size_t encoded_buffer_size);

  // Stops streaming on the |type| of buffer using the VIDIOC_STREAMOFF ioctl.
  bool StreamOff(BufferType type);

  // Starts streaming on the |type| of buffer using the VIDIOC_STREAMON ioctl.
  bool StreamOn(BufferType type);

  // Request a |count| of buffers via the VIDIOC_REQBUFS ioctl. The driver
  // will return a |uint32_t| with the number of buffers allocated. This
  // number does not need to be the same as |count|.
  absl::optional<uint32_t> RequestBuffers(BufferType type,
                                          MemoryType memory,
                                          size_t count);

  // Uses the VIDIOC_QUERYBUF ioctl to fill out and return a |Buffer|.
  absl::optional<Buffer> QueryBuffer(BufferType type,
                                     MemoryType memory,
                                     uint32_t index,
                                     uint32_t num_planes);

  // Query the driver for the smallest and largest uncompressed frame sizes that
  // are supported using the VIDIOC_ENUM_FRAMESIZES ioctl.
  std::pair<gfx::Size, gfx::Size> GetFrameResolutionRange(VideoCodec codec);

  // Uses the VIDIOC_QUERYCTRL and VIDIOC_QUERYMENU ioctls to list the
  // profiles of the input formats.
  std::vector<VideoCodecProfile> ProfilesForVideoCodec(VideoCodec codec);

  // mmap the |buffer| so that it can be read/written.
  bool MmapBuffer(Buffer& buffer);

  // unmmap the |buffer| when read/write access is no longer needed.
  void MunmapBuffer(Buffer& buffer);

  // Capabilities are queried using VIDIOC_QUERYCAP. Stateless and
  // stateful drivers need different capabilities.
  virtual bool CheckCapabilities(VideoCodec codec) = 0;

 private:
  friend class base::RefCountedThreadSafe<Device>;

  // Stateless and stateful drivers have different fourcc values for
  // the same codec to designate stateful vs stateless.
  virtual uint32_t VideoCodecToV4L2PixFmt(VideoCodec codec) = 0;
  virtual std::string DevicePath() = 0;

  // The actual device fd.
  base::ScopedFD device_fd_;

 protected:
  virtual ~Device();
  int Ioctl(const base::ScopedFD& fd, uint64_t request, void* arg);
  int IoctlDevice(uint64_t request, void* arg);
  bool OpenDevice();
};

}  //  namespace media

#endif  // MEDIA_GPU_V4L2_STATELESS_DEVICE_H_
