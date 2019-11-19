// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_GPU_GPU_H_
#define SERVICES_VIZ_PUBLIC_CPP_GPU_GPU_H_

#include <stdint.h>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "components/viz/common/gpu/context_provider.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/viz/public/cpp/gpu/client_gpu_memory_buffer_manager.h"
#include "services/viz/public/mojom/gpu.mojom.h"

namespace service_manager {
class Connector;
}

namespace viz {

class Gpu : public gpu::GpuChannelEstablishFactory {
 public:
  // The Gpu has to be initialized in the main thread before establishing
  // the gpu channel.
  static std::unique_ptr<Gpu> Create(
      service_manager::Connector* connector,
      const std::string& service_name,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);
  static std::unique_ptr<Gpu> Create(
      mojo::PendingRemote<mojom::Gpu> remote,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);

  ~Gpu() override;

  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager() const {
    return gpu_memory_buffer_manager_.get();
  }

#if defined(OS_CHROMEOS)
  void CreateJpegDecodeAccelerator(
      mojo::PendingReceiver<chromeos_camera::mojom::MjpegDecodeAccelerator>
          jda_receiver);
#endif  // defined(OS_CHROMEOS)
  void CreateVideoEncodeAcceleratorProvider(
      mojo::PendingReceiver<media::mojom::VideoEncodeAcceleratorProvider>
          vea_provider_receiver);

  // gpu::GpuChannelEstablishFactory:
  void EstablishGpuChannel(
      gpu::GpuChannelEstablishedCallback callback) override;
  scoped_refptr<gpu::GpuChannelHost> EstablishGpuChannelSync() override;
  gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager() override;

  void LoseChannel();
  scoped_refptr<gpu::GpuChannelHost> GetGpuChannel();

 private:
  friend class GpuTest;

  class GpuPtrIO;
  class EstablishRequest;

  Gpu(mojo::PendingRemote<mojom::Gpu> gpu_remote,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // Sends a request to establish a gpu channel. If a request is currently
  // pending this will do nothing.
  void SendEstablishGpuChannelRequest();

  // Handles results of request to establish a gpu channel in
  // |pending_request_|.
  void OnEstablishedGpuChannel();

  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  std::unique_ptr<ClientGpuMemoryBufferManager> gpu_memory_buffer_manager_;

  std::unique_ptr<GpuPtrIO, base::OnTaskRunnerDeleter> gpu_;
  scoped_refptr<EstablishRequest> pending_request_;
  scoped_refptr<gpu::GpuChannelHost> gpu_channel_;
  std::vector<gpu::GpuChannelEstablishedCallback> establish_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(Gpu);
};

}  // namespace viz

#endif  // SERVICES_VIZ_PUBLIC_CPP_GPU_GPU_H_
