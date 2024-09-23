// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/service_video_capture_provider.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/trace_event/common/trace_event_common.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/child_process_host_impl.h"
#include "content/browser/renderer_host/media/service_video_capture_device_launcher.h"
#include "content/browser/renderer_host/media/virtual_video_capture_devices_changed_observer.h"
#include "content/browser/video_capture_service_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/video_capture_service.h"
#include "content/public/common/content_features.h"
#include "gpu/config/gpu_info.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/video_capture/public/mojom/video_capture_service.mojom.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "content/public/browser/chromeos/delegate_to_browser_gpu_service_accelerator_factory.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

using GetSourceInfosResult =
    video_capture::mojom::VideoSourceProvider::GetSourceInfosResult;

#if BUILDFLAG(IS_CHROMEOS_ASH)
std::unique_ptr<video_capture::mojom::AcceleratorFactory>
CreateAcceleratorFactory() {
  return std::make_unique<
      content::DelegateToBrowserGpuServiceAcceleratorFactory>();
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Do not reorder, used for UMA Media.VideoCapture.GetDeviceInfosResult
enum class GetDeviceInfosResult {
  kSucessNoRetry = 0,
  kFailureNoRetry = 1,
  kSucessAfterRetry = 2,
  kFailureAfterRetry = 3,
  kMaxValue = kFailureAfterRetry,
};

void LogGetDeviceInfosResult(
    std::optional<GetSourceInfosResult> get_source_infos_result,
    bool get_device_infos_retried) {
  GetDeviceInfosResult result;
  if (get_source_infos_result &&
      *get_source_infos_result == GetSourceInfosResult::kSuccess) {
    result = get_device_infos_retried ? GetDeviceInfosResult::kSucessAfterRetry
                                      : GetDeviceInfosResult::kSucessNoRetry;
  } else {
    result = get_device_infos_retried ? GetDeviceInfosResult::kFailureAfterRetry
                                      : GetDeviceInfosResult::kFailureNoRetry;
  }
  base::UmaHistogramEnumeration("Media.VideoCapture.GetDeviceInfosResult",
                                result);
}

}  // anonymous namespace

namespace content {

class ServiceVideoCaptureProvider::ServiceProcessObserver
    : public ServiceProcessHost::Observer {
 public:
  ServiceProcessObserver(base::RepeatingClosure start_callback,
                         base::RepeatingClosure stop_callback)
      : io_task_runner_(GetIOThreadTaskRunner({})),
        start_callback_(std::move(start_callback)),
        stop_callback_(std::move(stop_callback)) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    ServiceProcessHost::AddObserver(this);
  }

  ServiceProcessObserver(const ServiceProcessObserver&) = delete;
  ServiceProcessObserver& operator=(const ServiceProcessObserver&) = delete;

  ~ServiceProcessObserver() override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    ServiceProcessHost::RemoveObserver(this);
  }

 private:
  // ServiceProcessHost::Observer implementation.
  void OnServiceProcessLaunched(const ServiceProcessInfo& info) override {
    if (info.IsService<video_capture::mojom::VideoCaptureService>())
      io_task_runner_->PostTask(FROM_HERE, base::BindOnce(start_callback_));
  }

  void OnServiceProcessTerminatedNormally(
      const ServiceProcessInfo& info) override {
    if (info.IsService<video_capture::mojom::VideoCaptureService>())
      io_task_runner_->PostTask(FROM_HERE, base::BindOnce(stop_callback_));
  }

  void OnServiceProcessCrashed(const ServiceProcessInfo& info) override {
    if (info.IsService<video_capture::mojom::VideoCaptureService>()) {
      LOG(WARNING) << "Detected crash of video capture service";
      io_task_runner_->PostTask(FROM_HERE, base::BindOnce(stop_callback_));
    }
  }

  const scoped_refptr<base::TaskRunner> io_task_runner_;
  const base::RepeatingClosure start_callback_;
  const base::RepeatingClosure stop_callback_;
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
ServiceVideoCaptureProvider::ServiceVideoCaptureProvider(
    base::RepeatingCallback<void(const std::string&)> emit_log_message_cb)
    : ServiceVideoCaptureProvider(base::NullCallback(),
                                  std::move(emit_log_message_cb)) {}

