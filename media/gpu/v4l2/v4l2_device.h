// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines the V4L2Device interface which is used by the
// V4L2DecodeAccelerator class to delegate/pass the device specific
// handling of any of the functionalities.

#ifndef MEDIA_GPU_V4L2_V4L2_DEVICE_H_
#define MEDIA_GPU_V4L2_V4L2_DEVICE_H_

#include <stddef.h>
#include <stdint.h>

#include <queue>
#include <vector>

// build_config.h must come before BUILDFLAG()
#include "build/build_config.h"

#if BUILDFLAG(IS_CHROMEOS)
#include <linux/media/av1-ctrls.h>
#endif
#include <linux/videodev2.h>

#include "base/containers/flat_map.h"
#include "base/containers/small_map.h"
#include "base/files/scoped_file.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "media/base/video_codecs.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_layout.h"
#include "media/gpu/chromeos/fourcc.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/v4l2/buffer_affinity_tracker.h"
#include "media/gpu/v4l2/v4l2_device_poller.h"
#include "media/video/video_decode_accelerator.h"
#include "media/video/video_encode_accelerator.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_pixmap_handle.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_image.h"

// TODO(mojahsu): remove this once V4L2 headers are updated.
#ifndef V4L2_PIX_FMT_JPEG_RAW
#define V4L2_PIX_FMT_JPEG_RAW v4l2_fourcc('J', 'P', 'G', 'R')
#endif
#ifndef V4L2_CID_JPEG_LUMA_QUANTIZATION
#define V4L2_CID_JPEG_LUMA_QUANTIZATION (V4L2_CID_JPEG_CLASS_BASE + 5)
#endif
#ifndef V4L2_CID_JPEG_CHROMA_QUANTIZATION
#define V4L2_CID_JPEG_CHROMA_QUANTIZATION (V4L2_CID_JPEG_CLASS_BASE + 6)
#endif

// TODO(b/255770680): Remove this once V4L2 header is updated.
// https://patchwork.linuxtv.org/project/linux-media/patch/20210810220552.298140-2-daniel.almeida@collabora.com/
#ifndef V4L2_PIX_FMT_AV1
#define V4L2_PIX_FMT_AV1 v4l2_fourcc('A', 'V', '0', '1') /* AV1 */
#endif
#ifndef V4L2_PIX_FMT_AV1_FRAME
#define V4L2_PIX_FMT_AV1_FRAME                        \
  v4l2_fourcc('A', 'V', '1', 'F') /* AV1 parsed frame \
                                   */
#endif

#if BUILDFLAG(IS_CHROMEOS)
#ifndef V4L2_CID_MPEG_VIDEO_AV1_PROFILE
#define V4L2_CID_MPEG_VIDEO_AV1_PROFILE V4L2_CID_STATELESS_AV1_PROFILE
#endif
#ifndef V4L2_MPEG_VIDEO_AV1_PROFILE_MAIN
#define V4L2_MPEG_VIDEO_AV1_PROFILE_MAIN V4L2_STATELESS_AV1_PROFILE_MAIN
#endif
#ifndef V4L2_MPEG_VIDEO_AV1_PROFILE_HIGH
#define V4L2_MPEG_VIDEO_AV1_PROFILE_HIGH V4L2_STATELESS_AV1_PROFILE_HIGH
#endif
#ifndef V4L2_MPEG_VIDEO_AV1_PROFILE_PROFESSIONAL
#define V4L2_MPEG_VIDEO_AV1_PROFILE_PROFESSIONAL \
  V4L2_STATELESS_AV1_PROFILE_PROFESSIONAL
#endif
#endif

// TODO(b/260863940): Remove this once V4L2 header is updated
#ifndef V4L2_CID_MPEG_VIDEO_HEVC_PROFILE
#define V4L2_CID_MPEG_VIDEO_HEVC_PROFILE (V4L2_CID_MPEG_BASE + 615)
#endif

// TODO(b/132589320): remove this once V4L2 header is updated.
#ifndef V4L2_PIX_FMT_MM21
// MTK 8-bit block mode, two non-contiguous planes.
#define V4L2_PIX_FMT_MM21 v4l2_fourcc('M', 'M', '2', '1')
#endif

namespace gfx {
struct NativePixmapPlane;
}  // namespace gfx

namespace media {

class V4L2Queue;
class V4L2BufferRefBase;
class V4L2BuffersList;
class V4L2DecodeSurface;
class V4L2RequestRef;

// Wrapper for the 'v4l2_ext_control' structure.
struct V4L2ExtCtrl {
  V4L2ExtCtrl(uint32_t id);
  V4L2ExtCtrl(uint32_t id, int32_t val);
  struct v4l2_ext_control ctrl;
};

// A unique reference to a buffer for clients to prepare and submit.
//
// Clients can prepare a buffer for queuing using the methods of this class, and
// then either queue it using the Queue() method corresponding to the memory
// type of the buffer, or drop the reference to make the buffer available again.
class MEDIA_GPU_EXPORT V4L2WritableBufferRef {
 public:
  V4L2WritableBufferRef(V4L2WritableBufferRef&& other);
  V4L2WritableBufferRef() = delete;
  V4L2WritableBufferRef& operator=(V4L2WritableBufferRef&& other);

