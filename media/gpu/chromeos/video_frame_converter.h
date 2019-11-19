// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CHROMEOS_VIDEO_FRAME_CONVERTER_H_
#define MEDIA_GPU_CHROMEOS_VIDEO_FRAME_CONVERTER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "media/base/video_frame.h"
#include "media/gpu/media_gpu_export.h"

namespace media {

// Video decoders make use of a video frame pool to allocate output frames,
// which are sent to the client after decoding. However, the storage type of the
// allocated frame can be different from what the client expects. This class
// can be used to convert the type of output video frame in this case.
class MEDIA_GPU_EXPORT VideoFrameConverter {
 public:
  using OutputCB = base::RepeatingCallback<void(scoped_refptr<VideoFrame>)>;

  VideoFrameConverter();

  // Initialize the converter. This method must be called before any
  // ConvertFrame() is called.
  void Initialize(scoped_refptr<base::SequencedTaskRunner> parent_task_runner,
                  OutputCB output_cb);

  // Convert the frame and return the converted frame to the client by
  // |output_cb_|. This method must be called on |parent_task_runner_|.
  // The default implementation calls |output_cb_| with |frame| as-is.
  virtual void ConvertFrame(scoped_refptr<VideoFrame> frame);

  // Abort all pending frames. |output_cb_| should not be called for the input
  // frames passed before calling AbortPendingFrames(). This method must be
  // called on |parent_task_runner_|.
  virtual void AbortPendingFrames();

  // Return true if there is any pending frame. This method must be called on
  // |parent_task_runner_|.
  virtual bool HasPendingFrames() const;

 protected:
  // Deletion is only allowed via Destroy().
  virtual ~VideoFrameConverter();

  // The working task runner.
  scoped_refptr<base::SequencedTaskRunner> parent_task_runner_;

  // The callback to return converted frames back to client. This callback will
  // be called on |parent_task_runner_|.
  OutputCB output_cb_;

 private:
  friend struct std::default_delete<VideoFrameConverter>;
  // Called by std::default_delete.
  virtual void Destroy();

  DISALLOW_COPY_AND_ASSIGN(VideoFrameConverter);
};

}  // namespace media

namespace std {

// Specialize std::default_delete to call Destroy().
template <>
struct MEDIA_GPU_EXPORT default_delete<media::VideoFrameConverter> {
  void operator()(media::VideoFrameConverter* ptr) const;
};

}  // namespace std

#endif  // MEDIA_GPU_CHROMEOS_VIDEO_FRAME_CONVERTER_H_
