// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CHROMEOS_VDA_VIDEO_FRAME_POOL_H_
#define MEDIA_GPU_CHROMEOS_VDA_VIDEO_FRAME_POOL_H_

#include <optional>

#include "base/containers/queue.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "media/gpu/chromeos/chromeos_status.h"
#include "media/gpu/chromeos/dmabuf_video_frame_pool.h"
#include "media/gpu/chromeos/fourcc.h"
#include "media/gpu/chromeos/frame_resource.h"

namespace base {
class WaitableEvent;
}

namespace media {

class GpuBufferLayout;

// This class is used by VdVideoDecodeAccelerator, which adapts
// VideoDecodeAccelerator to VideoDecoder interface.
// The mission is to allocate and manage DMA-buf FrameResource by delegating the
// requests of buffer allocation to a VideoDecodeAccelerator instance, and
// provide FrameResource to the VideoDecoder instance.
// The communication with VdVideoDecodeAccelerator, which inherits
// VdaDelegate, is executed on |vda_task_runner_|, and the communication with
// VideoDecoder instance is on |parent_task_runner_|.
class VdaVideoFramePool : public DmabufVideoFramePool {
 public:
  class VdaDelegate {
   public:
    // Callback for returning the layout of requested buffer.
    using NotifyLayoutChangedCb =
        base::OnceCallback<void(CroStatus::Or<GpuBufferLayout>)>;
    // Callback for importing available frames to this pool.
    using ImportFrameCb =
        base::RepeatingCallback<void(scoped_refptr<FrameResource>)>;

    // Request new frames from VDA's client. VdaDelegate has to return the
    // layout of frames by calling |notify_layout_changed_cb|.
    // After that, VdaDelegate should pass frames by calling
    // |import_frame_cb|.
    // Note: RequestFrames(), |notify_layout_changed_cb|, and |import_frame_cb|
    // should be called on VdaVideoFramePool's |vda_task_runner_|.
    virtual void RequestFrames(const Fourcc& fourcc,
                               const gfx::Size& coded_size,
                               const gfx::Rect& visible_rect,
                               size_t max_num_frames,
                               NotifyLayoutChangedCb notify_layout_changed_cb,
                               ImportFrameCb import_frame_cb) = 0;
    virtual VideoFrame::StorageType GetFrameStorageType() const = 0;
  };

  VdaVideoFramePool(base::WeakPtr<VdaDelegate> vda,
                    scoped_refptr<base::SequencedTaskRunner> vda_task_runner);
  ~VdaVideoFramePool() override;

  // DmabufVideoFramePool implementation.
  CroStatus::Or<GpuBufferLayout> Initialize(const Fourcc& fourcc,
                                            const gfx::Size& coded_size,
                                            const gfx::Rect& visible_rect,
                                            const gfx::Size& natural_size,
                                            size_t max_num_frames,
                                            bool use_protected,
                                            bool use_linear_buffers) override;
  scoped_refptr<FrameResource> GetFrame() override;
  VideoFrame::StorageType GetFrameStorageType() const override;
  bool IsExhausted() override;
  void NotifyWhenFrameAvailable(base::OnceClosure cb) override;
  void ReleaseAllFrames() override;
  std::optional<GpuBufferLayout> GetGpuBufferLayout() override;

 private:
  // Update the layout of the buffers. |vda_| calls this as
  // NotifyLayoutChangedCb.
  static void OnRequestFramesDone(base::WaitableEvent* done,
                                  CroStatus::Or<GpuBufferLayout>* layout,
                                  CroStatus::Or<GpuBufferLayout> layout_value);

  // Thunk to post ImportFrame() to |task_runner|.
  // Because this thunk may be called in any thread, We don't want to
  // dereference WeakPtr. Therefore we wrap the WeakPtr by std::optional to
  // avoid the task runner defererencing the WeakPtr.
  static void ImportFrameThunk(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      std::optional<base::WeakPtr<VdaVideoFramePool>> weak_this,
      scoped_refptr<FrameResource> frame);
  // Import an available frame.
  void ImportFrame(scoped_refptr<FrameResource> frame);

  // Call |frame_available_cb_| when the pool is not exhausted.
  void CallFrameAvailableCbIfNeeded();

  // WeakPtr of VdaDelegate instance, bound at |vda_task_runner_|.
  base::WeakPtr<VdaDelegate> vda_;
  // Task runner that interacts with VdaDelegate. All VdaDelegate's methods
  // and their callbacks should be called on this task runner.
  // Note: DmabufVideoFrame's public methods like Initialize() and GetFrame()
  // should be called on |parent_task_runner_|.
  scoped_refptr<base::SequencedTaskRunner> vda_task_runner_;

  // The storage type of frames imported by |vda_|. This is set at
  // construction time by calling |vda_->GetFrameStorageType()|.
  const VideoFrame::StorageType vda_frame_storage_type_;

  // The callback which is called when the pool is not exhausted.
  base::OnceClosure frame_available_cb_;

  // The layout of the frames in |frame_pool_|.
  std::optional<GpuBufferLayout> layout_;

  // Data passed from Initialize().
  size_t max_num_frames_ = 0;
  std::optional<Fourcc> fourcc_;
  gfx::Size coded_size_;
  gfx::Rect visible_rect_;
  gfx::Size natural_size_;

  base::queue<scoped_refptr<FrameResource>> frame_pool_;

  // Sequence checker for |parent_task_runner_|.
  SEQUENCE_CHECKER(parent_sequence_checker_);

  // The weak pointer of this, bound at |parent_task_runner_|.
  base::WeakPtr<VdaVideoFramePool> weak_this_;
  base::WeakPtrFactory<VdaVideoFramePool> weak_this_factory_{this};
};

}  // namespace media

#endif  // MEDIA_GPU_CHROMEOS_VDA_VIDEO_FRAME_POOL_H_