  // Return the memory type of the buffer. Useful to e.g. decide which Queue()
  // method to use.
  enum v4l2_memory Memory() const;

  // Queue a MMAP buffer.
  // When requests are supported, a |request_ref| can be passed along this
  // the buffer to be submitted.
  // If successful, true is returned and the reference to the buffer is dropped
  // so this reference becomes invalid.
  // In case of error, false is returned and the buffer is returned to the free
  // list.
  [[nodiscard]] bool QueueMMap(V4L2RequestRef* request_ref = nullptr) &&;
  // Queue a USERPTR buffer, assigning |ptrs| as pointer for each plane.
  // The size of |ptrs| must be equal to the number of planes of this buffer.
  // When requests are supported, a |request_ref| can be passed along this
  // the buffer to be submitted.
  // If successful, true is returned and the reference to the buffer is dropped
  // so this reference becomes invalid.
  // In case of error, false is returned and the buffer is returned to the free
  // list.
  [[nodiscard]] bool QueueUserPtr(const std::vector<void*>& ptrs,
                                  V4L2RequestRef* request_ref = nullptr) &&;
  // Queue a DMABUF buffer, assigning |fds| as file descriptors for each plane.
  // It is allowed the number of |fds| might be greater than the number of
  // planes of this buffer. It happens when the v4l2 pixel format is single
  // planar. The fd of the first plane is only used in that case.
  // When requests are supported, a |request_ref| can be passed along this
  // the buffer to be submitted.
  // If successful, true is returned and the reference to the buffer is dropped
  // so this reference becomes invalid.
  // In case of error, false is returned and the buffer is returned to the free
  // list.
  [[nodiscard]] bool QueueDMABuf(const std::vector<base::ScopedFD>& fds,
                                 V4L2RequestRef* request_ref = nullptr) &&;
  // Queue a DMABUF buffer, assigning file descriptors of |planes| for planes.
  // It is allowed the number of |planes| might be greater than the number of
  // planes of this buffer. It happens when the v4l2 pixel format is single
  // planar. The fd of the first plane of |planes| is only used in that case.
  // When requests are supported, a |request_ref| can be passed along this
  // the buffer to be submitted.
  // If successful, true is returned and the reference to the buffer is dropped
  // so this reference becomes invalid.
  // In case of error, false is returned and the buffer is returned to the free
  // list.
  [[nodiscard]] bool QueueDMABuf(
      const std::vector<gfx::NativePixmapPlane>& planes,
      V4L2RequestRef* request_ref = nullptr) &&;
  // Queue a |video_frame| using its file descriptors as DMABUFs. The VideoFrame
  // must have been constructed from its file descriptors.
  // The particularity of this method is that a reference to |video_frame| is
  // kept and made available again when the buffer is dequeued through
  // |V4L2ReadableBufferRef::GetVideoFrame()|. |video_frame| is thus guaranteed
  // to be alive until either all the |V4L2ReadableBufferRef| from the dequeued
  // buffer get out of scope, or |V4L2Queue::Streamoff()| is called.
  [[nodiscard]] bool QueueDMABuf(scoped_refptr<VideoFrame> video_frame,
                                 V4L2RequestRef* request_ref = nullptr) &&;

  // Returns the number of planes in this buffer.
  size_t PlanesCount() const;
  // Returns the size of the requested |plane|, in bytes.
  size_t GetPlaneSize(const size_t plane) const;
  // Set the size of the requested |plane|, in bytes. It is only valid for
  // USERPTR and DMABUF buffers. When using MMAP buffer, this method triggers a
  // DCHECK and is a no-op for release builds.
  void SetPlaneSize(const size_t plane, const size_t size);
  // This method can only be used with MMAP buffers.
  // It will return a pointer to the data of the |plane|th plane.
  // In case of error (invalid plane index or mapping failed), a nullptr is
  // returned.
  void* GetPlaneMapping(const size_t plane);
  // Set the timestamp field for this buffer.
  void SetTimeStamp(const struct timeval& timestamp);
  // Return the previously-set timestamp field for this buffer.
  const struct timeval& GetTimeStamp() const;
  // Set the number of bytes used for |plane|.
  void SetPlaneBytesUsed(const size_t plane, const size_t bytes_used);
  // Returns the previously-set number of bytes used for |plane|.
  size_t GetPlaneBytesUsed(const size_t plane) const;
  // Set the data offset for |plane|, in bytes.
  void SetPlaneDataOffset(const size_t plane, const size_t data_offset);

  // Return the VideoFrame underlying this buffer. The VideoFrame's layout
  // will match that of the V4L2 format. This method will *always* return the
  // same VideoFrame instance for a given V4L2 buffer. Moreover, the VideoFrame
  // instance will also be the same across V4L2WritableBufferRef and
  // V4L2ReadableBufferRef if both references point to the same V4L2 buffer.
  // Note: at the moment, this method is valid for MMAP buffers only. It will
  // return nullptr for any other buffer type.
  [[nodiscard]] scoped_refptr<VideoFrame> GetVideoFrame();

