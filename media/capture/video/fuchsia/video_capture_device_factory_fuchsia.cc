// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/fuchsia/video_capture_device_factory_fuchsia.h"

#include <lib/sys/cpp/component_context.h>

#include "base/check_op.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/system_monitor.h"
#include "base/time/time.h"
#include "media/capture/video/fuchsia/video_capture_device_fuchsia.h"

namespace media {

// Helper class that calls GetIdentifier() and GetConfigurations() for a
// fuchsia::camera3::Device instance and stores the results afterwards.
class VideoCaptureDeviceFactoryFuchsia::DeviceConfigFetcher {
 public:
  DeviceConfigFetcher(uint64_t device_id, fuchsia::camera3::DevicePtr device)
      : device_id_(device_id), device_(std::move(device)) {
    device_.set_error_handler(
        fit::bind_member(this, &DeviceConfigFetcher::OnError));
  }

  void Fetch(base::OnceClosure on_fetched_callback) {
    DCHECK(device_);
    DCHECK(!have_results());
    DCHECK(!on_fetched_callback_);

    on_fetched_callback_ = std::move(on_fetched_callback);
    device_->GetIdentifier(
        fit::bind_member(this, &DeviceConfigFetcher::OnDescription));
    device_->GetConfigurations(
        fit::bind_member(this, &DeviceConfigFetcher::OnConfigurations));
  }

  ~DeviceConfigFetcher() = default;

  DeviceConfigFetcher(const DeviceConfigFetcher&) = delete;
  const DeviceConfigFetcher& operator=(const DeviceConfigFetcher&) = delete;

  bool is_pending() const { return !!device_; }
  bool have_results() const { return description_ && formats_; }
  bool is_usable() const { return device_ || have_results(); }

  uint64_t device_id() const { return device_id_; }
  const std::string& description() const { return description_.value(); }
  const VideoCaptureFormats& formats() const { return formats_.value(); }

 private:
  void OnError(zx_status_t status) {
    ZX_LOG(ERROR, status) << "fuchsia.camera3.Device disconnected";

    if (on_fetched_callback_)
      std::move(on_fetched_callback_).Run();
  }

  void OnDescription(fidl::StringPtr identifier) {
    description_ = identifier.value_or("");
    MaybeSignalDone();
  }

  void OnConfigurations(std::vector<fuchsia::camera3::Configuration> configs) {
    VideoCaptureFormats formats;
    for (auto& config : configs) {
      for (auto& props : config.streams) {
        VideoCaptureFormat format;
        format.frame_size = gfx::Size(props.image_format.display_width,
                                      props.image_format.display_height);
        format.frame_rate = static_cast<float>(props.frame_rate.numerator) /
                            props.frame_rate.denominator;
        format.pixel_format =
            VideoCaptureDeviceFuchsia::GetConvertedPixelFormat(
                props.image_format.pixel_format.type);
        if (format.pixel_format == PIXEL_FORMAT_UNKNOWN)
          continue;
        formats.push_back(format);
      }
    }

    formats_ = std::move(formats);
    MaybeSignalDone();
  }

  void MaybeSignalDone() {
    if (!have_results())
      return;

    device_.Unbind();

    std::move(on_fetched_callback_).Run();
  }

  uint64_t device_id_;
  fuchsia::camera3::DevicePtr device_;
  std::optional<std::string> description_;
  std::optional<VideoCaptureFormats> formats_;
  base::OnceClosure on_fetched_callback_;
};

VideoCaptureDeviceFactoryFuchsia::VideoCaptureDeviceFactoryFuchsia() {
  DETACH_FROM_THREAD(thread_checker_);
}

VideoCaptureDeviceFactoryFuchsia::~VideoCaptureDeviceFactoryFuchsia() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

VideoCaptureErrorOrDevice VideoCaptureDeviceFactoryFuchsia::CreateDevice(
    const VideoCaptureDeviceDescriptor& device_descriptor) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  uint64_t device_id;
  bool converted =
      base::StringToUint64(device_descriptor.device_id, &device_id);

  // Test may call CreateDevice() with an invalid |device_id|.
  if (!converted)
    return VideoCaptureErrorOrDevice(
        VideoCaptureError::
            kVideoCaptureControllerInvalidOrUnsupportedVideoCaptureParametersRequested);

  // CreateDevice() may be called before GetDeviceDescriptors(). Make sure
  // |device_watcher_| is initialized.
  if (!device_watcher_)
    Initialize();

  fidl::InterfaceHandle<fuchsia::camera3::Device> device;
  device_watcher_->ConnectToDevice(device_id, device.NewRequest());
  return VideoCaptureErrorOrDevice(
      std::make_unique<VideoCaptureDeviceFuchsia>(std::move(device)));
}

void VideoCaptureDeviceFactoryFuchsia::GetDevicesInfo(
    GetDevicesInfoCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // If the list hasn't been received, then wait until it's received.
  if (!devices_ || num_pending_device_info_requests_) {
    pending_devices_info_requests_.push_back(std::move(callback));

    if (!device_watcher_)
      Initialize();

    return;
  }

  std::move(callback).Run(MakeDevicesInfo());
}

void VideoCaptureDeviceFactoryFuchsia::Initialize() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!device_watcher_);
  DCHECK(!devices_);

  base::ComponentContextForProcess()->svc()->Connect(
      device_watcher_.NewRequest());

  device_watcher_.set_error_handler(fit::bind_member(
      this, &VideoCaptureDeviceFactoryFuchsia::OnDeviceWatcherDisconnected));

  WatchDevices();
}

