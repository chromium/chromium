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
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "ui/gfx/geometry/size.h"

namespace media {

namespace v4l2_test {

// MmappedBuffer maintains |mmapped_planes_| for each buffer as well as
// |buffer_id_|. |buffer_id_| is an index used for VIDIOC_REQBUFS ioctl call.
class MmappedBuffer : public base::RefCounted<MmappedBuffer> {
 public:
  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();

  MmappedBuffer(const base::PlatformFile decode_fd,
                const struct v4l2_buffer& v4l2_buffer);

  class MmappedPlane {
   public:
    raw_ptr<void> start_addr;
    const size_t length;
    size_t bytes_used = 0;

    MmappedPlane(void* start, size_t len) : start_addr(start), length(len) {}

    // Appends the current slice data to the mmapped buffer. Resets |bytes_used|
    // to 0 for the first slice. This function is used for HEVC because multiple
    // slices per frame are supported.
    void CopyInSlice(const uint8_t* frame_data,
                     size_t frame_size,
                     bool is_first_slice) {
      if (is_first_slice) {
        bytes_used = 0;
      }

      LOG_ASSERT((bytes_used + frame_size) < length)
          << "Not enough memory allocated to copy into.";

      memcpy(static_cast<uint8_t*>(start_addr) + bytes_used, frame_data,
             frame_size);
      bytes_used += frame_size;
    }

    // Overwrites the mmapped buffer with the current frame data.
    void CopyIn(const uint8_t* frame_data, size_t frame_size) {
      CopyInSlice(frame_data, frame_size, true);
    }
  };

  using MmappedPlanes = std::vector<MmappedPlane>;

  MmappedPlanes& mmapped_planes() { return mmapped_planes_; }

  uint32_t buffer_id() const { return buffer_id_; }
  void set_buffer_id(uint32_t buffer_id) { buffer_id_ = buffer_id; }

  uint32_t frame_number() const { return frame_number_; }
  void set_frame_number(uint32_t frame_number) { frame_number_ = frame_number; }

 private:
  friend class base::RefCounted<MmappedBuffer>;
  ~MmappedBuffer();

  MmappedBuffer(const MmappedBuffer&) = delete;
  MmappedBuffer& operator=(const MmappedBuffer&) = delete;

  MmappedPlanes mmapped_planes_;
  const uint32_t num_planes_;
  uint32_t buffer_id_;
  // Indicates which frame in input bitstream corresponds to this MmappedBuffer
  // in OUTPUT queue.
  uint32_t frame_number_;
};

using MmappedBuffers = std::vector<scoped_refptr<MmappedBuffer>>;

// V4L2Queue class maintains properties of a queue.
class V4L2Queue {
 public:
  V4L2Queue(enum v4l2_buf_type type,
            const gfx::Size& resolution,
            enum v4l2_memory memory);

  V4L2Queue(const V4L2Queue&) = delete;
  V4L2Queue& operator=(const V4L2Queue&) = delete;
  ~V4L2Queue();

  // Retrieves a mmapped buffer for the given |buffer_id|, which is a decoded
  // surface, from MmappedBuffers.
  scoped_refptr<MmappedBuffer> GetBuffer(const size_t buffer_id) const;

  enum v4l2_buf_type type() const { return type_; }
  uint32_t fourcc() const { return fourcc_; }
  void set_fourcc(uint32_t fourcc) { fourcc_ = fourcc; }

  gfx::Size resolution() const { return resolution_; }
  void set_resolution(gfx::Size resolution) { resolution_ = resolution; }

  enum v4l2_memory memory() const { return memory_; }

  void set_buffers(MmappedBuffers& buffers) { buffers_ = buffers; }

  uint32_t num_buffers() const { return num_buffers_; }
  void set_num_buffers(uint32_t num_buffers) { num_buffers_ = num_buffers; }

  uint32_t num_planes() const { return num_planes_; }
  void set_num_planes(uint32_t num_planes) { num_planes_ = num_planes; }

  uint32_t last_queued_buffer_id() const { return last_queued_buffer_id_; }
  void set_last_queued_buffer_id(uint32_t last_queued_buffer_id) {
    last_queued_buffer_id_ = last_queued_buffer_id;
  }

  int media_request_fd() const { return media_request_fd_; }
  void set_media_request_fd(int media_request_fd) {
    media_request_fd_ = media_request_fd;
  }

  std::set<uint32_t> queued_buffer_ids() const { return queued_buffer_ids_; }