  // Return the V4L2 buffer ID of the underlying buffer.
  // TODO(acourbot) This is used for legacy clients but should be ultimately
  // removed. See crbug/879971
  size_t BufferId() const;

  V4L2WritableBufferRef(const V4L2WritableBufferRef&) = delete;
  V4L2WritableBufferRef& operator=(const V4L2WritableBufferRef&) = delete;

  ~V4L2WritableBufferRef();

 private:
  // Do the actual queue operation once the v4l2_buffer structure is properly
  // filled.
  // When requests are supported, a |request_ref| can be passed along this
  // the buffer to be submitted.
  [[nodiscard]] bool DoQueue(V4L2RequestRef* request_ref,
                             scoped_refptr<VideoFrame> video_frame) &&;

  V4L2WritableBufferRef(const struct v4l2_buffer& v4l2_buffer,
                        base::WeakPtr<V4L2Queue> queue);
  friend class V4L2BufferRefFactory;

  std::unique_ptr<V4L2BufferRefBase> buffer_data_;

  SEQUENCE_CHECKER(sequence_checker_);
};

// A reference to a read-only, dequeued buffer.
//
// Clients use this class to query the buffer state and content, and are
// guaranteed that the buffer will not be reused until all references are
// destroyed.
// All methods of this class must be called from the same sequence, but
// instances of V4L2ReadableBuffer objects can be destroyed from any sequence.
// They can even outlive the V4L2 buffers they originate from. This flexibility
// is required because V4L2ReadableBufferRefs can be embedded into VideoFrames,
// which are then passed to other threads and not necessarily destroyed before
// the V4L2Queue buffers are freed.
class MEDIA_GPU_EXPORT V4L2ReadableBuffer
    : public base::RefCountedThreadSafe<V4L2ReadableBuffer> {
 public:
  V4L2ReadableBuffer(const V4L2ReadableBuffer&) = delete;
  V4L2ReadableBuffer& operator=(const V4L2ReadableBuffer&) = delete;

  // Returns whether the V4L2_BUF_FLAG_LAST flag is set for this buffer.
  bool IsLast() const;
  // Returns whether the V4L2_BUF_FLAG_KEYFRAME flag is set for this buffer.
  bool IsKeyframe() const;
  // Return the timestamp set by the driver on this buffer.
  struct timeval GetTimeStamp() const;
  // Returns the number of planes in this buffer.
  size_t PlanesCount() const;
  // Returns the number of bytes used for |plane|.
  size_t GetPlaneBytesUsed(size_t plane) const;
  // Returns the data offset for |plane|.
  size_t GetPlaneDataOffset(size_t plane) const;
  // This method can only be used with MMAP buffers.
  // It will return a pointer to the data of the |plane|th plane.
  // In case of error (invalid plane index or mapping failed), a nullptr is
  // returned.
  const void* GetPlaneMapping(const size_t plane) const;

  // Return the V4L2 buffer ID of the underlying buffer.
  // TODO(acourbot) This is used for legacy clients but should be ultimately
  // removed. See crbug/879971
  size_t BufferId() const;

  // Return the VideoFrame underlying this buffer. The VideoFrame's layout
  // will match that of the V4L2 format. This method will *always* return the
  // same VideoFrame instance for a given V4L2 buffer. Moreover, the VideoFrame
  // instance will also be the same across V4L2WritableBufferRef and
  // V4L2ReadableBufferRef if both references point to the same V4L2 buffer.
  // Note: at the moment, this method is valid for MMAP buffers only. It will
  // return nullptr for any other buffer type.
  [[nodiscard]] scoped_refptr<VideoFrame> GetVideoFrame();

 private:
  friend class V4L2BufferRefFactory;
  friend class base::RefCountedThreadSafe<V4L2ReadableBuffer>;

  ~V4L2ReadableBuffer();

  V4L2ReadableBuffer(const struct v4l2_buffer& v4l2_buffer,
                     base::WeakPtr<V4L2Queue> queue,
                     scoped_refptr<VideoFrame> video_frame);

  std::unique_ptr<V4L2BufferRefBase> buffer_data_;
  // If this buffer was a DMABUF buffer queued with
  // QueueDMABuf(scoped_refptr<VideoFrame>), then this will hold the VideoFrame
  // that has been passed at the time of queueing.
  scoped_refptr<VideoFrame> video_frame_;

  SEQUENCE_CHECKER(sequence_checker_);
};

// Shortcut for naming consistency.
using V4L2ReadableBufferRef = scoped_refptr<V4L2ReadableBuffer>;

class V4L2Device;
class V4L2Buffer;

// Interface representing a specific queue of a |V4L2Device|. It provides free
// and queued buffer management that is commonly required by clients.
//
// Buffers managed by this class undergo the following cycle:
// 1) Allocated buffers are put into a free buffers pool, indicating that they
//    are used neither by the client nor the hardware.
// 2) The client obtains a unique, writable reference to one of the free
//    buffers in order to set its content and other parameters.
// 3) The client then queues the buffer obtained in 2), which invalidates its
//    reference. The buffer is now prepared to be processed by the hardware.
// 4) Once the hardware is done with the buffer, it is ready to be dequeued by
//    the client. The client obtains a read-only, counted reference to the
//    buffer and can read its content and metadata, as well as making other
//    references to it. The buffer will not be reused until all the references
//    are dropped. Once this happens, the buffer goes back to the free list
//    described in 1).
class MEDIA_GPU_EXPORT V4L2Queue
    : public base::RefCountedThreadSafe<V4L2Queue> {
 public:
  V4L2Queue(const V4L2Queue&) = delete;
  V4L2Queue& operator=(const V4L2Queue&) = delete;

  // Set |fourcc| as the current format on this queue. |size| corresponds to the
  // desired buffer's dimensions (i.e. width and height members of
  // v4l2_pix_format_mplane (if not applicable, pass gfx::Size()).
  // |buffer_size| is the desired size in bytes of the buffer for single-planar
  // formats (i.e. sizeimage of the first plane). It can be set to 0 if not
  // relevant for the desired format.
  // If the format could be set, then the |v4l2_format| reflecting the actual
  // format is returned. It is guaranteed to feature the specified |fourcc|,
  // but any other parameter (including |size| and |buffer_size| may have been
  // adjusted by the driver, so the caller must check their values.
  [[nodiscard]] absl::optional<struct v4l2_format>
  SetFormat(uint32_t fourcc, const gfx::Size& size, size_t buffer_size);

  // Identical to |SetFormat|, but does not actually apply the format, and can
  // be called anytime.
  // Returns an adjusted V4L2 format if |fourcc| is supported by the queue, or
  // |nullopt| if |fourcc| is not supported or an ioctl error happened.
  [[nodiscard]] absl::optional<struct v4l2_format>
  TryFormat(uint32_t fourcc, const gfx::Size& size, size_t buffer_size);

  // Returns the currently set format on the queue. The result is returned as
  // a std::pair where the first member is the format, or absl::nullopt if the
  // format could not be obtained due to an ioctl error. The second member is
  // only used in case of an error and contains the |errno| set by the failing
  // ioctl. If the first member is not absl::nullopt, the second member will
  // always be zero.
  //
  // If the second member is 0, then the first member is guaranteed to have
  // a valid value. So clients that are not interested in the precise error
  // message can just check that the first member is valid and go on.
  //
  // This pair is used because not all failures to get the format are
  // necessarily errors, so we need to way to let the use decide whether it
  // is one or not.
  [[nodiscard]] std::pair<absl::optional<struct v4l2_format>, int> GetFormat();

  // Codec-specific method to get the visible rectangle of the queue, using the
  // VIDIOC_G_SELECTION ioctl if available, or VIDIOC_G_CROP as a fallback.
  [[nodiscard]] absl::optional<gfx::Rect> GetVisibleRect();

  // Allocate |count| buffers for the current format of this queue, with a
  // specific |memory| allocation, and returns the number of buffers allocated
  // or zero if an error occurred, or if references to any previously allocated
  // buffers are still held by any clients.
  //
  // Setting the |incoherent| flag will allocate the buffers with the
  // V4L2_MEMORY_FLAG_NON_COHERENT flag set. This allows caching, which is a
  // potential performance improvement when reading from CPU, but may not be
  // safe for all V4L2 hardware. In particular, the MDP won't work with
  // incoherent memory.
  //
  // The number of allocated buffers may be larger than the number requested, so
  // callers must always check the return value.
  //
  // Calling this method while buffers are still allocated results in an error.
  [[nodiscard]] size_t AllocateBuffers(size_t count,
                                       enum v4l2_memory memory,
                                       bool incoherent);

  // Deallocate all buffers previously allocated by |AllocateBuffers|. Any
  // references to buffers previously allocated held by the client must be
  // released, or this call will fail.
  [[nodiscard]] bool DeallocateBuffers();

  // Returns the memory usage of v4l2 buffers owned by this V4L2Queue which are
  // mapped in user space memory.
  [[nodiscard]] size_t GetMemoryUsage() const;

  // Returns |memory_|, memory type of last buffers allocated by this V4L2Queue.
  [[nodiscard]] v4l2_memory GetMemoryType() const;

  // Return a reference to a free buffer for the caller to prepare and submit,
  // or nullopt if no buffer is currently free.
  //
  // If the caller discards the returned reference, the underlying buffer is
  // made available to clients again.
  [[nodiscard]] absl::optional<V4L2WritableBufferRef> GetFreeBuffer();
  // Return the buffer at index |requested_buffer_id|, if it is available at
  // this time.
  //
  // If the buffer is currently in use or the provided index is invalid,
  // return |absl::nullopt|.
  [[nodiscard]] absl::optional<V4L2WritableBufferRef> GetFreeBuffer(
      size_t requested_buffer_id);
  // Return a V4L2 buffer suitable for the passed VideoFrame.
  //
  // This method will try as much as possible to always return the same V4L2
  // buffer when the same frame is passed again, to avoid memory unmap
  // operations in the kernel driver.
  //
  // The operating mode of the queue must be DMABUF, and the VideoFrame must
  // be backed either by a GpuMemoryBuffer, or by DMABUFs. In the case of
  // DMABUFs, this method will only work correctly if the same DMABUFs are
  // passed with each call, i.e. no dup shall be performed.
  //
  // This should be the preferred way to obtain buffers when using DMABUF mode,
  // since it will maximize performance in that case provided the number of
  // different VideoFrames passed to this method does not exceed the number of
  // V4L2 buffers allocated on the queue.
  [[nodiscard]] absl::optional<V4L2WritableBufferRef> GetFreeBufferForFrame(
      const VideoFrame& frame);

  // Attempt to dequeue a buffer, and return a reference to it if one was
  // available.
  //
  // The first element of the returned pair will be false if an error occurred,
  // in which case the second element will be nullptr. If no error occurred,
  // then the first element will be true and the second element will contain a
  // reference to the dequeued buffer if one was available, or nullptr
  // otherwise.
  // Dequeued buffers will not be reused by the driver until all references to
  // them are dropped.
  [[nodiscard]] std::pair<bool, V4L2ReadableBufferRef> DequeueBuffer();

  // Returns true if this queue is currently streaming.
  [[nodiscard]] bool IsStreaming() const;
  // If not currently streaming, starts streaming. Returns true if we started
  // streaming, or were already streaming, or false if we were not streaming
  // and an error occurred when attempting to start the stream. On failure, any
  // previously-queued buffers will be dequeued without processing and made
  // available to the client, while any buffers held by the client will remain
  // unchanged and their ownership will remain with the client.
  [[nodiscard]] bool Streamon();
  // If currently streaming, stops streaming. Also make all queued buffers
  // available to the client again regardless of the streaming state.
  // If an error occurred while attempting to stop streaming, then false is
  // returned and queued buffers are left untouched since the V4L2 queue may
  // still be using them.
  [[nodiscard]] bool Streamoff();

  // Returns the number of buffers currently allocated for this queue.
  [[nodiscard]] size_t AllocatedBuffersCount() const;
  // Returns the number of currently free buffers on this queue.
  [[nodiscard]] size_t FreeBuffersCount() const;
  // Returns the number of buffers currently queued on this queue.
  [[nodiscard]] size_t QueuedBuffersCount() const;

  // Returns true if requests are supported by this queue.
  [[nodiscard]] bool SupportsRequests();

  // TODO (b/166275274) : Remove this once V4L2 properly supports modifiers.
  // Out of band method to configure V4L2 for modifier use.
  [[nodiscard]] absl::optional<struct v4l2_format> SetModifierFormat(
      uint64_t modifier,
      const gfx::Size& size);

 private:
  ~V4L2Queue();

  // Called when clients request a buffer to be queued.
  [[nodiscard]] bool QueueBuffer(struct v4l2_buffer* v4l2_buffer,
                                 scoped_refptr<VideoFrame> video_frame);

  const enum v4l2_buf_type type_;
  enum v4l2_memory memory_ = V4L2_MEMORY_MMAP;
  bool is_streaming_ = false;
  // Set to true if the queue supports requests.
  bool supports_requests_ = false;
  size_t planes_count_ = 0;
  // Current format as set by SetFormat.
  absl::optional<struct v4l2_format> current_format_;

  std::vector<std::unique_ptr<V4L2Buffer>> buffers_;

  // Buffers that are available for client to get and submit.
  // Buffers in this list are not referenced by anyone else than ourselves.
  scoped_refptr<V4L2BuffersList> free_buffers_;
  // Buffers that have been queued by the client, and not dequeued yet, indexed
  // by the v4l2_buffer queue ID. The value will be set to the VideoFrame that
  // has been passed when we queued the buffer, if any.
  base::small_map<std::map<size_t, scoped_refptr<VideoFrame>>> queued_buffers_;
  // Keep track of which buffer was assigned to which frame by
  // |GetFreeBufferForFrame()| so we reuse the same buffer in subsequent calls.
  BufferAffinityTracker affinity_tracker_;

  scoped_refptr<V4L2Device> device_;
  // Callback to call in this queue's destructor.
  base::OnceClosure destroy_cb_;

  V4L2Queue(scoped_refptr<V4L2Device> dev,
            enum v4l2_buf_type type,
            base::OnceClosure destroy_cb);
  friend class V4L2QueueFactory;
  friend class V4L2BufferRefBase;
  friend class base::RefCountedThreadSafe<V4L2Queue>;

  SEQUENCE_CHECKER(sequence_checker_);

  bool incoherent_ = false;

  base::WeakPtrFactory<V4L2Queue> weak_this_factory_;
};

class V4L2Request;

// Base class for all request related classes.
//
// This class is used to manage requests and not intended to be used
// directly.
class MEDIA_GPU_EXPORT V4L2RequestRefBase {
 public:
  V4L2RequestRefBase(const V4L2RequestRefBase&) = delete;
  V4L2RequestRefBase& operator=(const V4L2RequestRefBase&) = delete;

