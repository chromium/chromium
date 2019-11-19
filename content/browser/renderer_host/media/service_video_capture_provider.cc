// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/service_video_capture_provider.h"

#include <utility>

#include "base/bind.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/media/service_video_capture_device_launcher.h"
#include "content/browser/renderer_host/media/virtual_video_capture_devices_changed_observer.h"
#include "content/common/child_process_host_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/video_capture_service.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/video_capture/public/mojom/video_capture_service.mojom.h"
#include "services/video_capture/public/uma/video_capture_service_event.h"

#if defined(OS_CHROMEOS)
#include "content/public/browser/chromeos/delegate_to_browser_gpu_service_accelerator_factory.h"
#endif  // defined(OS_CHROMEOS)

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/trace_event/common/trace_event_common.h"
#include "content/public/common/content_features.h"
#endif

namespace {

#if defined(OS_CHROMEOS)
std::unique_ptr<video_capture::mojom::AcceleratorFactory>
CreateAcceleratorFactory() {
  return std::make_unique<
      content::DelegateToBrowserGpuServiceAcceleratorFactory>();
}
#endif  // defined(OS_CHROMEOS)

#if defined(OS_MACOSX)
static const int kMaxRetriesForGetDeviceInfos = 1;
#endif

}  // anonymous namespace

namespace content {

class ServiceVideoCaptureProvider::ServiceProcessObserver
    : public ServiceProcessHost::Observer {
 public:
  ServiceProcessObserver(base::RepeatingClosure start_callback,
                         base::RepeatingClosure stop_callback)
      : io_task_runner_(
            base::CreateSingleThreadTaskRunner({BrowserThread::IO})),
        start_callback_(std::move(start_callback)),
        stop_callback_(std::move(stop_callback)) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    ServiceProcessHost::AddObserver(this);
  }

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

  DISALLOW_COPY_AND_ASSIGN(ServiceProcessObserver);
};

