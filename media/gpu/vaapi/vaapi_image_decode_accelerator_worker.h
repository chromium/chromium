// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_VAAPI_IMAGE_DECODE_ACCELERATOR_WORKER_H_
#define MEDIA_GPU_VAAPI_VAAPI_IMAGE_DECODE_ACCELERATOR_WORKER_H_

#include <stdint.h>

#include <memory>
#include <unordered_map>
#include <vector>

#include "base/containers/small_map.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "gpu/config/gpu_info.h"
#include "gpu/ipc/service/image_decode_accelerator_worker.h"

namespace base {
class SequencedTaskRunner;
}

namespace gfx {
class Size;
}

namespace media {

class VaapiImageDecoder;

using VaapiImageDecoderVector = std::vector<std::unique_ptr<VaapiImageDecoder>>;

using VaapiImageDecoderMap =
    base::small_map<std::unordered_map<gpu::ImageDecodeAcceleratorType,
                                       std::unique_ptr<VaapiImageDecoder>>>;

// This class uses the VAAPI to provide image decode acceleration. The
// interaction with the VAAPI is done on |decoder_task_runner_|.
class VaapiImageDecodeAcceleratorWorker
    : public gpu::ImageDecodeAcceleratorWorker {
 public:
  // Creates a VaapiImageDecodeAcceleratorWorker and attempts to initialize the
  // internal state. Returns nullptr if initialization fails.
  static std::unique_ptr<VaapiImageDecodeAcceleratorWorker> Create();

  ~VaapiImageDecodeAcceleratorWorker() override;

  // gpu::ImageDecodeAcceleratorWorker implementation.
  gpu::ImageDecodeAcceleratorSupportedProfiles GetSupportedProfiles() override;
  void Decode(std::vector<uint8_t> encoded_data,
              const gfx::Size& output_size,
              CompletedDecodeCB decode_cb) override;

 private:
  friend class VaapiImageDecodeAcceleratorWorkerTest;

  explicit VaapiImageDecodeAcceleratorWorker(VaapiImageDecoderVector decoders);

  VaapiImageDecoder* GetDecoderForImage(
      const std::vector<uint8_t>& encoded_data);

  // We delegate the decoding to the appropriate decoder in |decoders_| which
  // are used and destroyed on |decoder_task_runner_|.
  VaapiImageDecoderMap decoders_;
  gpu::ImageDecodeAcceleratorSupportedProfiles supported_profiles_;
  scoped_refptr<base::SequencedTaskRunner> decoder_task_runner_;

  SEQUENCE_CHECKER(main_sequence_checker_);
  SEQUENCE_CHECKER(io_sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(VaapiImageDecodeAcceleratorWorker);
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VAAPI_IMAGE_DECODE_ACCELERATOR_WORKER_H_
