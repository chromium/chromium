// Copyright 2014 The Chromium Authors. All rights reserved.
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

#include <vector>

#include <linux/videodev2.h>

#include "base/containers/flat_map.h"
#include "base/files/scoped_file.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_layout.h"
#include "media/gpu/media_gpu_export.h"
#include "media/video/video_decode_accelerator.h"
#include "media/video/video_encode_accelerator.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_image.h"

// TODO(posciak): remove this once V4L2 headers are updated.
#define V4L2_PIX_FMT_MT21 v4l2_fourcc('M', 'T', '2', '1')
#ifndef V4L2_BUF_FLAG_LAST
#define V4L2_BUF_FLAG_LAST 0x00100000
#endif

namespace media {

class V4L2Queue;
class V4L2BufferQueueProxy;

// A unique reference to a buffer for clients to prepare and submit.
//
// Clients can prepare a buffer for queuing using the methods of this class, and
// then either queue it using the Queue() method corresponding to the memory
// type of the buffer, or drop the reference to make the buffer available again.
class MEDIA_GPU_EXPORT V4L2WritableBufferRef {
 public:
  // Default constructor, creates invalid buffer reference.
  V4L2WritableBufferRef();
  V4L2WritableBufferRef(V4L2WritableBufferRef&& other);
  V4L2WritableBufferRef& operator=(V4L2WritableBufferRef&& other);

  // Returns true if the reference points to a valid buffer.
  bool IsValid() const;

  // Return the memory type of the buffer. Useful to e.g. decide which Queue()
  // method to use.
  enum v4l2_memory Memory() const;

  // Queue a MMAP buffer.
  // If successful, true is returned and the reference to the buffer is dropped
  // so this reference becomes invalid.
  // In case of error, false is returned and the buffer is returned to the free
  // list.
  bool QueueMMap() &&;
  // Queue a USERPTR buffer, assigning |ptrs| as pointer for each plane.
  // The size of |ptrs| must be equal to the number of planes of this buffer.
  // If successful, true is returned and the reference to the buffer is dropped
  // so this reference becomes invalid.
  // In case of error, false is returned and the buffer is returned to the free
  // list.
  bool QueueUserPtr(const std::vector<void*>& ptrs) &&;
  // Queue a DMABUF buffer, assigning |fds| as file descriptors for each plane.
  // The size of |fds| must be equal to the number of planes of this buffer.
  // If successful, true is returned and the reference to the buffer is dropped
  // so this reference becomes invalid.
  // In case of error, false is returned and the buffer is returned to the free
  // list.
  bool QueueDMABuf(const std::vector<base::ScopedFD>& fds) &&;

  // Returns the number of planes in this buffer.
  size_t PlanesCount() const;
  // Returns the size of the requested |plane|, in bytes.
  size_t GetPlaneSize(const size_t plane) const;
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

  // Return the V4L2 buffer ID of the underlying buffer.
  // TODO(acourbot) This is used for legacy clients but should be ultimately
  // removed. See crbug/879971
  size_t BufferId() const;

  ~V4L2WritableBufferRef();

 private:
  // Do the actual queue operation once the v4l2_buffer structure is properly
  // filled.
  bool DoQueue() &&;

  V4L2WritableBufferRef(const struct v4l2_buffer* v4l2_buffer,
                        scoped_refptr<V4L2Queue> queue);
  friend class V4L2BufferRefFactory;

  std::unique_ptr<V4L2BufferQueueProxy> buffer_data_;