ServiceVideoCaptureProvider::ServiceVideoCaptureProvider(
    CreateAcceleratorFactoryCallback create_accelerator_factory_cb,
    base::RepeatingCallback<void(const std::string&)> emit_log_message_cb)
    : create_accelerator_factory_cb_(std::move(create_accelerator_factory_cb)),
      emit_log_message_cb_(std::move(emit_log_message_cb)),
      launcher_has_connected_to_source_provider_(false) {
#else   // BUILDFLAG(IS_CHROMEOS_ASH)
ServiceVideoCaptureProvider::ServiceVideoCaptureProvider(
    base::RepeatingCallback<void(const std::string&)> emit_log_message_cb)
    : emit_log_message_cb_(std::move(emit_log_message_cb)),
      launcher_has_connected_to_source_provider_(false) {
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  if (features::IsVideoCaptureServiceEnabledForOutOfProcess()) {
    service_process_observer_.emplace(
        GetUIThreadTaskRunner({}),
        base::BindRepeating(&ServiceVideoCaptureProvider::OnServiceStarted,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindRepeating(&ServiceVideoCaptureProvider::OnServiceStopped,
                            weak_ptr_factory_.GetWeakPtr()));
  } else if (features::IsVideoCaptureServiceEnabledForBrowserProcess()) {
    // Connect immediately and permanently when the service runs in-process.
    GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&ServiceVideoCaptureProvider::OnServiceStarted,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  // Must register at the IO thread so that callbacks would be also
  // triggered on the IO thread.
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ServiceVideoCaptureProvider::RegisterWithGpuDataManager,
                     weak_ptr_factory_.GetWeakPtr()));
}

ServiceVideoCaptureProvider::~ServiceVideoCaptureProvider() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  OnServiceConnectionClosed(ReasonForDisconnect::kShutdown);
  content::GpuDataManager::GetInstance()->RemoveObserver(this);
}

void ServiceVideoCaptureProvider::GetDeviceInfosAsync(
    GetDeviceInfosCallback result_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  emit_log_message_cb_.Run("ServiceVideoCaptureProvider::GetDeviceInfosAsync");
  get_device_infos_retried_ = false;
  get_device_infos_pending_callbacks_.push_back(std::move(result_callback));
  GetDeviceInfosAsyncForRetry();
}

std::unique_ptr<VideoCaptureDeviceLauncher>
ServiceVideoCaptureProvider::CreateDeviceLauncher() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  return std::make_unique<ServiceVideoCaptureDeviceLauncher>(
      base::BindRepeating(
          &ServiceVideoCaptureProvider::OnLauncherConnectingToSourceProvider,
          weak_ptr_factory_.GetWeakPtr()));
}

void ServiceVideoCaptureProvider::OpenNativeScreenCapturePicker(
    DesktopMediaID::Type type,
    base::OnceCallback<void(DesktopMediaID::Id)> created_callback,
    base::OnceCallback<void(webrtc::DesktopCapturer::Source)> picker_callback,
    base::OnceCallback<void()> cancel_callback,
    base::OnceCallback<void()> error_callback) {
  NOTREACHED();
}

void ServiceVideoCaptureProvider::CloseNativeScreenCapturePicker(
    DesktopMediaID device_id) {
  NOTREACHED();
}

void ServiceVideoCaptureProvider::OnServiceStarted() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  // Whenever the video capture service starts, we register a
  // VirtualVideoCaptureDevicesChangedObserver in order to propagate device
  // change events when virtual devices are added to or removed from the
  // service.
  auto service_connection = LazyConnectToService();
  mojo::PendingRemote<video_capture::mojom::DevicesChangedObserver> observer;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<VirtualVideoCaptureDevicesChangedObserver>(),
      observer.InitWithNewPipeAndPassReceiver());
  service_connection->source_provider()->RegisterVirtualDevicesChangedObserver(
      std::move(observer),
      true /*raise_event_if_virtual_devices_already_present*/);
}

void ServiceVideoCaptureProvider::OnServiceStopped() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (!get_device_infos_pending_callbacks_.empty()) {
    // The service stopped during a device info query.
    TRACE_EVENT_INSTANT0(
        TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
        "Video capture service has shut down. Retrying GetDeviceInfos.",
        TRACE_EVENT_SCOPE_PROCESS);
    GetDeviceInfosAsyncForRetry();
  }
}

void ServiceVideoCaptureProvider::OnLauncherConnectingToSourceProvider(
    scoped_refptr<RefCountedVideoSourceProvider>* out_provider) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  launcher_has_connected_to_source_provider_ = true;
  *out_provider = LazyConnectToService();
}

void ServiceVideoCaptureProvider::RegisterWithGpuDataManager() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  content::GpuDataManager::GetInstance()->AddObserver(this);
}

scoped_refptr<RefCountedVideoSourceProvider>
ServiceVideoCaptureProvider::LazyConnectToService() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (weak_service_connection_) {
    // There already is a connection.
    return base::WrapRefCounted(weak_service_connection_.get());
  }

  launcher_has_connected_to_source_provider_ = false;
  time_of_last_connect_ = base::TimeTicks::Now();

  auto ui_task_runner = GetUIThreadTaskRunner({});
#if BUILDFLAG(IS_CHROMEOS_ASH)
  mojo::PendingRemote<video_capture::mojom::AcceleratorFactory>
      accelerator_factory;
  if (!create_accelerator_factory_cb_)
    create_accelerator_factory_cb_ =
        base::BindRepeating(&CreateAcceleratorFactory);
  mojo::MakeSelfOwnedReceiver(
      create_accelerator_factory_cb_.Run(),
      accelerator_factory.InitWithNewPipeAndPassReceiver());
  GetVideoCaptureService().InjectGpuDependencies(
      std::move(accelerator_factory));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_WIN)
  // Pass active gpu info.
  GetVideoCaptureService().OnGpuInfoUpdate(
      content::GpuDataManager::GetInstance()->GetGPUInfo().active_gpu().luid);
