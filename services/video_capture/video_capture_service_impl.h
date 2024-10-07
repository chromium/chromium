// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_VIDEO_CAPTURE_SERVICE_IMPL_H_
#define SERVICES_VIDEO_CAPTURE_VIDEO_CAPTURE_SERVICE_IMPL_H_

#include <memory>

#include "base/feature_list.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/system/system_monitor.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/video_capture/public/mojom/video_capture_service.mojom.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/crosapi/mojom/video_capture.mojom.h"
#include "media/capture/video/chromeos/mojom/camera_app.mojom.h"
#include "services/video_capture/ash/video_capture_device_factory_ash.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH) || \
    BUILDFLAG(IS_MAC)
#include "services/viz/public/cpp/gpu/gpu.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) ||
        // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_MAC)
#include "media/device_monitors/device_monitor_mac.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "media/device_monitors/system_message_window_win.h"
#endif

namespace video_capture {

class VirtualDeviceEnabledDeviceFactory;
class VideoSourceProviderImpl;

class VideoCaptureServiceImpl : public mojom::VideoCaptureService {
 public:
  VideoCaptureServiceImpl(
      mojo::PendingReceiver<mojom::VideoCaptureService> receiver,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      bool create_system_monitor);

  VideoCaptureServiceImpl(const VideoCaptureServiceImpl&) = delete;
  VideoCaptureServiceImpl& operator=(const VideoCaptureServiceImpl&) = delete;

  ~VideoCaptureServiceImpl() override;

  // mojom::VideoCaptureService implementation.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  void InjectGpuDependencies(mojo::PendingRemote<mojom::AcceleratorFactory>
                                 accelerator_factory) override;
  void ConnectToCameraAppDeviceBridge(
      mojo::PendingReceiver<cros::mojom::CameraAppDeviceBridge> receiver)
      override;
  void BindVideoCaptureDeviceFactory(
      mojo::PendingReceiver<crosapi::mojom::VideoCaptureDeviceFactory> receiver)
      override;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  void ConnectToVideoSourceProvider(
      mojo::PendingReceiver<mojom::VideoSourceProvider> receiver) override;
  void BindControlsForTesting(
      mojo::PendingReceiver<mojom::TestingControls> receiver) override;
#if BUILDFLAG(IS_WIN)
  void OnGpuInfoUpdate(const CHROME_LUID& luid) override;
#endif
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH) || \
    BUILDFLAG(IS_MAC)
  void SetVizGpu(std::unique_ptr<viz::Gpu> viz_gpu);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) ||
        // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_MAC)
 private:
  class GpuDependenciesContext;

  void LazyInitializeGpuDependenciesContext();
  void LazyInitializeDeviceFactory();
  void LazyInitializeVideoSourceProvider();
  void OnLastSourceProviderClientDisconnected();
  // Initializes a platform-specific device monitor for device-change
  // notifications. If the client uses the DeviceNotifier interface to get
  // notifications this function should be called before the DeviceMonitor is
  // created. If the client uses base::SystemMonitor to get notifications,
  // this function should be called on service startup.
  void InitializeDeviceMonitor();

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void OnDisconnectedFromVCDFactoryAsh();
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_MAC)
  std::unique_ptr<media::DeviceMonitorMac> video_capture_device_monitor_mac_;
#endif
#if BUILDFLAG(IS_WIN)
  std::unique_ptr<media::SystemMessageWindowWin>
      video_capture_system_message_window_win_;
#endif

  mojo::Receiver<mojom::VideoCaptureService> receiver_;
  std::unique_ptr<base::SystemMonitor> system_monitor_;
  std::unique_ptr<VirtualDeviceEnabledDeviceFactory> device_factory_;
  std::unique_ptr<VideoSourceProviderImpl> video_source_provider_;
  std::unique_ptr<GpuDependenciesContext> gpu_dependencies_context_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Must be destroyed before |device_factory_|.
  std::unique_ptr<crosapi::VideoCaptureDeviceFactoryAsh>
      device_factory_ash_adapter_;
  // Must be destroyed before |device_factory_ash_adapter_|.
  mojo::ReceiverSet<crosapi::mojom::VideoCaptureDeviceFactory>
      factory_receivers_ash_;
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH) || \
    BUILDFLAG(IS_MAC)
  class VizGpuContextProvider;
  std::unique_ptr<VizGpuContextProvider> viz_gpu_context_provider_;
  std::unique_ptr<viz::Gpu> viz_gpu_;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) ||
        // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_MAC)

  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_VIDEO_CAPTURE_SERVICE_IMPL_H_