  void QueueBufferId(uint32_t last_queued_buffer_id) {
    queued_buffer_ids_.insert(last_queued_buffer_id);
  }

  void DequeueBufferId(uint32_t buffer_id) {
    queued_buffer_ids_.erase(buffer_id);
  }

  void DequeueAllBufferIds() { queued_buffer_ids_.clear(); }

 private:
  const enum v4l2_buf_type type_;
  uint32_t fourcc_;
  MmappedBuffers buffers_;
  uint32_t num_buffers_;
  // For the OUTPUT queue resolution refers to the coded dimensions of the
  // video. For the CAPTURE queue resolution refers to the size of the
  // buffer necessary for the driver to decode into and must
  // contain the resolution of the OUTPUT queue.
  gfx::Size resolution_;
  uint32_t num_planes_;
  const enum v4l2_memory memory_;
  // File descriptor returned by MEDIA_IOC_REQUEST_ALLOC ioctl call
  // to submit requests.
  int media_request_fd_;
  // Tracks which CAPTURE buffer was queued in the previous frame.
  uint32_t last_queued_buffer_id_;
  std::set<uint32_t> queued_buffer_ids_;
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
  void SetFmt(const std::unique_ptr<V4L2Queue>& queue) const;

  // Retrieves the format, |fmt|, (via VIDIOC_G_FMT)
  void GetFmt(struct v4l2_format* fmt) const;

  // Tries to configure |fmt|. This does not modify the underlying driver state.
  // https://www.kernel.org/doc/html/v5.10/userspace-api/media/v4l/vidioc-g-fmt.html?highlight=vidioc_try_fmt#description
  void TryFmt(struct v4l2_format* fmt) const;

  // Allocates buffers via VIDIOC_REQBUFS for |queue| with a buffer count.
  void ReqBufs(std::unique_ptr<V4L2Queue>& queue, uint32_t count) const;

  // Enqueues an empty (capturing) or filled (output) buffer
  // in the driver's incoming |queue|.
  [[nodiscard]] bool QBuf(const std::unique_ptr<V4L2Queue>& queue,
                          const uint32_t buffer_id) const;

  // Dequeues a filled (capturing) or decoded (output) buffer
  // from the driver’s outgoing |queue|.
  void DQBuf(const std::unique_ptr<V4L2Queue>& queue,
             uint32_t* buffer_id) const;

  // Starts streaming |queue| (via VIDIOC_STREAMON).
  void StreamOn(const enum v4l2_buf_type type) const;

  // Stops streaming |queue| (via VIDIOC_STREAMOFF).
  void StreamOff(const enum v4l2_buf_type type) const;

  // Sets the value of controls which specify decoding parameters for each
  // frame. |immediate| forces the call to be processed immediately when
  // |MediaIocRequestAlloc| is next called as opposed to being put in the queue.
  void SetExtCtrls(const std::unique_ptr<V4L2Queue>& queue,
                   v4l2_ext_controls* ext_ctrls,
                   bool immediate = false) const;

  // Allocates requests (likely one per OUTPUT buffer) via
  // MEDIA_IOC_REQUEST_ALLOC on the media device.
  void MediaIocRequestAlloc(int* req_fd) const;

  // Submits a request for the given OUTPUT |queue| by queueing
  // the request with |queue|'s media_request_fd().
  void MediaRequestIocQueue(const std::unique_ptr<V4L2Queue>& queue) const;

  // Re-initializes the previously allocated request for reuse.
  void MediaRequestIocReinit(const std::unique_ptr<V4L2Queue>& queue) const;

  // Completion of the request implies that the OUTPUT and CAPTURE buffers
  // are available for dequeueing
  void WaitForRequestCompletion(const std::unique_ptr<V4L2Queue>& queue) const;

  // Finds available media device for video decoder. This function also checks
  // to make sure either |bus_info| or |driver| field from |media_device_info|
  // struct (obtained from MEDIA_IOC_DEVICE_INFO call) is matched from the same
  // field in |v4l2_capability| struct.
  [[nodiscard]] bool FindMediaDevice(struct v4l2_capability* cap);

  // Allocates buffers for the given |queue|.
  void QueryAndMmapQueueBuffers(std::unique_ptr<V4L2Queue>& queue) const;

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

  // Whether V4L2_CTRL_WHICH_CUR_VAL is implemented correctly
  bool cur_val_is_supported_ = true;
};

}  // namespace v4l2_test
}  // namespace media

#endif  // MEDIA_GPU_V4L2_TEST_V4L2_IOCTL_SHIM_H_
