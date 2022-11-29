// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/service_video_capture_provider.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/renderer_host/media/service_video_capture_device_launcher.h"
#include "content/browser/renderer_host/media/virtual_video_capture_devices_changed_observer.h"
#include "content/common/child_process_host_impl.h"
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

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/trace_event/common/trace_event_common.h"
#endif

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
std::unique_ptr<video_capture::mojom::AcceleratorFactory>
CreateAcceleratorFactory() {
  return std::make_unique<
      content::DelegateToBrowserGpuServiceAcceleratorFactory>();
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_MAC)
static const int kMaxRetriesForGetDeviceInfos = 1;
#endif

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
    if (info.IsService<video_capture::mojom::VideoCaptureService>())
      io_task_runner_->PostTask(FROM_HERE, base::BindOnce(stop_callback_));
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
  GetDeviceInfosAsyncForRetry(std::move(result_callback), 0);
}

std::unique_ptr<VideoCaptureDeviceLauncher>
ServiceVideoCaptureProvider::CreateDeviceLauncher() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  return std::make_unique<ServiceVideoCaptureDeviceLauncher>(
      base::BindRepeating(
          &ServiceVideoCaptureProvider::OnLauncherConnectingToSourceProvider,
          weak_ptr_factory_.GetWeakPtr()));
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
#if BUILDFLAG(IS_MAC)
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (stashed_result_callback_for_retry_) {
    TRACE_EVENT_INSTANT0(
        TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
        "Video capture service has shut down. Retrying GetDeviceInfos.",
        TRACE_EVENT_SCOPE_PROCESS);
    GetDeviceInfosAsyncForRetry(std::move(stashed_result_callback_for_retry_),
                                stashed_retry_count_ + 1);
  }
#endif
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

void ServiceVideoCaptureProvider::GetDeviceInfosAsyncForRetry(
    GetDeviceInfosCallback result_callback,
    int retry_count) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  auto service_connection = LazyConnectToService();
  service_connection->SetRetryCount(retry_count);
  // Make sure that |result_callback| gets invoked with an empty result in case
  // that the service drops the request.
  auto split_callback = base::SplitOnceCallback(std::move(result_callback));
  service_connection->source_provider()->GetSourceInfos(
      mojo::WrapCallbackWithDropHandler(
          base::BindOnce(&ServiceVideoCaptureProvider::OnDeviceInfosReceived,
                         weak_ptr_factory_.GetWeakPtr(), service_connection,
                         std::move(split_callback.first), retry_count),
          base::BindOnce(
              &ServiceVideoCaptureProvider::OnDeviceInfosRequestDropped,
              weak_ptr_factory_.GetWeakPtr(), service_connection,
              std::move(split_callback.second), retry_count)));
}

void ServiceVideoCaptureProvider::OnDeviceInfosReceived(
    scoped_refptr<RefCountedVideoSourceProvider> service_connection,
    GetDeviceInfosCallback result_callback,
    int retry_count,
    const std::vector<media::VideoCaptureDeviceInfo>& infos) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
#if BUILDFLAG(IS_MAC)
  std::string model = base::mac::GetModelIdentifier();
  if (base::FeatureList::IsEnabled(
          features::kRetryGetVideoCaptureDeviceInfos) &&
      base::StartsWith(model, "MacBook",
                       base::CompareCase::INSENSITIVE_ASCII)) {
    if (infos.empty() && retry_count < kMaxRetriesForGetDeviceInfos &&
        !stashed_result_callback_for_retry_) {
      TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
                           "Waiting for video capture service to shut down.",
                           TRACE_EVENT_SCOPE_PROCESS);
      stashed_result_callback_for_retry_ = std::move(result_callback);
      stashed_retry_count_ = retry_count;

      // We may try again once |OnServiceStopped()| is invoked via our
      // ServiceProcessHost observer.
      return;
    }
  }
#endif
  std::move(result_callback)
      .Run(media::mojom::DeviceEnumerationResult::kSuccess, infos);
}

void ServiceVideoCaptureProvider::OnDeviceInfosRequestDropped(
    scoped_refptr<RefCountedVideoSourceProvider> service_connection,
    GetDeviceInfosCallback result_callback,
    int retry_count) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  std::move(result_callback)
      .Run(media::mojom::DeviceEnumerationResult::kErrorCaptureServiceCrash,
           std::vector<media::VideoCaptureDeviceInfo>());
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
