// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CHROMEOS_IMAGE_PROCESSOR_H_
#define MEDIA_GPU_CHROMEOS_IMAGE_PROCESSOR_H_

#include <stdint.h>

#include <map>
#include <optional>
#include <utility>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/video_frame.h"
#include "media/gpu/chromeos/fourcc.h"
#include "media/gpu/chromeos/frame_resource.h"
#include "media/gpu/chromeos/image_processor_backend.h"
#include "media/gpu/media_gpu_export.h"
#include "ui/gfx/native_pixmap_handle.h"

namespace media {

// An image processor is used to convert from one image format to another (e.g.
// I420 to NV12) while optionally scaling. It is useful in situations where
// a given video hardware (e.g. decoder or encoder) accepts or produces data
// in a format different from what the rest of the pipeline expects.
class MEDIA_GPU_EXPORT ImageProcessor {
 public:
  struct PixelLayoutCandidate {
    Fourcc fourcc;
    gfx::Size size;
    uint64_t modifier = gfx::NativePixmapHandle::kNoModifier;

    // For testing.
    bool operator==(const PixelLayoutCandidate& candidate) const {
      return this->fourcc == candidate.fourcc && this->size == candidate.size &&
             this->modifier == candidate.modifier;
    }
  };

  using PortConfig = ImageProcessorBackend::PortConfig;
  using OutputMode = ImageProcessorBackend::OutputMode;
  using ErrorCB = ImageProcessorBackend::ErrorCB;
  using FrameReadyCB = ImageProcessorBackend::FrameReadyCB;
  using FrameResourceReadyCB = ImageProcessorBackend::FrameResourceReadyCB;
  // Legacy callback is used when allocation mode is ALLOCATE.
  using LegacyFrameResourceReadyCB =
      ImageProcessorBackend::LegacyFrameResourceReadyCB;

  // Callback type for creating a ImageProcessorBackend instance. This allows us
  // to create ImageProcessorBackend instance inside ImageProcessor::Create().
  using CreateBackendCB =
      base::RepeatingCallback<std::unique_ptr<ImageProcessorBackend>(
          const PortConfig& input_config,
          const PortConfig& output_config,
          OutputMode output_mode,
          ErrorCB error_cb)>;

  static std::unique_ptr<ImageProcessor> Create(
      CreateBackendCB create_backend_cb,
      const PortConfig& input_config,
      const PortConfig& output_config,
      OutputMode output_mode,
      ErrorCB error_cb,
      scoped_refptr<base::SequencedTaskRunner> client_task_runner);

  ImageProcessor() = delete;
  ImageProcessor(const ImageProcessor&) = delete;
  ImageProcessor& operator=(const ImageProcessor&) = delete;

  virtual ~ImageProcessor();

  virtual const PortConfig& input_config() const;
  virtual const PortConfig& output_config() const;

  OutputMode output_mode() const { return backend_->output_mode(); }

  std::string backend_type() const { return backend_->type(); }
  // Called by client to process |frame|. The resulting processed frame will be
  // stored in a ImageProcessor-owned output buffer and notified via |cb|. The
  // processor will drop all its references to |frame| after it finishes
  // accessing it.
  // Process() must be called on |client_task_runner_|. This should not be
  // blocking function.
  // TODO(crbug.com/907767): Remove this once ImageProcessor always works as
  // IMPORT mode for output.
  bool Process(scoped_refptr<FrameResource> frame,
               LegacyFrameResourceReadyCB cb);

  // Called by client to process |input_frame| and store in |output_frame|. This
  // can only be used when output mode is IMPORT. The processor will drop all
  // its references to |input_frame| and |output_frame| after it finishes
  // accessing it.
  // Process() must be called on |client_task_runner_|. This should not be
  // blocking function.
  bool Process(scoped_refptr<VideoFrame> input_frame,
               scoped_refptr<VideoFrame> output_frame,
               FrameReadyCB cb);

  // FrameResource version of Process().
  bool Process(scoped_refptr<FrameResource> input_frame,
               scoped_refptr<FrameResource> output_frame,
               FrameResourceReadyCB cb);

