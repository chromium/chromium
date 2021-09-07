// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CHROMEOS_IMAGE_PROCESSOR_FACTORY_H_
#define MEDIA_GPU_CHROMEOS_IMAGE_PROCESSOR_FACTORY_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "media/gpu/chromeos/fourcc.h"
#include "media/gpu/chromeos/image_processor.h"
#include "media/gpu/media_gpu_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/size.h"

namespace media {

class MEDIA_GPU_EXPORT ImageProcessorFactory {
 public:
  // Callback to pick a valid format from the given |candidates| formats giving
  // preference to |preferred_fourcc| if provided.
  using PickFormatCB = base::RepeatingCallback<absl::optional<Fourcc>(
      const std::vector<Fourcc>& /* candidates */,
      absl::optional<Fourcc> /* preferred_fourcc */)>;

  // Factory method to create ImageProcessor.
  // Given input and output PortConfig, it tries to find out the most suitable
  // ImageProcessor to be used for the current platform.
  //
  // For |preferred_output_modes|, it tries instantiate an ImageProcessor class
  // with an output mode in sequence. With ImageProcessor subclass and output
  // mode selected, the subclass' factory method will pick the first supported
  // input and output storage type from input and output PortConfig,
  // respectively. It has an assumption that the selection of output mode is
  // independent from the selection of output storage type.
  //
  // TODO(crbug.com/907767): Remove |preferred_output_modes| once ImageProcessor
  // only accepts IMPORT output mode.
  //
  // Args:
  //   input_config: input PortConfig.
  //   output_config: output PortConfig.
  //   preferred_output_modes: list of preferred output modes.
  //   num_buffers: number of input and output buffers.
  //   error_cb: Callback when error occurs.
  //
  // Returns:
  //   Most suitable ImageProcessor instance. nullptr if no ImageProcessor
  //   is available for given parameters on current platform.
  static std::unique_ptr<ImageProcessor> Create(
      const ImageProcessor::PortConfig& input_config,
      const ImageProcessor::PortConfig& output_config,
      const std::vector<ImageProcessor::OutputMode>& preferred_output_modes,
      size_t num_buffers,
      VideoRotation relative_rotation,
      scoped_refptr<base::SequencedTaskRunner> client_task_runner,
      ImageProcessor::ErrorCB error_cb);

  // Factory method to create an ImageProcessor.
  // Unlike Create(), the caller passes a list of supported inputs,
  // |input_candidates|. It also passes the |input_visible_rect| and the desired
  // |output_size|. |out_format_picker| allows us to negotiate the output
  // format: we'll call it with a list of supported formats and (possibly) a
  // preferred one and the callback picks one. With the rest of the parameters
  // the factory can instantiate a suitable ImageProcessor. Returns nullptr if
  // an ImageProcessor can't be created.
  static std::unique_ptr<ImageProcessor> CreateWithInputCandidates(
      const std::vector<std::pair<Fourcc, gfx::Size>>& input_candidates,
      const gfx::Rect& input_visible_rect,
      const gfx::Size& output_size,
      size_t num_buffers,
      scoped_refptr<base::SequencedTaskRunner> client_task_runner,
      PickFormatCB out_format_picker,
      ImageProcessor::ErrorCB error_cb);

  DISALLOW_IMPLICIT_CONSTRUCTORS(ImageProcessorFactory);
};

}  // namespace media

#endif  // MEDIA_GPU_CHROMEOS_IMAGE_PROCESSOR_FACTORY_H_
