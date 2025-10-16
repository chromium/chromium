// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_VAAPI_IMAGE_DECODE_ACCELERATOR_WORKER_H_
#define MEDIA_GPU_VAAPI_VAAPI_IMAGE_DECODE_ACCELERATOR_WORKER_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/containers/small_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "gpu/config/gpu_info.h"
#include "gpu/ipc/service/image_decode_accelerator_worker.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace base {
class SequencedTaskRunner;
}

namespace media {

class VaapiImageDecoder;

using VaapiImageDecoderVector = std::vector<std::unique_ptr<VaapiImageDecoder>>;

using VaapiImageDecoderMap =
    base::small_map<absl::flat_hash_map<gpu::ImageDecodeAcceleratorType,
                                        std::unique_ptr<VaapiImageDecoder>>>;

// This class uses the VAAPI to provide image decode acceleration. The
// interaction with the VAAPI is done on |decoder_task_runner_|.
class VaapiImageDecodeAcceleratorWorker
    : public gpu::ImageDecodeAcceleratorWorker {
 public:
  // Creates a VaapiImageDecodeAcceleratorWorker. Returns nullptr if no image
  // decode profiles are supported.
  static std::unique_ptr<VaapiImageDecodeAcceleratorWorker> Create();

  VaapiImageDecodeAcceleratorWorker(const VaapiImageDecodeAcceleratorWorker&) =
      delete;
  VaapiImageDecodeAcceleratorWorker& operator=(
      const VaapiImageDecodeAcceleratorWorker&) = delete;

  ~VaapiImageDecodeAcceleratorWorker() override;

  // gpu::ImageDecodeAcceleratorWorker implementation.
  gpu::ImageDecodeAcceleratorSupportedProfiles GetSupportedProfiles() override;

 private:
  friend struct std::default_delete<VaapiImageDecodeAcceleratorWorker>;

  friend class VaapiImageDecodeAcceleratorWorkerTest;

  VaapiImageDecodeAcceleratorWorker(
      VaapiImageDecoderVector decoders,
      gpu::ImageDecodeAcceleratorSupportedProfiles supported_profiles);

  // Calls the destructor of the VaapiImageDecodeAcceleratorWorker instance from
  // the |decoder_task_runner_|.
  void Destroy();

  // We delegate the decoding to the appropriate decoder in |decoders_| which
  // are used and destroyed on |decoder_task_runner_|.
  VaapiImageDecoderMap decoders_ GUARDED_BY_CONTEXT(decoder_sequence_checker_);
  const gpu::ImageDecodeAcceleratorSupportedProfiles supported_profiles_;

  scoped_refptr<base::SequencedTaskRunner> decoder_task_runner_;
  SEQUENCE_CHECKER(decoder_sequence_checker_);

  SEQUENCE_CHECKER(main_sequence_checker_);
  SEQUENCE_CHECKER(io_sequence_checker_);

  // WeakPtr factory for *|this| to post tasks to |decoder_task_runner_|.
  base::WeakPtrFactory<VaapiImageDecodeAcceleratorWorker>
      decoder_weak_this_factory_{this};
};

}  // namespace media

namespace std {

// Specializes std::default_delete to call Destroy().
template <>
struct default_delete<media::VaapiImageDecodeAcceleratorWorker> {
  void operator()(media::VaapiImageDecodeAcceleratorWorker* ptr) const;
};

}  // namespace std

#endif  // MEDIA_GPU_VAAPI_VAAPI_IMAGE_DECODE_ACCELERATOR_WORKER_H_