  SEQUENCE_CHECKER(sequence_checker_);
  DISALLOW_COPY_AND_ASSIGN(V4L2WritableBufferRef);
};

// A reference to a read-only, dequeued buffer.
//
// Clients use this class to query the buffer state and content, and are
// guaranteed that the buffer will not be reused until all references are
// destroyed.
class MEDIA_GPU_EXPORT V4L2ReadableBuffer
    : public base::RefCounted<V4L2ReadableBuffer> {
 public:
  // Returns whether the V4L2_BUF_FLAG_LAST flag is set for this buffer.
  bool IsLast() const;
  // Return the timestamp set by the driver on this buffer.
  struct timeval GetTimeStamp() const;
  // Returns the number of planes in this buffer.
  size_t PlanesCount() const;
  // Returns the number of bytes used for |plane|.
  size_t GetPlaneBytesUsed(size_t plane) const;

  // Return the V4L2 buffer ID of the underlying buffer.
  // TODO(acourbot) This is used for legacy clients but should be ultimately
  // removed. See crbug/879971
  size_t BufferId() const;

 private:
  ~V4L2ReadableBuffer();

  V4L2ReadableBuffer(const struct v4l2_buffer* v4l2_buffer,
                     scoped_refptr<V4L2Queue> queue);
  friend class V4L2BufferRefFactory;

  std::unique_ptr<V4L2BufferQueueProxy> buffer_data_;

  SEQUENCE_CHECKER(sequence_checker_);
  friend class base::RefCounted<V4L2ReadableBuffer>;
  DISALLOW_COPY_AND_ASSIGN(V4L2ReadableBuffer);
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
  // Allocate |count| buffers for the current format of this queue, with a
  // specific |memory| allocation, and returns the number of buffers allocated
  // or zero if an error occurred, or if references to any previously allocated
  // buffers are still held by any clients.
  //
  // The number of allocated buffers may be larger than the number requested, so
  // callers must always check the return value.
  //
  // Calling this method while buffers are still allocated results in an error.
  size_t AllocateBuffers(size_t count,
                         enum v4l2_memory memory) WARN_UNUSED_RESULT;

  // Deallocate all buffers previously allocated by |AllocateBuffers|. Any
  // references to buffers previously allocated held by the client must be
  // released, or this call will fail.
  bool DeallocateBuffers();

  // Return a unique pointer to a free buffer for the caller to prepare and
  // submit, or an empty pointer if no buffer is currently free.
  //
  // If the caller discards the returned reference, the underlying buffer is
  // made available to clients again.
  V4L2WritableBufferRef GetFreeBuffer();

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
  std::pair<bool, V4L2ReadableBufferRef> DequeueBuffer();

  // Returns true if this queue is currently streaming.
  bool IsStreaming() const;
  // If not currently streaming, starts streaming. Returns true if we started
  // streaming, or were already streaming, or false if we were not streaming
  // and an error occurred when attempting to start the stream. On failure, any
  // previously-queued buffers will be dequeued without processing and made
  // available to the client, while any buffers held by the client will remain
  // unchanged and their ownership will remain with the client.
  bool Streamon();
  // If currently streaming, stops streaming. Also make all queued buffers
  // available to the client again regardless of the streaming state.
  // If an error occurred while attempting to stop streaming, then false is
  // returned and queued buffers are left untouched since the V4L2 queue may
  // still be using them.
  bool Streamoff();

  // Returns the number of buffers currently allocated for this queue.
  size_t AllocatedBuffersCount() const;
  // Returns the number of currently free buffers on this queue.
  size_t FreeBuffersCount() const;
  // Returns the number of buffers currently queued on this queue.
  size_t QueuedBuffersCount() const;

 private:
  ~V4L2Queue();

  // Called when clients lose their reference to a buffer.
  void ReturnBuffer(size_t buffer_id);
  // Called when clients request a buffer to be queued.
  bool QueueBuffer(struct v4l2_buffer* v4l2_buffer);

  const enum v4l2_buf_type type_;
  enum v4l2_memory memory_ = V4L2_MEMORY_MMAP;
  bool is_streaming_ = false;
  size_t planes_count_ = 0;

  std::vector<std::unique_ptr<V4L2Buffer>> buffers_;

  // Buffers that are available for client to get and submit.
  // Buffers in this list are not referenced by anyone else than ourselves.
  std::set<size_t> free_buffers_;
  // Buffers that have been queued by the client, and not dequeued yet.
  std::set<size_t> queued_buffers_;

  scoped_refptr<V4L2Device> device_;
  // Callback to call in this queue's destructor.
  base::OnceClosure destroy_cb_;

  V4L2Queue(scoped_refptr<V4L2Device> dev,
            enum v4l2_buf_type type,
            base::OnceClosure destroy_cb);
  friend class V4L2QueueFactory;
  friend class V4L2BufferQueueProxy;
  friend class base::RefCountedThreadSafe<V4L2Queue>;

  SEQUENCE_CHECKER(sequence_checker_);
  DISALLOW_COPY_AND_ASSIGN(V4L2Queue);
};

class MEDIA_GPU_EXPORT V4L2Device
    : public base::RefCountedThreadSafe<V4L2Device> {
 public:
  // Utility format conversion functions
  static VideoPixelFormat V4L2PixFmtToVideoPixelFormat(uint32_t format);
  static uint32_t VideoPixelFormatToV4L2PixFmt(VideoPixelFormat format);
  static uint32_t VideoCodecProfileToV4L2PixFmt(VideoCodecProfile profile,
                                                bool slice_based);
  static VideoCodecProfile V4L2VP9ProfileToVideoCodecProfile(uint32_t profile);
  std::vector<VideoCodecProfile> V4L2PixFmtToVideoCodecProfiles(
      uint32_t pix_fmt,
      bool is_encoder);
  static uint32_t V4L2PixFmtToDrmFormat(uint32_t format);
  // Convert format requirements requested by a V4L2 device to gfx::Size.
  static gfx::Size CodedSizeFromV4L2Format(struct v4l2_format format);

  // Convert required H264 profile and level to V4L2 enums.
  static int32_t VideoCodecProfileToV4L2H264Profile(VideoCodecProfile profile);
  static int32_t H264LevelIdcToV4L2H264Level(uint8_t level_idc);

  // Composes human readable string of v4l2_format.
  static std::string V4L2FormatToString(const struct v4l2_format& format);

  // Composes VideoFrameLayout based on v4l2_format.
  // If error occurs, it returns base::nullopt.
  static base::Optional<VideoFrameLayout> V4L2FormatToVideoFrameLayout(
      const struct v4l2_format& format);

  enum class Type {
    kDecoder,
    kEncoder,
    kImageProcessor,
    kJpegDecoder,
  };

  // Create and initialize an appropriate V4L2Device instance for the current
  // platform, or return nullptr if not available.
  static scoped_refptr<V4L2Device> Create();

  // Open a V4L2 device of |type| for use with |v4l2_pixfmt|.
  // Return true on success.
  // The device will be closed in the destructor.
  virtual bool Open(Type type, uint32_t v4l2_pixfmt) = 0;

  // Returns the V4L2Queue corresponding to the requested |type|, or nullptr
  // if the requested queue type is not supported.
  scoped_refptr<V4L2Queue> GetQueue(enum v4l2_buf_type type);

  // Parameters and return value are the same as for the standard ioctl() system
  // call.
  virtual int Ioctl(int request, void* arg) = 0;

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
  virtual bool CanCreateEGLImageFrom(uint32_t v4l2_pixfmt) = 0;

  // Create an EGLImage from provided |dmabuf_fds| and bind |texture_id| to it.
  // Some implementations may also require the V4L2 |buffer_index| of the buffer
  // for which |dmabuf_fds| have been exported.
  // The caller may choose to close the file descriptors after this method
  // returns, and may expect the buffers to remain valid for the lifetime of
  // the created EGLImage.
  // Return EGL_NO_IMAGE_KHR on failure.
  virtual EGLImageKHR CreateEGLImage(
      EGLDisplay egl_display,
      EGLContext egl_context,
      GLuint texture_id,
      const gfx::Size& size,
      unsigned int buffer_index,
      uint32_t v4l2_pixfmt,
      const std::vector<base::ScopedFD>& dmabuf_fds) = 0;

  // Create a GLImage from provided |dmabuf_fds|.
  // The caller may choose to close the file descriptors after this method
  // returns, and may expect the buffers to remain valid for the lifetime of
  // the created GLImage.
  // Return the newly created GLImage.
  virtual scoped_refptr<gl::GLImage> CreateGLImage(
      const gfx::Size& size,
      uint32_t fourcc,
      const std::vector<base::ScopedFD>& dmabuf_fds) = 0;

  // Destroys the EGLImageKHR.
  virtual EGLBoolean DestroyEGLImage(EGLDisplay egl_display,
                                     EGLImageKHR egl_image) = 0;

  // Returns the supported texture target for the V4L2Device.
  virtual GLenum GetTextureTarget() = 0;

  // Returns the preferred V4L2 input format for |type| or 0 if none.
  virtual uint32_t PreferredInputFormat(Type type) = 0;

  // NOTE: The below methods to query capabilities have a side effect of
  // closing the previously-open device, if any, and should not be called after
  // Open().
  // TODO(posciak): fix this.

  // Get minimum and maximum resolution for fourcc |pixelformat| and store to
  // |min_resolution| and |max_resolution|.
  void GetSupportedResolution(uint32_t pixelformat,
                              gfx::Size* min_resolution,
                              gfx::Size* max_resolution);

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

  // Return true if JPEG decoding is supported, false otherwise.
  virtual bool IsJpegDecodingSupported() = 0;

 protected:
  friend class base::RefCountedThreadSafe<V4L2Device>;
  V4L2Device();
  virtual ~V4L2Device();

  VideoDecodeAccelerator::SupportedProfiles EnumerateSupportedDecodeProfiles(
      const size_t num_formats,
      const uint32_t pixelformats[]);

  VideoEncodeAccelerator::SupportedProfiles EnumerateSupportedEncodeProfiles();

  std::vector<uint32_t> EnumerateSupportedPixelformats(v4l2_buf_type buf_type);

 private:
  // Perform platform-specific initialization of the device instance.
  // Return true on success, false on error or if the particular implementation
  // is not available.
  virtual bool Initialize() = 0;

  // Associates a v4l2_buf_type to its queue.
  base::flat_map<enum v4l2_buf_type, V4L2Queue*> queues_;

  // Callback that is called upon a queue's destruction, to cleanup its pointer
  // in queues_.
  void OnQueueDestroyed(v4l2_buf_type buf_type);
};

}  //  namespace media

#endif  // MEDIA_GPU_V4L2_V4L2_DEVICE_H_
