// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_VAAPI_IMAGE_PROCESSOR_H_
#define MEDIA_GPU_VAAPI_VAAPI_IMAGE_PROCESSOR_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/task/cancelable_task_tracker.h"
#include "build/build_config.h"
#include "media/gpu/chromeos/image_processor.h"

namespace media {

class VaapiWrapper;

// ImageProcessor that is hardware accelerated with VA-API. This ImageProcessor
// supports DmaBuf only for both input and output.
class VaapiImageProcessor : public ImageProcessor {
 public:
  // Factory method to create VaapiImageProcessor for a buffer conversion
  // specified by |input_config| and |output_config|. Provided |error_cb| will
  // be posted to the same thread that executes Create(), if an error occurs
  // after initialization.
  // Returns nullptr if it fails to create VaapiImageProcessor.
  static std::unique_ptr<VaapiImageProcessor> Create(
      const ImageProcessor::PortConfig& input_config,
      const ImageProcessor::PortConfig& output_config,
      const std::vector<ImageProcessor::OutputMode>& preferred_output_modes,
      const base::RepeatingClosure& error_cb);

  // ImageProcessor implementation.
  ~VaapiImageProcessor() override;
  bool Reset() override;

 private:
  VaapiImageProcessor(const ImageProcessor::PortConfig& input_config,
                      const ImageProcessor::PortConfig& output_config,
                      scoped_refptr<VaapiWrapper> vaapi_wrapper);

  // ImageProcessor implementation.
  bool ProcessInternal(scoped_refptr<VideoFrame> input_frame,
                       scoped_refptr<VideoFrame> output_frame,
                       FrameReadyCB cb) override;

  // Sequence task runner in which the buffer conversion is performed.
  const scoped_refptr<base::SequencedTaskRunner> processor_task_runner_;

  // CancelableTaskTracker for posted tasks to |processor_task_runner_|. We
  // can't use CancelableCallback or WeakPtr because we may need to cancel tasks
  // from outside |processor_task_runner_|.
  base::CancelableTaskTracker process_task_tracker_;

  const scoped_refptr<VaapiWrapper> vaapi_wrapper_;

  SEQUENCE_CHECKER(client_sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(VaapiImageProcessor);
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VAAPI_IMAGE_PROCESSOR_H_
