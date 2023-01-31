// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_VIDEO_ENCODE_ACCELERATOR_PROVIDER_H_
#define MEDIA_MOJO_SERVICES_MOJO_VIDEO_ENCODE_ACCELERATOR_PROVIDER_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_refptr.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_preferences.h"
#include "media/mojo/mojom/video_encode_accelerator.mojom.h"
#include "media/mojo/services/media_mojo_export.h"
#include "media/mojo/services/mojo_video_encode_accelerator_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace base {
class SequencedTaskRunner;
}

namespace gpu {
struct GpuPreferences;
class GpuDriverBugWorkarounds;
}  // namespace gpu

namespace media {

// This class implements the interface mojom::VideoEncodeAcceleratorProvider,
// holds on to the necessary objects to create mojom::VideoEncodeAccelerators.
class MEDIA_MOJO_EXPORT MojoVideoEncodeAcceleratorProvider
    : public mojom::VideoEncodeAcceleratorProvider {
 public:
  using CreateAndInitializeVideoEncodeAcceleratorCallback =
      base::RepeatingCallback<std::unique_ptr<::media::VideoEncodeAccelerator>(
          const ::media::VideoEncodeAccelerator::Config& config,
          VideoEncodeAccelerator::Client* client,
          const gpu::GpuPreferences& gpu_preferences,
          const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
          const gpu::GPUInfo::GPUDevice& gpu_device,
          std::unique_ptr<MediaLog> media_log)>;

  static void Create(
      mojo::PendingReceiver<mojom::VideoEncodeAcceleratorProvider> receiver,
      CreateAndInitializeVideoEncodeAcceleratorCallback create_vea_callback,
      const gpu::GpuPreferences& gpu_preferences,
      const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
      const gpu::GPUInfo::GPUDevice& gpu_device,
      scoped_refptr<base::SequencedTaskRunner> runner);

  MojoVideoEncodeAcceleratorProvider(
      CreateAndInitializeVideoEncodeAcceleratorCallback create_vea_callback,
      const gpu::GpuPreferences& gpu_preferences,
      const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
      const gpu::GPUInfo::GPUDevice& gpu_device);

  MojoVideoEncodeAcceleratorProvider(
      const MojoVideoEncodeAcceleratorProvider&) = delete;
  MojoVideoEncodeAcceleratorProvider& operator=(
      const MojoVideoEncodeAcceleratorProvider&) = delete;

  ~MojoVideoEncodeAcceleratorProvider() override;

  // mojom::VideoEncodeAcceleratorProvider impl.
  void CreateVideoEncodeAccelerator(
      mojo::PendingReceiver<mojom::VideoEncodeAccelerator> receiver) override;
  void GetVideoEncodeAcceleratorSupportedProfiles(
      GetVideoEncodeAcceleratorSupportedProfilesCallback callback) override;

 private:
  const CreateAndInitializeVideoEncodeAcceleratorCallback create_vea_callback_;
  const gpu::GpuPreferences gpu_preferences_;
  const gpu::GpuDriverBugWorkarounds gpu_workarounds_;
  const gpu::GPUInfo::GPUDevice gpu_device_;
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_VIDEO_ENCODE_ACCELERATOR_PROVIDER_H_