 protected:
  V4L2RequestRefBase(V4L2RequestRefBase&& req_base);
  V4L2RequestRefBase(V4L2Request* request);
  ~V4L2RequestRefBase();

  V4L2Request* request_;

  SEQUENCE_CHECKER(sequence_checker_);
};

class V4L2SubmittedRequestRef;

// Interface representing a request reference.
//
// The request reference allows the client to set the controls and buffer to
// the request. It also allows to submit the request to the driver.
// Once the request as been submitted, the request reference cannot be used
// any longer.
// Instead, when a request is submitted, an object denoting a submitted request
// is returned.
class MEDIA_GPU_EXPORT V4L2RequestRef : public V4L2RequestRefBase {
 public:
  V4L2RequestRef(V4L2RequestRef&& req_ref) :
    V4L2RequestRefBase(std::move(req_ref)) {}

  V4L2RequestRef(const V4L2RequestRef&) = delete;
  V4L2RequestRef& operator=(const V4L2RequestRef&) = delete;

  // Apply controls to the request.
  bool ApplyCtrls(struct v4l2_ext_controls* ctrls) const;
  // Apply buffer to the request.
  [[nodiscard]] bool ApplyQueueBuffer(struct v4l2_buffer* buffer) const;
  // Submits the request to the driver.
  absl::optional<V4L2SubmittedRequestRef> Submit() &&;