#endif

  mojo::Remote<video_capture::mojom::VideoSourceProvider> source_provider;
#if BUILDFLAG(IS_MAC)
  if (get_device_infos_retried_) {
    // If the service crashed once during a device info query, enable the
    // safe-mode VideoCaptureService.
    EnableVideoCaptureServiceSafeMode();
  }
#endif
  GetVideoCaptureService().ConnectToVideoSourceProvider(
      source_provider.BindNewPipeAndPassReceiver());
  source_provider.set_disconnect_handler(base::BindOnce(
      &ServiceVideoCaptureProvider::OnLostConnectionToSourceProvider,
      weak_ptr_factory_.GetWeakPtr()));
  auto result = base::MakeRefCounted<RefCountedVideoSourceProvider>(
      std::move(source_provider),
      base::BindOnce(&ServiceVideoCaptureProvider::OnServiceConnectionClosed,
                     weak_ptr_factory_.GetWeakPtr(),
                     ReasonForDisconnect::kUnused));
  weak_service_connection_ = result->GetWeakPtr();
  return result;
}

void ServiceVideoCaptureProvider::GetDeviceInfosAsyncForRetry() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  auto service_connection = LazyConnectToService();
  // Make sure that the callback gets invoked with an empty result in case
  // that the service drops the request.
  service_connection->source_provider()->GetSourceInfos(
      mojo::WrapCallbackWithDropHandler(
          base::BindOnce(&ServiceVideoCaptureProvider::OnDeviceInfosReceived,
                         weak_ptr_factory_.GetWeakPtr(), service_connection),
          base::BindOnce(
              &ServiceVideoCaptureProvider::OnDeviceInfosRequestDropped,
              weak_ptr_factory_.GetWeakPtr(), service_connection)));
}

void ServiceVideoCaptureProvider::OnDeviceInfosReceived(
    scoped_refptr<RefCountedVideoSourceProvider> service_connection,
    GetSourceInfosResult result,
    const std::vector<media::VideoCaptureDeviceInfo>& infos) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  LogGetDeviceInfosResult(result, get_device_infos_retried_);
  for (GetDeviceInfosCallback& callback : get_device_infos_pending_callbacks_) {
    media::mojom::DeviceEnumerationResult callback_result;
    switch (result) {
      case GetSourceInfosResult::kSuccess:
        callback_result = media::mojom::DeviceEnumerationResult::kSuccess;
        break;
      case GetSourceInfosResult::kErrorDroppedRequest:
        callback_result = media::mojom::DeviceEnumerationResult::
            kErrorCaptureServiceDroppedRequest;
        break;
      default:
        NOTREACHED() << "Invalid GetSourceInfosResult result " << result;
    }
    std::move(callback).Run(callback_result, infos);
  }
  get_device_infos_pending_callbacks_.clear();
}

void ServiceVideoCaptureProvider::OnDeviceInfosRequestDropped(
    scoped_refptr<RefCountedVideoSourceProvider> service_connection) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (base::FeatureList::IsEnabled(
          features::kRetryGetVideoCaptureDeviceInfos)) {
    if (!get_device_infos_retried_) {
      get_device_infos_retried_ = true;
      // Do nothing, OnServiceStopped will retry the query automatically when
      // the service has been torn down.
      return;
    }
    LOG(WARNING) << "Too many GetDeviceInfos() retries";
    emit_log_message_cb_.Run(
        "ServiceVideoCaptureProvider::OnDeviceInfosRequestDropped: Too many "
        "retries");
  }

  LogGetDeviceInfosResult(/*get_source_infos_result=*/std::nullopt,
                          get_device_infos_retried_);

  // After too many retries, we just return an empty list
  for (GetDeviceInfosCallback& callback : get_device_infos_pending_callbacks_) {
    std::move(callback).Run(
        media::mojom::DeviceEnumerationResult::kErrorCaptureServiceCrash,
        std::vector<media::VideoCaptureDeviceInfo>());
  }
  get_device_infos_pending_callbacks_.clear();
}

void ServiceVideoCaptureProvider::OnLostConnectionToSourceProvider() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  emit_log_message_cb_.Run(
      "ServiceVideoCaptureProvider::OnLostConnectionToSourceProvider");
  // This may indicate that the video capture service has crashed. Uninitialize
  // here, so that a new connection will be established when clients try to
  // reconnect.
  OnServiceConnectionClosed(ReasonForDisconnect::kConnectionLost);
}

void ServiceVideoCaptureProvider::OnServiceConnectionClosed(
    ReasonForDisconnect reason) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  time_of_last_uninitialize_ = base::TimeTicks::Now();
}

void ServiceVideoCaptureProvider::OnGpuInfoUpdate() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (!weak_service_connection_) {
    // Only need to notify the service if it's already running.
    return;
  }
#if BUILDFLAG(IS_WIN)
  GetVideoCaptureService().OnGpuInfoUpdate(
      content::GpuDataManager::GetInstance()->GetGPUInfo().active_gpu().luid);
#endif
}

}  // namespace content