  // Reset all processing frames. After this method returns, no more callbacks
  // will be invoked. ImageProcessor is ready to process more frames.
  // Reset() must be called on |client_task_runner_|.
  bool Reset();

  // Returns true if and only if, in IMPORT mode, the image processor requires
  // the output video frames to be CPU-readable with a linear view of the data.
  bool needs_linear_output_buffers() const {
    return needs_linear_output_buffers_;
  }

  // Returns true if the image processor supports buffers allocated
  // incoherently. The MTK MDP3 image processor has coherency issues, but the
  // Libyuv image processor benefits greatly from incoherent allocations.
  // Defaults to false, since only Libyuv has been shown to support this feature
  // so far.
  bool SupportsIncoherentBufs() const {
    return backend_ && backend_->supports_incoherent_buffers();
  }

 protected:
  // Container for FrameReadyCB, FrameResourceReadyCB, and
  // LegacyFrameResourceReadyCB. With this class, we can store any of the
  // callback types in the same container.
  // TODO(crbug.com/907767): Remove LegacyFrameResourceReadyCB once
  // ImageProcessor always works as IMPORT mode for output.
  struct ClientCallback {
    ClientCallback(FrameReadyCB ready_cb);
    ClientCallback(FrameResourceReadyCB frame_resource_ready_cb);
    ClientCallback(LegacyFrameResourceReadyCB legacy_frame_resource_ready_cb);
    ClientCallback(ClientCallback&&);
    ~ClientCallback();

    FrameReadyCB ready_cb;
    FrameResourceReadyCB frame_resource_ready_cb;
    LegacyFrameResourceReadyCB legacy_frame_resource_ready_cb;
  };

  ImageProcessor(std::unique_ptr<ImageProcessorBackend> backend,
                 scoped_refptr<base::SequencedTaskRunner> client_task_runner,
                 scoped_refptr<base::SequencedTaskRunner> backend_task_runner);

  // Callbacks of processing frames.
  static void OnProcessDoneThunk(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      std::optional<base::WeakPtr<ImageProcessor>> weak_this,
      int cb_index,
      scoped_refptr<VideoFrame> frame);
  static void OnProcessFrameResourceDoneThunk(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      std::optional<base::WeakPtr<ImageProcessor>> weak_this,
      int cb_index,
      scoped_refptr<FrameResource> frame);
  static void OnProcessFrameResourceLegacyDoneThunk(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      std::optional<base::WeakPtr<ImageProcessor>> weak_this,
      int cb_index,
      size_t buffer_id,
      scoped_refptr<FrameResource> frame);
  void OnProcessDone(int cb_index, scoped_refptr<VideoFrame> frame);
  void OnProcessFrameResourceDone(int cb_index,
                                  scoped_refptr<FrameResource> frame);
  void OnProcessFrameResourceLegacyDone(int cb_index,
                                        size_t buffer_id,
                                        scoped_refptr<FrameResource> frame);

  // Store |cb| at |pending_cbs_| and return a index for the callback.
  int StoreCallback(ClientCallback cb);

  // The backend of image processor. All of its methods are called on
  // |backend_task_runner_|.
  std::unique_ptr<ImageProcessorBackend> backend_;

  // Store the callbacks from the client, accessed on |client_task_runner_|.
  // This storage is to guarantee the callbacks are called or destroyed on the
  // correct sequence.
  std::map<int /* cb_index */, ClientCallback /* cb */> pending_cbs_;
  // The index of next callback.
  int next_cb_index_ = 0;

  // The sequence and its checker for interacting with the client.
  scoped_refptr<base::SequencedTaskRunner> client_task_runner_;
  SEQUENCE_CHECKER(client_sequence_checker_);

  // The sequence for interacting with |backend_|.
  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;

  const bool needs_linear_output_buffers_;

  // The weak pointer of this, bound to |client_task_runner_|.
  base::WeakPtr<ImageProcessor> weak_this_;
  base::WeakPtrFactory<ImageProcessor> weak_this_factory_{this};
};

}  // namespace media

#endif  // MEDIA_GPU_CHROMEOS_IMAGE_PROCESSOR_H_
