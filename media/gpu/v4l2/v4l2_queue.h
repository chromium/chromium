// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_V4L2_QUEUE_H_
#define MEDIA_GPU_V4L2_V4L2_QUEUE_H_

#include <linux/videodev2.h>
#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <queue>
#include <vector>

#include "base/containers/small_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "build/build_config.h"
#include "media/base/video_codecs.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/gpu/chromeos/chromeos_status.h"
#include "media/gpu/chromeos/fourcc.h"
#include "media/gpu/chromeos/frame_resource.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/v4l2/v4l2_utils.h"
#include "ui/gfx/generic_shared_memory_id.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {
struct NativePixmapPlane;
}  // namespace gfx

namespace media {

class V4L2Queue;
class V4L2Buffer;
class V4L2BufferRefBase;
class V4L2BuffersList;
class V4L2RequestRef;
class V4L2BufferRefFactory;
typedef struct SecureBufferData SecureBufferData;

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
  // Queues |frame_resource| using its file descriptors as DMABUFs. The
  // FrameResource must use DMABUF fd-based storage. When called, this method
  // keeps a reference to |frame_resource| and releases it when the buffer is
  // dequeued through |V4L2ReadableBufferRef::GetFrameResource()|.
  // |frame_resource| is thus guaranteed to be alive until either all the
  // |V4L2ReadableBufferRef| from the dequeued buffer get out of scope, or
  // |V4L2Queue::Streamoff()| is called. Usage must not be mixed with
  // |QueueDMABuf(scoped_refptr<VideoFrame>)|.
  [[nodiscard]] bool QueueDMABuf(scoped_refptr<FrameResource> frame_resource,
                                 V4L2RequestRef* request_ref = nullptr) &&;
  // Queue a DMABUF with the corresponding |secure_handle|. This is used during
  // secure playback and the corresponding FD for the |secure_handle| will be
  // resolved by the V4L2Queue.
  [[nodiscard]] bool QueueDMABuf(uint64_t secure_handle,
                                 V4L2RequestRef* request_ref) &&;

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

  // Return the FrameResource underlying this buffer. The FrameResource's layout
  // will match that of the V4L2 format. This method will *always* return the
  // same FrameResource instance for a given V4L2 buffer. Moreover, the
  // FrameResource instance will also be the same across V4L2WritableBufferRef
  // and V4L2ReadableBufferRef if both references point to the same V4L2 buffer.
  // Note: at the moment, this method is valid for MMAP buffers only. It will
  // return nullptr for any other buffer type.
  [[nodiscard]] scoped_refptr<FrameResource> GetFrameResource();

  // Return the V4L2 buffer ID of the underlying buffer.
  // TODO(acourbot) This is used for legacy clients but should be ultimately
  // removed. See crbug/879971
  size_t BufferId() const;

  V4L2WritableBufferRef(const V4L2WritableBufferRef&) = delete;
  V4L2WritableBufferRef& operator=(const V4L2WritableBufferRef&) = delete;

  ~V4L2WritableBufferRef();

 private:
  friend class V4L2BufferRefFactory;

  // DoQueue does the actual queue operation once the v4l2_buffer structure is
  // properly filled. When requests are supported, a |request_ref| can be
  // passed along this the buffer to be submitted.
  [[nodiscard]] bool DoQueue(V4L2RequestRef* request_ref,
                             scoped_refptr<FrameResource> frame_resource) &&;

  V4L2WritableBufferRef(const struct v4l2_buffer& v4l2_buffer,
                        base::WeakPtr<V4L2Queue> queue);

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
// is required because V4L2ReadableBufferRefs can be embedded into frames,
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
  // Returns whether the V4L2_BUF_FLAG_ERROR flag is set for this buffer.
  bool IsError() const;
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

  // Return the FrameResource underlying this buffer. The FrameResource's layout
  // will match that of the V4L2 format. This method will *always* return the
  // same FrameResource instance for a given V4L2 buffer. Moreover, the
  // FrameResource instance will also be the same across V4L2WritableBufferRef
  // and V4L2ReadableBufferRef if both references point to the same V4L2 buffer.
  // Note: at the moment, this method is valid for MMAP buffers only. It will
  // return nullptr for any other buffer type.
  [[nodiscard]] scoped_refptr<FrameResource> GetFrameResource();

 private:
  friend class V4L2BufferRefFactory;
  friend class base::RefCountedThreadSafe<V4L2ReadableBuffer>;

  ~V4L2ReadableBuffer();

  V4L2ReadableBuffer(const struct v4l2_buffer& v4l2_buffer,
                     base::WeakPtr<V4L2Queue> queue,
                     scoped_refptr<FrameResource> frame);

