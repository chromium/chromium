// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_TEST_V4L2_IOCTL_SHIM_H_
#define MEDIA_GPU_V4L2_TEST_V4L2_IOCTL_SHIM_H_

#include <linux/media/vp9-ctrls.h>
#include <linux/videodev2.h>

#include "base/files/memory_mapped_file.h"
#include "base/memory/ref_counted.h"
#include "media/filters/vp9_parser.h"

namespace media {

namespace v4l2_test {

// MmapedBuffer maintains |mmaped_planes_| for each buffer as well as
// |reference_id_|. Reference ID is computed from buffer ID, which is an
// index used for VIDIOC_REQBUFS ioctl call. Reference ID is needed to use
// previously decoded frames from reference frames list.
class MmapedBuffer : public base::RefCounted<MmapedBuffer> {
 public:
  MmapedBuffer(const base::PlatformFile decode_fd,
               const struct v4l2_buffer& v4l2_buffer);
  ~MmapedBuffer();

  class MmapedPlane {
   public:
    void* start_addr;
    size_t length;

    MmapedPlane(void* start, size_t len) {
      start_addr = start;
      length = len;
    }
  };

  using MmapedPlanes = std::vector<MmapedPlane>;

  MmapedPlanes mmaped_planes() const { return mmaped_planes_; }

  uint64_t reference_id() const { return reference_id_; }
  void set_reference_id(uint64_t reference_id) { reference_id_ = reference_id; }

 private:
  friend class base::RefCounted<MmapedBuffer>;

  MmapedBuffer(const MmapedBuffer&) = delete;
  MmapedBuffer& operator=(const MmapedBuffer&) = delete;

  MmapedPlanes mmaped_planes_;
  const uint32_t num_planes_;
  uint64_t reference_id_;
};

using MmapedBuffers = std::vector<scoped_refptr<MmapedBuffer>>;

// V4L2Queue class maintains properties of a queue.
class V4L2Queue {
 public:
  V4L2Queue(enum v4l2_buf_type type,
            uint32_t fourcc,
            const gfx::Size& size,
            uint32_t num_planes,
            enum v4l2_memory memory,
            uint32_t num_buffers);

  V4L2Queue(const V4L2Queue&) = delete;
  V4L2Queue& operator=(const V4L2Queue&) = delete;
  ~V4L2Queue();

  // Retrieves a mmaped buffer for the given |index|, which is a decoded
  // surface, from MmapedBuffers.
  scoped_refptr<MmapedBuffer> GetBuffer(const size_t index) const;

  enum v4l2_buf_type type() const { return type_; }
  uint32_t fourcc() const { return fourcc_; }
  gfx::Size display_size() const { return display_size_; }
  enum v4l2_memory memory() const { return memory_; }

  void set_buffers(MmapedBuffers& buffers) { buffers_ = buffers; }

  uint32_t num_buffers() const { return num_buffers_; }
  void set_num_buffers(uint32_t num_buffers) { num_buffers_ = num_buffers; }

  gfx::Size coded_size() const { return coded_size_; }
  void set_coded_size(gfx::Size coded_size) { coded_size_ = coded_size; }

  uint32_t num_planes() const { return num_planes_; }
  void set_num_planes(uint32_t num_planes) { num_planes_ = num_planes; }

  int media_request_fd() const { return media_request_fd_; }
  void set_media_request_fd(int media_request_fd) {
    media_request_fd_ = media_request_fd;
  }

 private:
  const enum v4l2_buf_type type_;
  const uint32_t fourcc_;
  MmapedBuffers buffers_;
  uint32_t num_buffers_;
  // The size of the image on the screen.
  const gfx::Size display_size_;
  // The size of the encoded frame. Usually has an alignment of 16, 32
  // depending on codec.
  gfx::Size coded_size_;
  uint32_t num_planes_;
  const enum v4l2_memory memory_;
  // File descriptor returned by MEDIA_IOC_REQUEST_ALLOC ioctl call
  // to submit requests.
  int media_request_fd_;
};

// V4L2IoctlShim is a shallow wrapper which wraps V4L2 ioctl requests
// with error checking and maintains the lifetime of a file descriptor
// for decode/media device.
// https://www.kernel.org/doc/html/v5.10/userspace-api/media/v4l/user-func.html
class V4L2IoctlShim {
 public:
  V4L2IoctlShim();
  V4L2IoctlShim(const V4L2IoctlShim&) = delete;
  V4L2IoctlShim& operator=(const V4L2IoctlShim&) = delete;
  ~V4L2IoctlShim();

