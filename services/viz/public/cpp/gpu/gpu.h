// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_GPU_GPU_H_
#define SERVICES_VIZ_PUBLIC_CPP_GPU_GPU_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "components/viz/common/gpu/context_provider.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/viz/public/mojom/gpu.mojom.h"

namespace viz {

class Gpu : public gpu::GpuChannelEstablishFactory {
 public:
  // The Gpu has to be initialized in the main thread before establishing
  // the gpu channel.
  static std::unique_ptr<Gpu> Create(
      mojo::PendingRemote<mojom::Gpu> remote,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      int client_id = 0,
      mojo::ScopedMessagePipeHandle channel_handle =
          mojo::ScopedMessagePipeHandle(),
      gpu::GpuChannelEstablishedCallback callback =
          gpu::GpuChannelEstablishedCallback());

  Gpu(const Gpu&) = delete;
  Gpu& operator=(const Gpu&) = delete;

  ~Gpu() override;

#if BUILDFLAG(IS_CHROMEOS)
  void CreateJpegDecodeAccelerator(
      mojo::PendingReceiver<chromeos_camera::mojom::MjpegDecodeAccelerator>
          jda_receiver);
#endif  // BUILDFLAG(IS_CHROMEOS)
  void CreateVideoEncodeAcceleratorProvider(
      mojo::PendingReceiver<media::mojom::VideoEncodeAcceleratorProvider>
          vea_provider_receiver);

  // gpu::GpuChannelEstablishFactory:
  void EstablishGpuChannel(
      gpu::GpuChannelEstablishedCallback callback) override;
  scoped_refptr<gpu::GpuChannelHost> EstablishGpuChannelSync() override;

  bool gpu_remote_disconnected() { return gpu_remote_disconnected_; }

  void LoseChannel();
  scoped_refptr<gpu::GpuChannelHost> GetGpuChannel();

 private:
  friend class GpuTest;

  class GpuPtrIO;
  class EstablishRequest;

  Gpu(mojo::PendingRemote<mojom::Gpu> gpu_remote,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      int client_id,
      mojo::ScopedMessagePipeHandle channel_handle,
      gpu::GpuChannelEstablishedCallback callback);

  // Sends a request to establish a gpu channel. If a request is currently
  // pending this will do nothing.
  void SendEstablishGpuChannelRequest(base::WaitableEvent* waitable_event);

  // Handles results of request to establish a gpu channel in
  // |pending_request_|.
  void OnEstablishedGpuChannel();

  void OnGPUInfoReceived(
      const gpu::GPUInfo& gpu_info,
      const gpu::GpuFeatureInfo& gpu_feature_info,
      const gpu::SharedImageCapabilities& shared_image_capabilities);

  // Completes the initialization of the GpuChannelHost with the fetched info.
  // Called when GPUInfo is available (either from the async callback or
  // fetched synchronously in EstablishGpuChannelSync).
  void CompleteInitialChannelCreation(
      const gpu::GPUInfo& gpu_info,
      const gpu::GpuFeatureInfo& gpu_feature_info,
      const gpu::SharedImageCapabilities& shared_image_capabilities);

  void RunEstablishCallbacks();

  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  std::unique_ptr<GpuPtrIO, base::OnTaskRunnerDeleter> gpu_;
  scoped_refptr<EstablishRequest> pending_request_;
  scoped_refptr<gpu::GpuChannelHost> gpu_channel_;
  // The GpuChannelHost builder that is being created asynchronously from the
  // initial handle.
  std::optional<gpu::GpuChannelHost::Builder>
      pending_initial_gpu_channel_builder_;

  bool gpu_remote_disconnected_ = false;

  std::vector<std::pair<base::TimeTicks, gpu::GpuChannelEstablishedCallback>>
      establish_callbacks_;

  base::WeakPtrFactory<Gpu> weak_ptr_factory_{this};
};

}  // namespace viz

#endif  // SERVICES_VIZ_PUBLIC_CPP_GPU_GPU_H_
