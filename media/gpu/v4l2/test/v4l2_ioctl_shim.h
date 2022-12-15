// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_TEST_V4L2_IOCTL_SHIM_H_
#define MEDIA_GPU_V4L2_TEST_V4L2_IOCTL_SHIM_H_

#include <linux/videodev2.h>
#include <string.h>

#include <set>

#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "ui/gfx/geometry/size.h"

namespace media {

namespace v4l2_test {

// MmapedBuffer maintains |mmaped_planes_| for each buffer as well as
// |buffer_id_|. |buffer_id_| is an index used for VIDIOC_REQBUFS ioctl call.
class MmapedBuffer : public base::RefCounted<MmapedBuffer> {
 public:
  MmapedBuffer(const base::PlatformFile decode_fd,
               const struct v4l2_buffer& v4l2_buffer);

  class MmapedPlane {
   public:
    void* start_addr;
    const size_t length;
    size_t bytes_used;

    MmapedPlane(void* start, size_t len) : start_addr(start), length(len) {}

    void CopyIn(const uint8_t* frame_data, size_t frame_size) {
      LOG_ASSERT(frame_size < length)
          << "Not enough memory allocated to copy into.";
      bytes_used = frame_size;
      memcpy(static_cast<uint8_t*>(start_addr), frame_data, frame_size);
    }
  };

  using MmapedPlanes = std::vector<MmapedPlane>;

  MmapedPlanes& mmaped_planes() { return mmaped_planes_; }

  uint32_t buffer_id() const { return buffer_id_; }
  void set_buffer_id(uint32_t buffer_id) { buffer_id_ = buffer_id; }

  uint32_t frame_number() const { return frame_number_; }
  void set_frame_number(uint32_t frame_number) { frame_number_ = frame_number; }

 private:
  friend class base::RefCounted<MmapedBuffer>;

  ~MmapedBuffer();
  MmapedBuffer(const MmapedBuffer&) = delete;
  MmapedBuffer& operator=(const MmapedBuffer&) = delete;

  MmapedPlanes mmaped_planes_;
  const uint32_t num_planes_;
  // TODO(stevecho): might be better to use constructor for default value
  uint32_t buffer_id_ = 0;
  // Indicates which frame in input bitstream corresponds to this MmapedBuffer
  // in OUTPUT queue.
  // TODO(stevecho): might need to consider |show_frame| flag in the frame
  // header.
  uint32_t frame_number_;
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
  void set_display_size(gfx::Size display_size) {
    display_size_ = display_size;
  }

  enum v4l2_memory memory() const { return memory_; }

  void set_buffers(MmapedBuffers& buffers) { buffers_ = buffers; }

  uint32_t num_buffers() const { return num_buffers_; }
  void set_num_buffers(uint32_t num_buffers) { num_buffers_ = num_buffers; }

  gfx::Size coded_size() const { return coded_size_; }
  void set_coded_size(gfx::Size coded_size) { coded_size_ = coded_size; }

  uint32_t num_planes() const { return num_planes_; }
  void set_num_planes(uint32_t num_planes) { num_planes_ = num_planes; }

  // TODO(stevecho): change naming from |last_queued_buffer_index| to
  // |last_queued_buffer_id|
  uint32_t last_queued_buffer_index() const {
    return last_queued_buffer_index_;
  }
  void set_last_queued_buffer_index(uint32_t last_queued_buffer_index) {
    last_queued_buffer_index_ = last_queued_buffer_index;
  }

  int media_request_fd() const { return media_request_fd_; }
  void set_media_request_fd(int media_request_fd) {
    media_request_fd_ = media_request_fd;
  }

  std::set<uint32_t> queued_buffer_indexes() const {
    return queued_buffer_indexes_;
  }

  void QueueBufferIndex(uint32_t last_queued_buffer_index) {
    queued_buffer_indexes_.insert(last_queued_buffer_index);
  }

  void DequeueBufferIndex(uint32_t index) {
    queued_buffer_indexes_.erase(index);
  }

  void DequeueAllBufferIndexes() { queued_buffer_indexes_.clear(); }

 private:
  const enum v4l2_buf_type type_;
  const uint32_t fourcc_;
  MmapedBuffers buffers_;
  uint32_t num_buffers_;
  // The size of the image on the screen.
  gfx::Size display_size_;
  // The size of the encoded frame. Usually has an alignment of 16, 32
  // depending on codec.
  gfx::Size coded_size_;
  uint32_t num_planes_;
  const enum v4l2_memory memory_;
  // File descriptor returned by MEDIA_IOC_REQUEST_ALLOC ioctl call
  // to submit requests.
  int media_request_fd_;
  // Tracks which CAPTURE buffer was queued in the previous frame.
  uint32_t last_queued_buffer_index_;
  std::set<uint32_t> queued_buffer_indexes_;
};

// V4L2IoctlShim is a shallow wrapper which wraps V4L2 ioctl requests
// with error checking and maintains the lifetime of a file descriptor
// for decode/media device.
// https://www.kernel.org/doc/html/v5.10/userspace-api/media/v4l/user-func.html
class V4L2IoctlShim {
 public:
  // Finds first decoder that can decode |coded_fourcc|
  V4L2IoctlShim(uint32_t coded_fourcc);
  V4L2IoctlShim(const V4L2IoctlShim&) = delete;
  V4L2IoctlShim& operator=(const V4L2IoctlShim&) = delete;
  ~V4L2IoctlShim();