void VideoCaptureDeviceFactoryFuchsia::OnDeviceWatcherDisconnected(
    zx_status_t status) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // When running on a device with no camera there may be no device-watcher
  // service, in which case the device watcher channel will close with a well-
  // defined error. There is no need to log the error in that case.
  if (status != ZX_ERR_UNAVAILABLE)
    ZX_LOG(ERROR, status) << "fuchsia.camera3.DeviceWatcher disconnected.";

  // Clear the list of devices, so we don't report any camera devices while
  // DeviceWatcher is disconnected. We will try connecting DeviceWatcher again
  // when GetDevicesInfo() is called.
  devices_ = std::nullopt;
  num_pending_device_info_requests_ = 0;

  MaybeResolvePendingDeviceInfoCallbacks();
}

void VideoCaptureDeviceFactoryFuchsia::WatchDevices() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  device_watcher_->WatchDevices(fit::bind_member(
      this, &VideoCaptureDeviceFactoryFuchsia::OnWatchDevicesResult));
}

void VideoCaptureDeviceFactoryFuchsia::OnWatchDevicesResult(
    std::vector<fuchsia::camera3::WatchDevicesEvent> events) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!devices_)
    devices_.emplace();

  for (auto& e : events) {
    if (e.is_removed()) {
      auto it = devices_->find(e.removed());
      if (it == devices_->end()) {
        LOG(WARNING) << "Received device removed event for a device that "
                        "wasn't previously registered.";
        continue;
      }
      if (it->second->is_pending()) {
        // If the device info request was still pending then consider it
        // complete now. If this was the only device in pending state then all
        // callbacks will be resolved in
        // MaybeResolvePendingDeviceInfoCallbacks() called below.
        DCHECK_GT(num_pending_device_info_requests_, 0U);
        num_pending_device_info_requests_--;
      }
      devices_->erase(it);
      continue;
    }

    uint64_t id;
    if (e.is_added()) {
      id = e.added();
      if (devices_->find(id) != devices_->end()) {
        LOG(WARNING) << "Received device added event for a device that was "
                        "previously registered.";
        continue;
      }
    } else {
      id = e.existing();
      if (devices_->find(id) != devices_->end()) {
        continue;
      }
      LOG(WARNING) << "Received device exists event for a device that wasn't "
                      "previously registered.";
    }

    fuchsia::camera3::DevicePtr device;
    device_watcher_->ConnectToDevice(id, device.NewRequest());

    auto fetcher = std::make_unique<DeviceConfigFetcher>(id, std::move(device));

    // base::Unretained() is safe because the |fetcher| is owned by |this|.
    fetcher->Fetch(
        base::BindOnce(&VideoCaptureDeviceFactoryFuchsia::OnDeviceInfoFetched,
                       base::Unretained(this)));
    num_pending_device_info_requests_++;

    devices_->emplace(id, std::move(fetcher));
  }

  // Watch for further updates.
  WatchDevices();

  // Notify system monitor that the list of the devices has changed, except if
  // this is the first WatchDevices() response we've received. The first
  // time WatchDevices() is called it responds immediately with the current list
  // of the devices, i.e. there is no need to emit notification in that case. If
  // the `device_watcher_` was disconnected and reconnected later then we still
  // want to emit the notification as the list could change while
  // `device_watcher_` was disconnected. There is no need to compare the content
  // of the list: `MediaDeviceManager` will notify applications only if the list
  // has actually changed.
  if (received_initial_list_) {
    auto* system_monitor = base::SystemMonitor::Get();
    if (system_monitor) {
      system_monitor->ProcessDevicesChanged(
          base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE);
    }
  } else {
    received_initial_list_ = true;
  }

  // Calls callbacks, which may delete |this|.
  MaybeResolvePendingDeviceInfoCallbacks();
}

void VideoCaptureDeviceFactoryFuchsia::OnDeviceInfoFetched() {
  DCHECK_GT(num_pending_device_info_requests_, 0U);
  num_pending_device_info_requests_--;
  MaybeResolvePendingDeviceInfoCallbacks();
}

std::vector<VideoCaptureDeviceInfo>
VideoCaptureDeviceFactoryFuchsia::MakeDevicesInfo() {
  std::vector<VideoCaptureDeviceInfo> devices_info;
  // Leave the list empty if |devices_| isn't set (e.g. after DeviceWatcher
  // has disconnected).
  if (devices_) {
    for (auto& device : devices_.value()) {
      DCHECK(!device.second->is_pending());
      if (device.second->is_usable()) {
        devices_info.emplace_back(VideoCaptureDeviceDescriptor(
            device.second->description(), base::NumberToString(device.first),
            VideoCaptureApi::FUCHSIA_CAMERA3));
        devices_info.back().supported_formats = device.second->formats();
      }
    }
  }
  return devices_info;
}

void VideoCaptureDeviceFactoryFuchsia::
    MaybeResolvePendingDeviceInfoCallbacks() {
  if (num_pending_device_info_requests_ > 0)
    return;

  std::vector<GetDevicesInfoCallback> callbacks;
  callbacks.swap(pending_devices_info_requests_);

  auto weak_this = weak_factory_.GetWeakPtr();
  for (auto& c : callbacks) {
    auto devices_info = MakeDevicesInfo();
    std::move(c).Run(devices_info);
    if (!weak_this)
      return;
  }
}

}  // namespace media