 private:
  friend class V4L2RequestsQueue;
  V4L2RequestRef(V4L2Request* request) : V4L2RequestRefBase(request) {}
};

// Interface representing a submitted request.
//
// After a request is submitted, a request reference cannot be used anymore.
// Instead, an object representing a submitted request is returned.
// Through this object, it is possible to check whether the request
// completed or not.
class MEDIA_GPU_EXPORT V4L2SubmittedRequestRef : public V4L2RequestRefBase {
 public:
  V4L2SubmittedRequestRef(V4L2SubmittedRequestRef&& req_ref) :
    V4L2RequestRefBase(std::move(req_ref)) {}

  V4L2SubmittedRequestRef(const V4L2SubmittedRequestRef&) = delete;
  V4L2SubmittedRequestRef& operator=(const V4L2SubmittedRequestRef&) = delete;

  // Indicates if the request has completed.
  [[nodiscard]] bool IsCompleted();

 private:
  friend class V4L2RequestRef;
  V4L2SubmittedRequestRef(V4L2Request* request) : V4L2RequestRefBase(request) {}
};

// Interface representing a queue of requests. The requests queue manages and
// recycles requests.
//
// Requests undergo the following cycle:
// 1) Allocated requests are put into a free request pool, indicating that they
//    are not used by the client and free to be used.
// 2) The client obtains a unique request reference to one of the free
//    requests in order to set its controls and buffer.
// 3) The client then submit the request obtained in 2), which invalidates its
//    reference and returns a reference to a submitted request.
// 4) Once client releases the submitted request reference, the request goes
//    back to the free request pool described in 1).
class MEDIA_GPU_EXPORT V4L2RequestsQueue {
 public:
  V4L2RequestsQueue(const V4L2RequestsQueue&) = delete;
  V4L2RequestsQueue& operator=(const V4L2RequestsQueue&) = delete;