  // Queries whether the given |ctrl_id| is supported on current platform.
  [[nodiscard]] bool QueryCtrl(const uint32_t ctrl_id) const;

  // Enumerates all frame sizes that the device supports
  // via VIDIOC_ENUM_FRAMESIZES.
  [[nodiscard]] bool EnumFrameSizes(uint32_t fourcc) const;

  // Configures the underlying V4L2 queue via VIDIOC_S_FMT. Returns true
  // if the configuration was successful.
  [[nodiscard]] bool SetFmt(const std::unique_ptr<V4L2Queue>& queue) const;

  // Retrieves the format of |queue| (via VIDIOC_G_FMT) and returns true if
  // successful, filling in |coded_size| and |num_planes| in that case.
  [[nodiscard]] bool GetFmt(const enum v4l2_buf_type type,
                            gfx::Size* coded_size,
                            uint32_t* num_planes) const;

  // Tries to configure |queue|. This does not modify the underlying
  // driver state.
  // https://www.kernel.org/doc/html/v5.10/userspace-api/media/v4l/vidioc-g-fmt.html?highlight=vidioc_try_fmt#description
  [[nodiscard]] bool TryFmt(const std::unique_ptr<V4L2Queue>& queue) const;

  // Allocates buffers via VIDIOC_REQBUFS for |queue|.
  [[nodiscard]] bool ReqBufs(std::unique_ptr<V4L2Queue>& queue) const;

  // Allocates buffers via VIDIOC_REQBUFS for |queue| with a buffer count.
  [[nodiscard]] bool ReqBufsWithCount(std::unique_ptr<V4L2Queue>& queue,
                                      uint32_t count) const;

  // Enqueues an empty (capturing) or filled (output) buffer
  // in the driver's incoming |queue|.
  [[nodiscard]] bool QBuf(const std::unique_ptr<V4L2Queue>& queue,
                          const uint32_t index) const;

  // Dequeues a filled (capturing) or decoded (output) buffer
  // from the driver’s outgoing |queue|.
  [[nodiscard]] bool DQBuf(const std::unique_ptr<V4L2Queue>& queue,
                           uint32_t* index) const;

  // Starts streaming |queue| (via VIDIOC_STREAMON).
  [[nodiscard]] bool StreamOn(const enum v4l2_buf_type type) const;

  // Stops streaming |queue| (via VIDIOC_STREAMOFF).
  [[nodiscard]] bool StreamOff(const enum v4l2_buf_type type) const;

  // Sets the value of controls which specify decoding parameters
  // for each frame.
  [[nodiscard]] bool SetExtCtrls(const std::unique_ptr<V4L2Queue>& queue,
                                 v4l2_ext_controls* ext_ctrls) const;

  // Allocates requests (likely one per OUTPUT buffer) via
  // MEDIA_IOC_REQUEST_ALLOC on the media device.
  [[nodiscard]] bool MediaIocRequestAlloc(int* req_fd) const;

  // Submits a request for the given OUTPUT |queue| by queueing
  // the request with |queue|'s media_request_fd().
  [[nodiscard]] bool MediaRequestIocQueue(
      const std::unique_ptr<V4L2Queue>& queue) const;

  // Re-initializes the previously allocated request for reuse.
  [[nodiscard]] bool MediaRequestIocReinit(
      const std::unique_ptr<V4L2Queue>& queue) const;

  // Verifies |v4l_fd| supports |compressed_format| for OUTPUT queues
  // and |uncompressed_format| for CAPTURE queues, respectively.
  [[nodiscard]] bool VerifyCapabilities(uint32_t compressed_format,
                                        uint32_t uncompressed_format) const;

  // Allocates buffers for the given |queue|.
  [[nodiscard]] bool QueryAndMmapQueueBuffers(
      std::unique_ptr<V4L2Queue>& queue) const;

  enum class DeviceType {
    kDecoder,
    kMedia,
  };

 private:
  // Queries |v4l_fd| to see if it can use the specified |fourcc| format
  // for the given buffer |type|.
  [[nodiscard]] bool QueryFormat(enum v4l2_buf_type type,
                                 uint32_t fourcc) const;

  // Uses a specialized function template to execute V4L2 ioctl request
  // for |request_code| and returns the output of the ioctl() in |arg|
  // if this is a pointer, otherwise |arg| is considered a file descriptor
  // for said ioctl().
  template <typename T>
  [[nodiscard]] bool Ioctl(int request_code, T arg) const;

  // Decode device file descriptor used for ioctl requests.
  base::File decode_fd_;
  // Media device file descriptor used for ioctl requests.
  base::File media_fd_;
};

}  // namespace v4l2_test
}  // namespace media

#endif  // MEDIA_GPU_V4L2_TEST_V4L2_IOCTL_SHIM_H_