  std::unique_ptr<V4L2BufferRefBase> buffer_data_;
  // If this buffer was a DMABUF buffer queued with
  // QueueDMABuf(scoped_refptr<FrameResource>), then this will hold the frame
  // that was passed at the time of queueing.
  scoped_refptr<FrameResource> frame_;

  SEQUENCE_CHECKER(sequence_checker_);
};

// Shortcut for naming consistency.
using V4L2ReadableBufferRef = scoped_refptr<V4L2ReadableBuffer>;

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

  raw_ptr<V4L2Request> request_;

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
  V4L2RequestRef(V4L2RequestRef&& req_ref)
      : V4L2RequestRefBase(std::move(req_ref)) {}

  V4L2RequestRef(const V4L2RequestRef&) = delete;
  V4L2RequestRef& operator=(const V4L2RequestRef&) = delete;

  // Apply controls to the request.
  bool ApplyCtrls(struct v4l2_ext_controls* ctrls) const;
  // Apply buffer to the request.
  [[nodiscard]] bool ApplyQueueBuffer(struct v4l2_buffer* buffer) const;
  // Submits the request to the driver.
  std::optional<V4L2SubmittedRequestRef> Submit() &&;

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
  V4L2SubmittedRequestRef(V4L2SubmittedRequestRef&& req_ref)
      : V4L2RequestRefBase(std::move(req_ref)) {}

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
  std::optional<V4L2RequestRef> GetFreeRequest();

 private:
  // File descriptor of the media device (/dev/mediaX) from which requests
  // are created.
  base::ScopedFD media_fd_;

  // Stores all available requests.
  std::vector<std::unique_ptr<V4L2Request>> requests_;
  std::queue<V4L2Request*> free_requests_;

  // Returns a new request file descriptor.
  std::optional<base::ScopedFD> CreateRequestFD();

  friend class V4L2Request;
  // Returns a request to the queue after being used.
  void ReturnRequest(V4L2Request* request);

  friend class V4L2Device;
  friend std::unique_ptr<V4L2RequestsQueue>::deleter_type;
  V4L2RequestsQueue(base::ScopedFD&& media_fd);
  ~V4L2RequestsQueue();

  SEQUENCE_CHECKER(sequence_checker_);
};

