// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_SERVICE_VIDEO_CAPTURE_PROVIDER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_SERVICE_VIDEO_CAPTURE_PROVIDER_H_

#include <vector>

#include "base/threading/sequence_bound.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/renderer_host/media/ref_counted_video_source_provider.h"
#include "content/browser/renderer_host/media/video_capture_provider.h"
#include "content/common/content_export.h"
#include "content/public/browser/gpu_data_manager_observer.h"
#include "content/public/browser/service_process_host.h"
#include "services/video_capture/public/mojom/video_capture_service.mojom.h"

namespace content {

// Implementation of VideoCaptureProvider that uses
// video_capture::mojom::VideoCaptureService.
//
// Connects to the service lazily on demand and disconnects from the service as
// soon as all previously handed out VideoCaptureDeviceLauncher instances have
// been released and no more answers to GetDeviceInfosAsync() calls are pending.
class CONTENT_EXPORT ServiceVideoCaptureProvider
    : public VideoCaptureProvider,
      public content::GpuDataManagerObserver {
 public:
  // This constructor uses a default factory for instances of
  // viz::mojom::Gpu which produces instances of class content::GpuClient.
  explicit ServiceVideoCaptureProvider(
      base::RepeatingCallback<void(const std::string&)> emit_log_message_cb);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  using CreateAcceleratorFactoryCallback = base::RepeatingCallback<
      std::unique_ptr<video_capture::mojom::AcceleratorFactory>()>;
  // Lets clients provide a custom factory method for creating instances of
  // viz::mojom::Gpu.
  ServiceVideoCaptureProvider(
      CreateAcceleratorFactoryCallback create_accelerator_factory_cb,
      base::RepeatingCallback<void(const std::string&)> emit_log_message_cb);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  ~ServiceVideoCaptureProvider() override;

  // VideoCaptureProvider implementation.
  void GetDeviceInfosAsync(GetDeviceInfosCallback result_callback) override;
  std::unique_ptr<VideoCaptureDeviceLauncher> CreateDeviceLauncher() override;
  void OpenNativeScreenCapturePicker(
      DesktopMediaID::Type type,
      base::OnceCallback<void(DesktopMediaID::Id)> created_callback,
      base::OnceCallback<void(webrtc::DesktopCapturer::Source)> picker_callback,
      base::OnceCallback<void()> cancel_callback,
      base::OnceCallback<void()> error_callback) override;
  void CloseNativeScreenCapturePicker(DesktopMediaID device_id) override;

  // content::GpuDataManagerObserver implementation.
  void OnGpuInfoUpdate() override;

 private:
  void OnServiceStarted();
  void OnServiceStopped();

  void RegisterWithGpuDataManager();

  enum class ReasonForDisconnect { kShutdown, kUnused, kConnectionLost };

  void OnLauncherConnectingToSourceProvider(
      scoped_refptr<RefCountedVideoSourceProvider>* out_provider);
  // Discarding the returned RefCountedVideoSourceProvider indicates that the
  // caller no longer requires the connection to the service and allows it to
  // disconnect.
  [[nodiscard]] scoped_refptr<RefCountedVideoSourceProvider>
  LazyConnectToService();

  void GetDeviceInfosAsyncForRetry();
  void OnDeviceInfosReceived(
      scoped_refptr<RefCountedVideoSourceProvider> service_connection,
      video_capture::mojom::VideoSourceProvider::GetSourceInfosResult result,
      const std::vector<media::VideoCaptureDeviceInfo>& infos);
  void OnDeviceInfosRequestDropped(
      scoped_refptr<RefCountedVideoSourceProvider> service_connection);
  void OnLostConnectionToSourceProvider();
  void OnServiceConnectionClosed(ReasonForDisconnect reason);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  CreateAcceleratorFactoryCallback create_accelerator_factory_cb_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  base::RepeatingCallback<void(const std::string&)> emit_log_message_cb_;

  base::WeakPtr<RefCountedVideoSourceProvider> weak_service_connection_;

  bool launcher_has_connected_to_source_provider_;
  base::TimeTicks time_of_last_connect_;
  base::TimeTicks time_of_last_uninitialize_;

  std::vector<GetDeviceInfosCallback> get_device_infos_pending_callbacks_;
  bool get_device_infos_retried_ = false;

  // We own this but it must operate on the UI thread.
  class ServiceProcessObserver;
  std::optional<base::SequenceBound<ServiceProcessObserver>>
      service_process_observer_;

  base::WeakPtrFactory<ServiceVideoCaptureProvider> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_SERVICE_VIDEO_CAPTURE_PROVIDER_H_
