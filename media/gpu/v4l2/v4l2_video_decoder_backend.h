// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_V4L2_VIDEO_DECODER_BACKEND_H_
#define MEDIA_GPU_V4L2_V4L2_VIDEO_DECODER_BACKEND_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "media/base/decoder_status.h"
#include "media/base/video_color_space.h"
#include "media/base/video_decoder.h"
#include "media/gpu/chromeos/chromeos_status.h"
#include "media/gpu/chromeos/dmabuf_video_frame_pool.h"
#include "media/gpu/chromeos/frame_resource.h"
#include "ui/gfx/geometry/rect.h"

namespace media {

class DmabufVideoFramePool;
class V4L2Device;
class V4L2Queue;
class V4L2ReadableBuffer;
using V4L2ReadableBufferRef = scoped_refptr<V4L2ReadableBuffer>;

// Abstract class that performs the low-level V4L2 decoding tasks depending
// on the decoding API chosen (stateful or stateless).
//
// The backend receives encoded buffers via EnqueueDecodeTask() and is
// responsible for acquiring the V4L2 resources (output and capture buffers,
// etc) and sending them to the V4L2 driver. When a decoded buffer is dequeued,
// |OutputBufferDequeued| is automatically called from the decoder.
//
// The backend can call into some of the decoder methods, notably OutputFrame()
// to send a |FrameResource| to the decoder's client, and Error() to signal that
// an unrecoverable error has occurred.
//
// This class must run entirely inside the decoder thread. All overridden
// methods must check that they are called from sequence_checker_.
class V4L2VideoDecoderBackend {
 public:
  // Interface for the backend to call back into the decoder it is serving.
  // All methods must be called from the same sequence as the backend.
  class Client {
   public:
    // Inform that an unrecoverable error has occurred in the backend.
    virtual void OnBackendError() = 0;
    // Returns true is we are in a state that allows decoding to proceed.
    virtual bool IsDecoding() const = 0;
    // Start flushing. No new decoding requests will be processed until
    // CompleteFlush() is called.
    virtual void InitiateFlush() = 0;
    // Inform the flushing is complete.
    virtual void CompleteFlush() = 0;
    // Perform streamoff - streamon sequence to start capture queue when
    // it is stopped after LAST buffer dequeue.
    virtual void RestartStream() = 0;
    // Stop the stream to reallocate the CAPTURE buffers. Can only be done
    // between calls to |InitiateFlush| and |CompleteFlush|.
    virtual void ChangeResolution(gfx::Size pic_size,
                                  gfx::Rect visible_rect,
                                  size_t num_codec_reference_frames,
                                  uint8_t bit_depth) = 0;
    // Convert the frame and call the output callback.
    virtual void OutputFrame(scoped_refptr<FrameResource> frame,
                             const gfx::Rect& visible_rect,
                             const VideoColorSpace& color_space,
                             base::TimeDelta timestamp) = 0;
    // Get the video frame pool without passing the ownership.
    virtual DmabufVideoFramePool* GetVideoFramePool() const = 0;
  };

  V4L2VideoDecoderBackend(const V4L2VideoDecoderBackend&) = delete;
  V4L2VideoDecoderBackend& operator=(const V4L2VideoDecoderBackend&) = delete;

  virtual ~V4L2VideoDecoderBackend();

  virtual bool Initialize() = 0;

  // Schedule |buffer| to be processed.
  // The backend must call |decode_cb| once the buffer is not used anymore.
  virtual void EnqueueDecodeTask(scoped_refptr<DecoderBuffer> buffer,
                                 VideoDecoder::DecodeCB decode_cb) = 0;
  // Called by the decoder when it has dequeued a buffer from the CAPTURE queue.
  virtual void OnOutputBufferDequeued(V4L2ReadableBufferRef buf) = 0;
  // Backend can overload this method if it needs to do specific work when
  // the device task is called.
  virtual void OnServiceDeviceTask(bool event) {}
  // Called whenever the V4L2 stream is stopped (|Streamoff| called on either
  // the CAPTURE queue alone or on both queues). |input_queue_stopped| is
  // true if the input queue has been requested to stop.
  virtual void OnStreamStopped(bool input_queue_stopped) = 0;
  // Called when the resolution has been decided, in case the backend needs
  // to do something specific beyond applying these parameters to the CAPTURE
  // queue.
  virtual bool ApplyResolution(const gfx::Size& pic_size,
                               const gfx::Rect& visible_rect) = 0;
  // Called when ChangeResolution is done. |status| indicates whether there is
  // any error occurs during the resolution change.
  virtual void OnChangeResolutionDone(CroStatus status) = 0;
  // Clear all pending decoding tasks and call all pending decode callbacks
  // with |status| as argument.
  virtual void ClearPendingRequests(DecoderStatus status) = 0;
  // Whether we should stop the input queue when changing resolution. Stateless
  // decoders require this, but stateful ones need the input queue to keep
  // running. Although not super elegant, this is required to express that
  // difference.
  virtual bool StopInputQueueOnResChange() const = 0;
  // Returns the amount of OUTPUT queue buffers needed or estimated to be
  // needed by the specific backend.
  virtual size_t GetNumOUTPUTQueueBuffers(bool secure_mode) const = 0;

 protected:
  V4L2VideoDecoderBackend(Client* const client,
                          scoped_refptr<V4L2Device> device);

  // The decoder we are serving. |client_| is the owner of this backend
  // instance, and is guaranteed to live longer than it. Thus it is safe to use
  // a raw pointer here.
  raw_ptr<Client> const client_;
  // V4L2 device to use.
  scoped_refptr<V4L2Device> device_;
  // Input and output queued from which to get buffers.
  scoped_refptr<V4L2Queue> input_queue_;
  scoped_refptr<V4L2Queue> output_queue_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media

#endif  // MEDIA_GPU_V4L2_V4L2_VIDEO_DECODER_BACKEND_H_
