// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_MAC_VIDEO_TOOLBOX_FRAME_CONVERTER_H_
#define MEDIA_GPU_MAC_VIDEO_TOOLBOX_FRAME_CONVERTER_H_

#include <CoreMedia/CoreMedia.h>

#include <memory>

#include "base/apple/scoped_cftyperef.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/service/sequence_id.h"
#include "gpu/ipc/service/command_buffer_stub.h"
#include "media/base/video_frame.h"

namespace gpu {
class SharedImageStub;
struct SyncToken;
}  // namespace gpu

namespace media {

class MediaLog;
struct VideoToolboxDecodeMetadata;

// Converts IOSurface-backed CVImageBuffers to VideoFrames.
class VideoToolboxFrameConverter
    : public gpu::CommandBufferStub::DestructionObserver,
      public base::RefCountedDeleteOnSequence<VideoToolboxFrameConverter> {
 public:
  using GetCommandBufferStubCB =
      base::RepeatingCallback<gpu::CommandBufferStub*()>;
  using OutputCB =
      base::OnceCallback<void(scoped_refptr<VideoFrame>,
                              std::unique_ptr<VideoToolboxDecodeMetadata>)>;

  // `gpu_task_runner` is the task runner on which |this| operates, which must
  // match the task runner used by the stub returned from |get_stub_cb|
  // (presumably the GPU main thread task runner). Construction can occur on
  // any sequence, but Convert() must be called on `gpu_task_runner`.
  VideoToolboxFrameConverter(
      scoped_refptr<base::SequencedTaskRunner> gpu_task_runner,
      std::unique_ptr<MediaLog> media_log,
      GetCommandBufferStubCB get_stub_cb);

  void Convert(base::apple::ScopedCFTypeRef<CVImageBufferRef> image,
               std::unique_ptr<VideoToolboxDecodeMetadata> metadata,
               OutputCB output_cb);

 private:
  // base::RefCountedDeleteOnSequence implementation.
  friend class base::DeleteHelper<VideoToolboxFrameConverter>;
  friend class base::RefCountedDeleteOnSequence<VideoToolboxFrameConverter>;
  ~VideoToolboxFrameConverter() override;

  // gpu::CommandBufferStub::DestructionObserver implementation.
  void OnWillDestroyStub(bool have_context) override;

  void Initialize();
  void DestroyStub();

  void OnVideoFrameReleased(
      scoped_refptr<gpu::ClientSharedImage> client_shared_image,
      base::apple::ScopedCFTypeRef<CVImageBufferRef> image,
      const gpu::SyncToken& sync_token);

  scoped_refptr<base::SequencedTaskRunner> gpu_task_runner_;
  std::unique_ptr<MediaLog> media_log_;
  GetCommandBufferStubCB get_stub_cb_;

  bool initialized_ = false;
  raw_ptr<gpu::CommandBufferStub> stub_ = nullptr;
  gpu::SequenceId wait_sequence_id_;
  raw_ptr<gpu::SharedImageStub> sis_ = nullptr;
};

}  // namespace media

#endif  // MEDIA_GPU_MAC_VIDEO_TOOLBOX_FRAME_CONVERTER_H_