// Interface representing a specific V4L2 queue. It provides free
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
  [[nodiscard]] std::optional<struct v4l2_format>
  SetFormat(uint32_t fourcc, const gfx::Size& size, size_t buffer_size);

  // Identical to |SetFormat|, but does not actually apply the format, and can
  // be called anytime.
  // Returns an adjusted V4L2 format if |fourcc| is supported by the queue, or
  // |nullopt| if |fourcc| is not supported or an ioctl error happened.
  [[nodiscard]] std::optional<struct v4l2_format>
  TryFormat(uint32_t fourcc, const gfx::Size& size, size_t buffer_size);

  // Returns the currently set format on the queue. The result is returned as
  // a std::pair where the first member is the format, or std::nullopt if the
  // format could not be obtained due to an ioctl error. The second member is
  // only used in case of an error and contains the |errno| set by the failing
  // ioctl. If the first member is not std::nullopt, the second member will
  // always be zero.
  //
  // If the second member is 0, then the first member is guaranteed to have
  // a valid value. So clients that are not interested in the precise error
  // message can just check that the first member is valid and go on.
  //
  // This pair is used because not all failures to get the format are
  // necessarily errors, so we need to way to let the use decide whether it
  // is one or not.
  [[nodiscard]] std::pair<std::optional<struct v4l2_format>, int> GetFormat();

  // Codec-specific method to get the visible rectangle of the queue, using the
  // VIDIOC_G_SELECTION ioctl if available, or VIDIOC_G_CROP as a fallback.
  [[nodiscard]] std::optional<gfx::Rect> GetVisibleRect();

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

  // This returns the secure handle for a free buffer and then tags that buffer
  // as having its handle claimed. It expects another call later to
  // ReleaseSecureHandle to return control of the secure handle back to the
  // queue.
  [[nodiscard]] CroStatus::Or<uint64_t> GetFreeSecureHandle();
  void ReleaseSecureHandle(uint64_t secure_handle);

  // Return a reference to a free buffer for the caller to prepare and submit,
  // or nullopt if no buffer is currently free.
  //
  // If the caller discards the returned reference, the underlying buffer is
  // made available to clients again.
  [[nodiscard]] std::optional<V4L2WritableBufferRef> GetFreeBuffer();
  // Return the buffer at index |requested_buffer_id|, if it is available at
  // this time.
  //
  // If the buffer is currently in use or the provided index is invalid,
  // return |std::nullopt|.
  [[nodiscard]] std::optional<V4L2WritableBufferRef> GetFreeBuffer(
      size_t requested_buffer_id);
  // Return a V4L2 buffer suitable for the passed frame.
  //
  // This method will try as much as possible to always return the same V4L2
  // buffer when the same frame is passed again, to avoid memory unmap
  // operations in the kernel driver.
  //
  // The operating mode of the queue must be DMABUF, and the frame must be
  // backed either by a GpuMemoryBuffer, or by DMABUFs. In the case of DMABUFs,
  // this method will only work correctly if the same DMABUFs are passed with
  // each call, i.e. no dup shall be performed.
  //
  // This should be the preferred way to obtain buffers when using DMABUF mode,
  // since it will maximize performance in that case provided the number of
  // different frames passed to this method does not exceed the number of V4L2
  // buffers allocated on the queue.
  [[nodiscard]] std::optional<V4L2WritableBufferRef> GetFreeBufferForFrame(
      const gfx::GenericSharedMemoryId& id);

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
  [[nodiscard]] std::optional<struct v4l2_format> SetModifierFormat(
      uint64_t modifier,
      const gfx::Size& size);

  // Sends a V4L2_DEC_CMD_STOP/V4L2_DEC_CMD_START to this queue.
  [[nodiscard]] bool SendStopCommand();
  [[nodiscard]] bool SendStartCommand();

  // Sets the FD in a |v4L2_buffer| to be the one associated with the specified
  // |secure_handle|. Returns false if no such handle exists or is not currently
  // active.
  bool SetBufferFdForSecureHandle(uint64_t secure_handle,
                                  struct v4l2_buffer* v4l2_buffer);

 private:
  ~V4L2Queue();

  // Called when clients request a buffer to be queued.
  [[nodiscard]] bool QueueBuffer(struct v4l2_buffer* v4l2_buffer,
                                 scoped_refptr<FrameResource> frame);

  // Sends a V4L2_DEC_CMD_* to this queue.
  [[nodiscard]] bool SendCommand(__u32 command);

  // Callback used from secure buffer allocation.
  void SecureBufferAllocated(base::ScopedFD secure_fd, uint64_t secure_handle);

  const enum v4l2_buf_type type_;
  enum v4l2_memory memory_ = V4L2_MEMORY_MMAP;
  bool is_streaming_ = false;
  // Set to true if the queue supports requests.
  bool supports_requests_ = false;
  size_t planes_count_ = 0;
  // Current format as set by SetFormat.
  std::optional<struct v4l2_format> current_format_;

  std::vector<std::unique_ptr<V4L2Buffer>> buffers_;

  // Buffers that are available for client to get and submit.
  // Buffers in this list are not referenced by anyone else than ourselves.
  scoped_refptr<V4L2BuffersList> free_buffers_;

  // Buffers that have been queued by the client, and not dequeued yet, indexed
  // by the v4l2_buffer queue ID. The value will be set to the FrameResource
  // that has been passed when we queued the buffer, if any.
  base::small_map<std::map<size_t, scoped_refptr<FrameResource>>>
      queued_buffers_;

  // Dictionary of queue buffers (indexed 0... |buffers_| size), indexed by the
  // unique frame id (be that a GpuMemoryBuffer ID or a DmaBuf ID).
  std::map<gfx::GenericSharedMemoryId, size_t> free_buffers_indexes_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // List of the allocated secure buffers.
  std::vector<SecureBufferData> secure_buffers_;

  const IoctlAsCallback ioctl_cb_ GUARDED_BY_CONTEXT(sequence_checker_);
  const base::RepeatingClosure schedule_poll_cb_
      GUARDED_BY_CONTEXT(sequence_checker_);
  const MmapAsCallback mmap_cb_ GUARDED_BY_CONTEXT(sequence_checker_);
  const AllocateSecureBufferAsCallback allocate_secure_cb_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Callback to call in this queue's destructor.
  base::OnceClosure destroy_cb_;

  V4L2Queue(const IoctlAsCallback& ioctl_cb,
            const base::RepeatingClosure& schedule_poll_cb,
            const MmapAsCallback& mmap_cb,
            const AllocateSecureBufferAsCallback& allocate_secure_cb,
            enum v4l2_buf_type type,
            base::OnceClosure destroy_cb);
  friend class V4L2QueueFactory;
  friend class V4L2BufferRefBase;
  friend class base::RefCountedThreadSafe<V4L2Queue>;
  friend class V4L2StatefulVideoDecoder;

  SEQUENCE_CHECKER(sequence_checker_);

  bool incoherent_ = false;

  base::WeakPtrFactory<V4L2Queue> weak_this_factory_;
};

}  //  namespace media

#endif  // MEDIA_GPU_V4L2_V4L2_QUEUE_H_