#if defined(OS_CHROMEOS)
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
#else   // defined(OS_CHROMEOS)
ServiceVideoCaptureProvider::ServiceVideoCaptureProvider(
    base::RepeatingCallback<void(const std::string&)> emit_log_message_cb)
    : emit_log_message_cb_(std::move(emit_log_message_cb)),
      launcher_has_connected_to_source_provider_(false) {
#endif  // defined(OS_CHROMEOS)
  if (features::IsVideoCaptureServiceEnabledForOutOfProcess()) {
    service_process_observer_.emplace(
        base::CreateSingleThreadTaskRunner({BrowserThread::UI}),
        base::BindRepeating(&ServiceVideoCaptureProvider::OnServiceStarted,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindRepeating(&ServiceVideoCaptureProvider::OnServiceStopped,
                            weak_ptr_factory_.GetWeakPtr()));
  } else if (features::IsVideoCaptureServiceEnabledForBrowserProcess()) {
    // Connect immediately and permanently when the service runs in-process.
    base::CreateSingleThreadTaskRunner({BrowserThread::IO})
        ->PostTask(FROM_HERE,
                   base::Bind(&ServiceVideoCaptureProvider::OnServiceStarted,
                              weak_ptr_factory_.GetWeakPtr()));
  }
}

ServiceVideoCaptureProvider::~ServiceVideoCaptureProvider() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  OnServiceConnectionClosed(ReasonForDisconnect::kShutdown);
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
#if defined(OS_MACOSX)
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (stashed_result_callback_for_retry_) {
    TRACE_EVENT_INSTANT0(
        TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
        "Video capture service has shut down. Retrying GetDeviceInfos.",
        TRACE_EVENT_SCOPE_PROCESS);
    video_capture::uma::LogMacbookRetryGetDeviceInfosEvent(
        video_capture::uma::PROVIDER_SERVICE_STOPPED_ISSUING_RETRY);
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

scoped_refptr<RefCountedVideoSourceProvider>
ServiceVideoCaptureProvider::LazyConnectToService() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (weak_service_connection_) {
    // There already is a connection.
    return base::WrapRefCounted(weak_service_connection_.get());
  }

  video_capture::uma::LogVideoCaptureServiceEvent(
      video_capture::uma::BROWSER_CONNECTING_TO_SERVICE);
  if (time_of_last_uninitialize_ != base::TimeTicks()) {
    if (launcher_has_connected_to_source_provider_) {
      video_capture::uma::LogDurationUntilReconnectAfterCapture(
          base::TimeTicks::Now() - time_of_last_uninitialize_);
    } else {
      video_capture::uma::LogDurationUntilReconnectAfterEnumerationOnly(
          base::TimeTicks::Now() - time_of_last_uninitialize_);
    }
  }

  launcher_has_connected_to_source_provider_ = false;
  time_of_last_connect_ = base::TimeTicks::Now();

  auto ui_task_runner = base::CreateSingleThreadTaskRunner({BrowserThread::UI});
#if defined(OS_CHROMEOS)
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
#endif  // defined(OS_CHROMEOS)

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
  service_connection->source_provider()->GetSourceInfos(
      mojo::WrapCallbackWithDropHandler(
          base::BindOnce(&ServiceVideoCaptureProvider::OnDeviceInfosReceived,
                         weak_ptr_factory_.GetWeakPtr(), service_connection,
                         result_callback, retry_count),
          base::BindOnce(
              &ServiceVideoCaptureProvider::OnDeviceInfosRequestDropped,
              weak_ptr_factory_.GetWeakPtr(), service_connection,
              result_callback, retry_count)));
}

void ServiceVideoCaptureProvider::OnDeviceInfosReceived(
    scoped_refptr<RefCountedVideoSourceProvider> service_connection,
    GetDeviceInfosCallback result_callback,
    int retry_count,
    const std::vector<media::VideoCaptureDeviceInfo>& infos) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
#if defined(OS_MACOSX)
  std::string model = base::mac::GetModelIdentifier();
  if (base::FeatureList::IsEnabled(
          features::kRetryGetVideoCaptureDeviceInfos) &&
      base::StartsWith(model, "MacBook",
                       base::CompareCase::INSENSITIVE_ASCII)) {
    if (retry_count > 0) {
      video_capture::uma::LogMacbookRetryGetDeviceInfosEvent(
          infos.empty()
              ? video_capture::uma::
                    PROVIDER_RECEIVED_ZERO_INFOS_FROM_RETRY_GIVING_UP
              : video_capture::uma::PROVIDER_RECEIVED_NONZERO_INFOS_FROM_RETRY);
    }
    if (infos.empty() && stashed_result_callback_for_retry_) {
      video_capture::uma::LogMacbookRetryGetDeviceInfosEvent(
          video_capture::uma::
              PROVIDER_NOT_ATTEMPTING_RETRY_BECAUSE_ALREADY_PENDING);
    }
    if (infos.empty() && retry_count < kMaxRetriesForGetDeviceInfos &&
        !stashed_result_callback_for_retry_) {
      video_capture::uma::LogMacbookRetryGetDeviceInfosEvent(
          video_capture::uma::PROVIDER_RECEIVED_ZERO_INFOS_STOPPING_SERVICE);
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
  std::move(result_callback).Run(infos);
}

void ServiceVideoCaptureProvider::OnDeviceInfosRequestDropped(
    scoped_refptr<RefCountedVideoSourceProvider> service_connection,
    GetDeviceInfosCallback result_callback,
    int retry_count) {
#if defined(OS_MACOSX)
  std::string model = base::mac::GetModelIdentifier();
  if (base::FeatureList::IsEnabled(
          features::kRetryGetVideoCaptureDeviceInfos) &&
      base::StartsWith(model, "MacBook",
                       base::CompareCase::INSENSITIVE_ASCII)) {
    video_capture::uma::LogMacbookRetryGetDeviceInfosEvent(
        retry_count == 0 ? video_capture::uma::
                               SERVICE_DROPPED_DEVICE_INFOS_REQUEST_ON_FIRST_TRY
                         : video_capture::uma::
                               SERVICE_DROPPED_DEVICE_INFOS_REQUEST_ON_RETRY);
  }
#endif
  std::move(result_callback).Run(std::vector<media::VideoCaptureDeviceInfo>());
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
  base::TimeDelta duration_since_last_connect(base::TimeTicks::Now() -
                                              time_of_last_connect_);
  switch (reason) {
    case ReasonForDisconnect::kShutdown:
    case ReasonForDisconnect::kUnused:
      if (launcher_has_connected_to_source_provider_) {
        video_capture::uma::LogVideoCaptureServiceEvent(
            video_capture::uma::
                BROWSER_CLOSING_CONNECTION_TO_SERVICE_AFTER_CAPTURE);
        video_capture::uma::
            LogDurationFromLastConnectToClosingConnectionAfterCapture(
                duration_since_last_connect);
      } else {
        video_capture::uma::LogVideoCaptureServiceEvent(
            video_capture::uma::
                BROWSER_CLOSING_CONNECTION_TO_SERVICE_AFTER_ENUMERATION_ONLY);
        video_capture::uma::
            LogDurationFromLastConnectToClosingConnectionAfterEnumerationOnly(
                duration_since_last_connect);
      }
      break;
    case ReasonForDisconnect::kConnectionLost:
      video_capture::uma::LogVideoCaptureServiceEvent(
          video_capture::uma::BROWSER_LOST_CONNECTION_TO_SERVICE);
      video_capture::uma::LogDurationFromLastConnectToConnectionLost(
          duration_since_last_connect);
      break;
  }
  time_of_last_uninitialize_ = base::TimeTicks::Now();
}

}  // namespace content
