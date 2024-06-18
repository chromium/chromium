// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CHROMEOS_FRAME_RESOURCE_CONVERTER_H_
#define MEDIA_GPU_CHROMEOS_FRAME_RESOURCE_CONVERTER_H_

#include "base/memory/scoped_refptr.h"
#include "ui/gfx/generic_shared_memory_id.h"

namespace base {
class Location;
class SequencedTaskRunner;
}  // namespace base

namespace media {

class FrameResource;

// This interface is at the end of VideoDecoderPipeline to convert a frame's
// storage type.
//
// A FrameResourceConverter is expected to be used as follows:
//
// 1) It can be constructed on any sequence. The first method call after
//    construction must be Initialize() which can happen on any sequence and
//    should only be called once.
//
// 2) Other methods must be called on the |parent_task_runner| passed to
//    Initialize().
//
// 3) Destruction must occur through
//    std::default_delete<FrameResourceConverter> on that same
//    |parent_task_runner| (unless Initialize() was never called, in which case,
//    the destruction can occur on any sequence).
class FrameResourceConverter {
 public:
  // TODO(nhebert): change the output type to VideoFrame.
  using OutputCB = base::RepeatingCallback<void(scoped_refptr<FrameResource>)>;
  using GetOriginalFrameCB = base::RepeatingCallback<FrameResource*(
      gfx::GenericSharedMemoryId frame_id)>;

  FrameResourceConverter();
  // A |FrameResourceConverter| is not copyable or moveable.
  FrameResourceConverter(const FrameResourceConverter&) = delete;
  FrameResourceConverter(FrameResourceConverter&&) = delete;
  FrameResourceConverter& operator=(const FrameResourceConverter&) = delete;
  FrameResourceConverter& operator=(FrameResourceConverter&&) = delete;

  // Initializes the converter. This method must be called before any other
  // methods. Each of the public methods of the FrameResourceConverter interface
  // performs to ensure they are called on |parent_task_runner|.
  void Initialize(scoped_refptr<base::SequencedTaskRunner> parent_task_runner,
                  OutputCB output_cb);

  // Converts the storage type of |frame|. Conversion may happen asynchronously
  // or synchronously. Implementers should guarantee that generated frames do
  // not rely on the incoming frame being kept alive by the caller. I.e. it must
  // not be invalidated by the client dropping a reference to |frame|.
  void ConvertFrame(scoped_refptr<FrameResource> frame);

  // For implementations that queue frames to convert, these interfaces provide
  // a way to abort or check the status of the queue.
  void AbortPendingFrames();
  bool HasPendingFrames() const;

  // Sets the callback to unwrap FrameResources provided to ConvertFrame(). If
  // |get_original_frame_cb| is null or this method is never called at all,
  // ConvertFrame() assumes it's called with unwrapped FrameResources.
  //
  // If used, |get_original_frame_cb| will only be called during a call to
  // ConvertVideoFrame().
  //
  // Note: if |get_original_frame_cb| is called at all, it will be called on
  // |parent_task_runner_|.
  void set_get_original_frame_cb(GetOriginalFrameCB get_original_frame_cb);

 protected:
  virtual ~FrameResourceConverter();

  // Invoked when any error occurs. |msg| is the error message. If a derived
  // class uses pending frames, it should call AbortPendingFrames before calling
  // FrameResourceConverter::OnError().
  virtual void OnError(const base::Location& location, const std::string& msg);

  // In DmabufVideoFramePool and OOPVideoDecoder, we recycle the unused frames.
  // This is done a bit differently for each case:
  //
  // - For DmabufVideoFramePool: each time a frame is requested from the pool it
  //   is wrapped inside another frame. A destruction callback is then added to
  //   this wrapped frame to automatically return it to the pool upon
  //   destruction.
  //
  // - For OOPVideoDecoder: each time we receive a frame from the remote
  //   decoder, we look it up in a cache of known, previously received buffers
  //   (or insert it into this cache if it's a new buffer). We wrap the known or
  //   new frame inside another frame. A destruction callback is then added to
  //   this wrapped frame to automatically notify the remote decoder that it can
  //   re-use the underlying buffer upon destruction.
  //
  // Unfortunately this means that a new frame is returned each time (i.e., we
  // receive a new FrameResource::unique_id() each time). Some implementations
  // need a way to uniquely identify the underlying frame to avoid converting
  // the same frame multiple times. GetOriginalFrame() is used to get the
  // original frame.
  //
  // This must be called on |parent_task_runner_|.
  FrameResource* GetOriginalFrame(FrameResource& frame) const;

  const scoped_refptr<base::SequencedTaskRunner>& parent_task_runner();

  // This must be called on |parent_task_runner_|.
  void Output(scoped_refptr<FrameResource> frame) const;

 private:
  friend struct std::default_delete<FrameResourceConverter>;

  virtual void Destroy();

  // The *Impl methods are run by their public counterparts after sequence
  // validation is run. ConvertFrameImpl() is the private implementation for
  // ConvertFrame(). Similarly for AbortPendingFramesImpl() and
  // HasPendingFramesImpl().
  virtual void ConvertFrameImpl(scoped_refptr<FrameResource> frame) = 0;

  // Derived classes should override these if there is a need to asynchronously
  // convert frames. The base implementations assume that there will never be
  // a pending frame.
  virtual void AbortPendingFramesImpl();
  virtual bool HasPendingFramesImpl() const;

  // |get_original_frame_cb_| is used by GetOriginalFrame() to get the original
  // frame.
  //
  // When |get_original_frame_cb_| is null, we assume it's not necessary to get
  // the original frames, and we just use them directly.
  //
  // TODO(b/195769334): remove the null |get_original_frame_cb_| path because it
  // shouldn't be used after https://crrev.com/c/4457504.
  GetOriginalFrameCB get_original_frame_cb_;

  // The working task runner. Set by Initialize().
  scoped_refptr<base::SequencedTaskRunner> parent_task_runner_;

  // |output_cb| is called on the |parent_task_runner| to output converted
  // frames or to indicate an error (in which case, |output_cb| is called with a
  // nullptr).
  OutputCB output_cb_;
};

}  // namespace media

namespace std {

// Specialize std::default_delete to call Destroy().
template <>
struct default_delete<media::FrameResourceConverter> {
  constexpr default_delete() = default;

  template <
      typename U,
      typename = typename std::enable_if<
          std::is_convertible<U*, media::FrameResourceConverter*>::value>::type>
  explicit default_delete(const default_delete<U>& d) {}

  void operator()(media::FrameResourceConverter* ptr) const;
};

}  // namespace std

#endif  // MEDIA_GPU_CHROMEOS_FRAME_RESOURCE_CONVERTER_H_