  // Gets a free request. If no request is available, a non-valid request
  // reference will be returned.
  absl::optional<V4L2RequestRef> GetFreeRequest();

 private:
  // File descriptor of the media device (/dev/mediaX) from which requests
  // are created.
  base::ScopedFD media_fd_;

  // Stores all available requests.
  std::vector<std::unique_ptr<V4L2Request>> requests_;
  std::queue<V4L2Request*> free_requests_;

  // Returns a new request file descriptor.
  absl::optional<base::ScopedFD> CreateRequestFD();

  friend class V4L2Request;
  // Returns a request to the queue after being used.
  void ReturnRequest(V4L2Request* request);

  friend class V4L2Device;
  friend std::unique_ptr<V4L2RequestsQueue>::deleter_type;
  V4L2RequestsQueue(base::ScopedFD&& media_fd);
  ~V4L2RequestsQueue();

  SEQUENCE_CHECKER(sequence_checker_);
};

class MEDIA_GPU_EXPORT V4L2Device
    : public base::RefCountedThreadSafe<V4L2Device> {
 public:
  // Utility format conversion functions
  // If there is no corresponding single- or multi-planar format, returns 0.
  static uint32_t VideoCodecProfileToV4L2PixFmt(VideoCodecProfile profile,
                                                bool slice_based);
  std::vector<VideoCodecProfile> V4L2PixFmtToVideoCodecProfiles(
      uint32_t pix_fmt);
  // Calculates the largest plane's allocation size requested by a V4L2 device.
  static gfx::Size AllocatedSizeFromV4L2Format(
      const struct v4l2_format& format);

  // Convert required H264 profile and level to V4L2 enums.
  static int32_t VideoCodecProfileToV4L2H264Profile(VideoCodecProfile profile);
  static int32_t H264LevelIdcToV4L2H264Level(uint8_t level_idc);

  // Composes VideoFrameLayout based on v4l2_format.
  // If error occurs, it returns absl::nullopt.
  static absl::optional<VideoFrameLayout> V4L2FormatToVideoFrameLayout(
      const struct v4l2_format& format);

  // Returns number of planes of |pix_fmt|.
  static size_t GetNumPlanesOfV4L2PixFmt(uint32_t pix_fmt);

  enum class Type {
    kDecoder,
    kEncoder,
    kImageProcessor,
    kJpegDecoder,
    kJpegEncoder,
  };

  inline static constexpr char kLibV4l2Path[] =
#if defined(__aarch64__)
      "/usr/lib64/libv4l2.so";
#else
      "/usr/lib/libv4l2.so";
#endif

  // Returns true iff libv4l2 should be used to interact with the V4L2 driver.
  // This method is thread-safe.
  static bool UseLibV4L2();

  // Create and initialize an appropriate V4L2Device instance for the current
  // platform, or return nullptr if not available.
  static scoped_refptr<V4L2Device> Create();

  // Open a V4L2 device of |type| for use with |v4l2_pixfmt|.
  // Return true on success.
  // The device will be closed in the destructor.
  [[nodiscard]] virtual bool Open(Type type, uint32_t v4l2_pixfmt) = 0;

  // Returns the driver name.
  std::string GetDriverName();

  // Returns the V4L2Queue corresponding to the requested |type|, or nullptr
  // if the requested queue type is not supported.
  scoped_refptr<V4L2Queue> GetQueue(enum v4l2_buf_type type);

  // Parameters and return value are the same as for the standard ioctl() system
  // call.
  [[nodiscard]] virtual int Ioctl(int request, void* arg) = 0;

  // This method sleeps until either:
  // - SetDevicePollInterrupt() is called (on another thread),
  // - |poll_device| is true, and there is new data to be read from the device,
  //   or an event from the device has arrived; in the latter case
  //   |*event_pending| will be set to true.
  // Returns false on error, true otherwise.
  // This method should be called from a separate thread.
  virtual bool Poll(bool poll_device, bool* event_pending) = 0;

  // These methods are used to interrupt the thread sleeping on Poll() and force
  // it to return regardless of device state, which is usually when the client
  // is no longer interested in what happens with the device (on cleanup,
  // client state change, etc.). When SetDevicePollInterrupt() is called, Poll()
  // will return immediately, and any subsequent calls to it will also do so
  // until ClearDevicePollInterrupt() is called.
  virtual bool SetDevicePollInterrupt() = 0;
  virtual bool ClearDevicePollInterrupt() = 0;

  // Wrappers for standard mmap/munmap system calls.
  virtual void* Mmap(void* addr,
                     unsigned int len,
                     int prot,
                     int flags,
                     unsigned int offset) = 0;
  virtual void Munmap(void* addr, unsigned int len) = 0;

  // Return a vector of dmabuf file descriptors, exported for V4L2 buffer with
  // |index|, assuming the buffer contains |num_planes| V4L2 planes and is of
  // |type|. Return an empty vector on failure.
  // The caller is responsible for closing the file descriptors after use.
  virtual std::vector<base::ScopedFD> GetDmabufsForV4L2Buffer(
      int index,
      size_t num_planes,
      enum v4l2_buf_type type) = 0;

  // Return true if the given V4L2 pixfmt can be used in CreateEGLImage()
  // for the current platform.
  virtual bool CanCreateEGLImageFrom(const Fourcc fourcc) const = 0;

  // Create an EGLImage from provided |handle|, taking full ownership of it.
  // Some implementations may also require the V4L2 |buffer_index| of the buffer
  // for which |handle| has been exported.
  // Return EGL_NO_IMAGE_KHR on failure.
  virtual EGLImageKHR CreateEGLImage(EGLDisplay egl_display,
                                     EGLContext egl_context,
                                     GLuint texture_id,
                                     const gfx::Size& size,
                                     unsigned int buffer_index,
                                     const Fourcc fourcc,
                                     gfx::NativePixmapHandle handle) const = 0;

  // Create a GLImage from provided |handle|, taking full ownership of it.
  virtual scoped_refptr<gl::GLImage> CreateGLImage(
      const gfx::Size& size,
      const Fourcc fourcc,
      gfx::NativePixmapHandle handle) const = 0;

  // Destroys the EGLImageKHR.
  virtual EGLBoolean DestroyEGLImage(EGLDisplay egl_display,
                                     EGLImageKHR egl_image) const = 0;

  // Returns the supported texture target for the V4L2Device.
  virtual GLenum GetTextureTarget() const = 0;

  // Returns the preferred V4L2 input formats for |type| or empty if none.
  virtual std::vector<uint32_t> PreferredInputFormat(Type type) const = 0;

  // Get minimum and maximum resolution for fourcc |pixelformat| and store to
  // |min_resolution| and |max_resolution|.
  void GetSupportedResolution(uint32_t pixelformat,
                              gfx::Size* min_resolution,
                              gfx::Size* max_resolution);

  // Get the supported bitrate control modes. This function should be called
  // when V4L2Device opens an encoder driver node.
  VideoEncodeAccelerator::SupportedRateControlMode
  GetSupportedRateControlMode();

  std::vector<uint32_t> EnumerateSupportedPixelformats(v4l2_buf_type buf_type);

  // NOTE: The below methods to query capabilities have a side effect of
  // closing the previously-open device, if any, and should not be called after
  // Open().
  // TODO(b/150431552): fix this.

  // Return V4L2 pixelformats supported by the available image processor
  // devices for |buf_type|.
  virtual std::vector<uint32_t> GetSupportedImageProcessorPixelformats(
      v4l2_buf_type buf_type) = 0;

  // Return supported profiles for decoder, including only profiles for given
  // fourcc |pixelformats|.
  virtual VideoDecodeAccelerator::SupportedProfiles GetSupportedDecodeProfiles(
      const size_t num_formats,
      const uint32_t pixelformats[]) = 0;

  // Return supported profiles for encoder.
  virtual VideoEncodeAccelerator::SupportedProfiles
  GetSupportedEncodeProfiles() = 0;

  // Return true if image processing is supported, false otherwise.
  virtual bool IsImageProcessingSupported() = 0;

  // Return true if JPEG codec is supported, false otherwise.
  virtual bool IsJpegDecodingSupported() = 0;
  virtual bool IsJpegEncodingSupported() = 0;

  // Start polling on this V4L2Device. |event_callback| will be posted to
  // the caller's sequence if a buffer is ready to be dequeued and/or a V4L2
  // event has been posted. |error_callback| will be posted to the client's
  // sequence if a polling error has occurred.
  [[nodiscard]] bool StartPolling(
      V4L2DevicePoller::EventCallback event_callback,
      base::RepeatingClosure error_callback);
  // Stop polling this V4L2Device if polling was active. No new events will
  // be posted after this method has returned.
  [[nodiscard]] bool StopPolling();
  // Schedule a polling event if polling is enabled. This method is intended
  // to be called from V4L2Queue, clients should not need to call it directly.
  void SchedulePoll();

  // Attempt to dequeue a V4L2 event and return it.
  absl::optional<struct v4l2_event> DequeueEvent();

  // Returns requests queue to get free requests. A null pointer is returned if
  // the queue creation failed or if requests are not supported.
  V4L2RequestsQueue* GetRequestsQueue();

  // Check whether the V4L2 control with specified |ctrl_id| is supported.
  bool IsCtrlExposed(uint32_t ctrl_id);
  // Set the specified list of |ctrls| for the specified |ctrl_class|, returns
  // whether the operation succeeded. If |request_ref| is not nullptr, the
  // controls are applied to the request instead of globally for the device.
  bool SetExtCtrls(uint32_t ctrl_class,
                   std::vector<V4L2ExtCtrl> ctrls,
                   V4L2RequestRef* request_ref = nullptr);

  // Get the value of a single control, or absl::nullopt of the control is not
  // exposed by the device.
  absl::optional<struct v4l2_ext_control> GetCtrl(uint32_t ctrl_id);

  // Set periodic keyframe placement (group of pictures length)
  bool SetGOPLength(uint32_t gop_length);

 protected:
  friend class base::RefCountedThreadSafe<V4L2Device>;
  V4L2Device();
  virtual ~V4L2Device();

  VideoDecodeAccelerator::SupportedProfiles EnumerateSupportedDecodeProfiles(
      const size_t num_formats,
      const uint32_t pixelformats[]);

  VideoEncodeAccelerator::SupportedProfiles EnumerateSupportedEncodeProfiles();

 private:
  // Perform platform-specific initialization of the device instance.
  // Return true on success, false on error or if the particular implementation
  // is not available.
  [[nodiscard]] virtual bool Initialize() = 0;

  // Associates a v4l2_buf_type to its queue.
  base::flat_map<enum v4l2_buf_type, V4L2Queue*> queues_;

  // Callback that is called upon a queue's destruction, to cleanup its pointer
  // in queues_.
  void OnQueueDestroyed(v4l2_buf_type buf_type);

  // Used if EnablePolling() is called to signal the user that an event
  // happened or a buffer is ready to be dequeued.
  std::unique_ptr<V4L2DevicePoller> device_poller_;

  // Indicates whether the request queue creation has been tried once.
  bool requests_queue_creation_called_ = false;

  // The request queue stores all requests allocated to be used.
  std::unique_ptr<V4L2RequestsQueue> requests_queue_;

  SEQUENCE_CHECKER(client_sequence_checker_);
};

}  //  namespace media

#endif  // MEDIA_GPU_V4L2_V4L2_DEVICE_H_
