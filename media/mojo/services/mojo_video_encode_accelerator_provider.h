// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_VIDEO_ENCODE_ACCELERATOR_PROVIDER_H_
#define MEDIA_MOJO_SERVICES_MOJO_VIDEO_ENCODE_ACCELERATOR_PROVIDER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "media/mojo/mojom/video_encode_accelerator.mojom.h"
#include "media/mojo/services/media_mojo_export.h"
#include "media/mojo/services/mojo_video_encode_accelerator_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace gpu {
struct GpuPreferences;
}  // namespace gpu

namespace media {

// This class implements the interface mojom::VideoEncodeAcceleratorProvider,
// holds on to the necessary objects to create mojom::VideoEncodeAccelerators.
class MEDIA_MOJO_EXPORT MojoVideoEncodeAcceleratorProvider
    : public mojom::VideoEncodeAcceleratorProvider {
 public:
  using CreateAndInitializeVideoEncodeAcceleratorCallback =
      MojoVideoEncodeAcceleratorService::
          CreateAndInitializeVideoEncodeAcceleratorCallback;

  static void Create(
      mojo::PendingReceiver<mojom::VideoEncodeAcceleratorProvider> receiver,
      const CreateAndInitializeVideoEncodeAcceleratorCallback&
          create_vea_callback,
      const gpu::GpuPreferences& gpu_preferences);

  MojoVideoEncodeAcceleratorProvider(
      const CreateAndInitializeVideoEncodeAcceleratorCallback&
          create_vea_callback,
      const gpu::GpuPreferences& gpu_preferences);
  ~MojoVideoEncodeAcceleratorProvider() override;

  // mojom::VideoEncodeAcceleratorProvider impl.
  void CreateVideoEncodeAccelerator(
      mojo::PendingReceiver<mojom::VideoEncodeAccelerator> receiver) override;

 private:
  const CreateAndInitializeVideoEncodeAcceleratorCallback create_vea_callback_;
  const gpu::GpuPreferences& gpu_preferences_;

  DISALLOW_COPY_AND_ASSIGN(MojoVideoEncodeAcceleratorProvider);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_VIDEO_ENCODE_ACCELERATOR_PROVIDER_H_