  // Enumerates all frame sizes that the device supports
  // via VIDIOC_ENUM_FRAMESIZES.
  bool EnumFrameSizes(uint32_t fourcc) const WARN_UNUSED_RESULT;

  // Configures the underlying V4L2 queue via VIDIOC_S_FMT. Returns true
  // if the configuration was successful.
  bool SetFmt(const std::unique_ptr<V4L2Queue>& queue) const WARN_UNUSED_RESULT;

  // Retrieves the format of |queue| (via VIDIOC_G_FMT) and returns true if
  // successful, filling in |coded_size| and |num_planes| in that case.
  bool GetFmt(const enum v4l2_buf_type type,
              gfx::Size* coded_size,
              uint32_t* num_planes) const WARN_UNUSED_RESULT;

  // Tries to configure |queue|. This does not modify the underlying
  // driver state.
  // https://www.kernel.org/doc/html/v5.10/userspace-api/media/v4l/vidioc-g-fmt.html?highlight=vidioc_try_fmt#description
  bool TryFmt(const std::unique_ptr<V4L2Queue>& queue) const WARN_UNUSED_RESULT;

  // Allocates buffers via VIDIOC_REQBUFS for |queue|.
  bool ReqBufs(std::unique_ptr<V4L2Queue>& queue) const WARN_UNUSED_RESULT;

  // Enqueues an empty (capturing) or filled (output) buffer
  // in the driver's incoming |queue|.
  bool QBuf(const std::unique_ptr<V4L2Queue>& queue,
            const uint32_t index) const WARN_UNUSED_RESULT;

  // Dequeues a filled (capturing) or decoded (output) buffer
  // from the driver’s outgoing |queue|.
  bool DQBuf(const std::unique_ptr<V4L2Queue>& queue,
             uint32_t* index) const WARN_UNUSED_RESULT;

  // Starts streaming |queue| (via VIDIOC_STREAMON).
  bool StreamOn(const enum v4l2_buf_type type) const WARN_UNUSED_RESULT;

  // Sets the value of a control which specifies VP9 decoding parameters
  // for each frame.
  bool SetExtCtrls(const std::unique_ptr<V4L2Queue>& queue,
                   v4l2_ctrl_vp9_frame_decode_params& frame_params) const
      WARN_UNUSED_RESULT;

  // Allocates requests (likely one per OUTPUT buffer) via
  // MEDIA_IOC_REQUEST_ALLOC on the media device.
  bool MediaIocRequestAlloc(int* req_fd) const WARN_UNUSED_RESULT;

  // Submits a request for the given OUTPUT |queue| by queueing
  // the request with |queue|'s media_request_fd().
  bool MediaRequestIocQueue(const std::unique_ptr<V4L2Queue>& queue) const
      WARN_UNUSED_RESULT;

  // Re-initializes the previously allocated request for reuse.
  bool MediaRequestIocReinit(const std::unique_ptr<V4L2Queue>& queue) const
      WARN_UNUSED_RESULT;

  // Verifies |v4l_fd| supports |compressed_format| for OUTPUT queues
  // and |uncompressed_format| for CAPTURE queues, respectively.
  bool VerifyCapabilities(uint32_t compressed_format,
                          uint32_t uncompressed_format) const
      WARN_UNUSED_RESULT;

  // Allocates buffers for the given |queue|.
  bool QueryAndMmapQueueBuffers(std::unique_ptr<V4L2Queue>& queue) const
      WARN_UNUSED_RESULT;

 private:
  // Queries |v4l_fd| to see if it can use the specified |fourcc| format
  // for the given buffer |type|.
  bool QueryFormat(enum v4l2_buf_type type,
                   uint32_t fourcc) const WARN_UNUSED_RESULT;

  // Uses a specialized function template to execute V4L2 ioctl request
  // for |request_code| and returns the output of the ioctl() in |arg|
  // if this is a pointer, otherwise |arg| is considered a file descriptor
  // for said ioctl().
  template <typename T>
  bool Ioctl(int request_code, T arg) const WARN_UNUSED_RESULT;

  // Decode device file descriptor used for ioctl requests.
  const base::File decode_fd_;
  // Media device file descriptor used for ioctl requests.
  const base::File media_fd_;
};

}  // namespace v4l2_test
}  // namespace media

#endif  // MEDIA_GPU_V4L2_TEST_V4L2_IOCTL_SHIM_H_