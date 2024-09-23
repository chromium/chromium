// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_EFFECTS_PROCESSOR_H_
#define MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_EFFECTS_PROCESSOR_H_

#include <optional>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/trace_id_helper.h"
#include "base/types/expected.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "media/capture/capture_export.h"
#include "media/capture/mojom/video_capture_buffer.mojom-forward.h"
#include "media/capture/video/video_capture_device.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_effects/public/mojom/video_effects_processor.mojom.h"

namespace media {

// Structure used to pass information about a post-processed video frame.
// Equivalent to `std::pair<VideoCaptureDevice::Client::Buffer,
// mojom::VideoFrameInfoPtr>`.
struct CAPTURE_EXPORT PostProcessDoneInfo {
  // Note: style-guide allows us to use `explicit` keyword even for
  // multi-parameter constructors.
  explicit PostProcessDoneInfo(VideoCaptureDevice::Client::Buffer buffer,
                               mojom::VideoFrameInfoPtr info);

  PostProcessDoneInfo(PostProcessDoneInfo& other) = delete;
  PostProcessDoneInfo& operator=(const PostProcessDoneInfo& other) = delete;

  PostProcessDoneInfo(PostProcessDoneInfo&& other);
  PostProcessDoneInfo& operator=(PostProcessDoneInfo&& other);

  ~PostProcessDoneInfo();

  VideoCaptureDevice::Client::Buffer buffer;
  mojom::VideoFrameInfoPtr info;
};

// Companion class of `VideoCaptureDeviceClient`, used when video effects are
// enabled. Responsible for marshaling video frame data so that it's suitable
// for an IPC to Video Effects Processor, and for actually invoking the
// processor.
class CAPTURE_EXPORT VideoCaptureEffectsProcessor {
 public:
  // `video_effects_processor` remote will be used to apply the video effects.
  explicit VideoCaptureEffectsProcessor(
      mojo::PendingRemote<video_effects::mojom::VideoEffectsProcessor>
          video_effects_processor);

  VideoCaptureEffectsProcessor(const VideoCaptureEffectsProcessor& other) =
      delete;
  VideoCaptureEffectsProcessor& operator=(
      const VideoCaptureEffectsProcessor& other) = delete;

  VideoCaptureEffectsProcessor(VideoCaptureEffectsProcessor&& other) = delete;
  VideoCaptureEffectsProcessor& operator=(
      VideoCaptureEffectsProcessor&& other) = delete;

  ~VideoCaptureEffectsProcessor();

  // Callback that will be called once the post-processor has completed
  // processing a single frame. On success, the buffer will contain the
  // processed frame (that was allocated by the client of the post-processor),
  // and it is the same buffer that the client passed in to one of the
  // PostProcess methods. The video frame info describes the video frame
  // residing now in the buffer.
  using PostProcessDoneCallback = base::OnceCallback<void(
      base::expected<PostProcessDoneInfo,
                     video_effects::mojom::PostProcessError>)>;

  // On-CPU variant. Marshals `data` into a shared memory buffer and prepares
  // the shared images backed by `out_buffer` for receiving the processing
  // results. Invokes the processor.
  void PostProcessData(
      base::span<const uint8_t> data,
      mojom::VideoFrameInfoPtr frame_info,
      VideoCaptureDevice::Client::Buffer out_buffer,
      const VideoCaptureFormat& out_buffer_format,
      VideoCaptureBufferType out_buffer_type,
      VideoCaptureEffectsProcessor::PostProcessDoneCallback post_process_cb);

  // On-GPU variant. Creates shared images backed by `in_buffer` and
  // `out_buffer`. Invokes the processor.
  void PostProcessBuffer(
      VideoCaptureDevice::Client::Buffer in_buffer,
      mojom::VideoFrameInfoPtr frame_info,
      VideoCaptureBufferType in_buffer_type,
      VideoCaptureDevice::Client::Buffer out_buffer,
      const VideoCaptureFormat& out_buffer_format,
      VideoCaptureBufferType out_buffer_type,
      VideoCaptureEffectsProcessor::PostProcessDoneCallback post_process_cb);

 private:
  struct PostProcessContext {
    // Creates the context. If `in_buffer` is set, then `in_shared_image` must
    // also be set for buffers that had shared images created from them. Same
    // requirement applies for `out_buffer` and `out_shared_image`. If we don't
    // maintain the ownership of shared images backed by the buffers, the dtors
    // of `gpu::ClientSharedImage` will be invoked and the shared images won't
    // be visible on the other side of the IPC, despite being exported for IPC.
    PostProcessContext(
        std::optional<VideoCaptureDevice::Client::Buffer> in_buffer,
        scoped_refptr<gpu::ClientSharedImage> in_shared_image,
        VideoCaptureDevice::Client::Buffer out_buffer,
        scoped_refptr<gpu::ClientSharedImage> out_shared_image,
        VideoCaptureEffectsProcessor::PostProcessDoneCallback post_process_cb);
    ~PostProcessContext();

    PostProcessContext(const PostProcessContext& other) = delete;
    PostProcessContext& operator=(const PostProcessContext& other) = delete;

    // Note: the class is move-constructible but not move-assignable!
    // This is a consequence of `trace_id` being const.
    PostProcessContext(PostProcessContext&& other);
    PostProcessContext& operator=(PostProcessContext&& other) = delete;

    const uint64_t trace_id = base::trace_event::GetNextGlobalTraceId();

    // May be std::nullopt if the context was created for a post-process request
    // that operates on on-CPU data - we won't have an `in_buffer` in this case.
    std::optional<VideoCaptureDevice::Client::Buffer> in_buffer;
    // May be null if `in_buffer` is not set.
    scoped_refptr<gpu::ClientSharedImage> in_shared_image;

    VideoCaptureDevice::Client::Buffer out_buffer;
    scoped_refptr<gpu::ClientSharedImage> out_shared_image;
    VideoCaptureEffectsProcessor::PostProcessDoneCallback post_process_cb;
  };

  void PostProcessDataOnValidSequence(
      PostProcessContext context,
      mojom::VideoBufferHandlePtr in_buffer_handle,
      mojom::VideoFrameInfoPtr frame_info,
      mojom::VideoBufferHandlePtr out_buffer_handle,
      const VideoCaptureFormat& out_buffer_format);

  void OnPostProcess(PostProcessContext context,
                     video_effects::mojom::PostProcessResultPtr result);

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  mojo::Remote<video_effects::mojom::VideoEffectsProcessor> effects_processor_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);

  // Must be last:
  base::WeakPtrFactory<VideoCaptureEffectsProcessor> weak_ptr_factory_{this};
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_EFFECTS_PROCESSOR_H_
