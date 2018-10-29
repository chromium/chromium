// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/device_service.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "device/usb/mojo/device_manager_impl.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/device/bluetooth/bluetooth_system_factory.h"
#include "services/device/fingerprint/fingerprint.h"
#include "services/device/generic_sensor/sensor_provider_impl.h"
#include "services/device/geolocation/geolocation_config.h"
#include "services/device/geolocation/geolocation_context.h"
#include "services/device/geolocation/public_ip_address_geolocator.h"
#include "services/device/geolocation/public_ip_address_location_notifier.h"
#include "services/device/power_monitor/power_monitor_message_broadcaster.h"
#include "services/device/public/mojom/battery_monitor.mojom.h"
#include "services/device/serial/serial_device_enumerator_impl.h"
#include "services/device/serial/serial_io_handler_impl.h"
#include "services/device/time_zone_monitor/time_zone_monitor.h"
#include "services/device/wake_lock/wake_lock_provider.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/gfx/native_widget_types.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "jni/InterfaceRegistrar_jni.h"
#include "services/device/screen_orientation/screen_orientation_listener_android.h"
#else
#include "services/device/battery/battery_monitor_impl.h"
#include "services/device/battery/battery_status_service.h"
#include "services/device/hid/hid_manager_impl.h"
#include "services/device/vibration/vibration_manager_impl.h"
#endif

#if defined(OS_LINUX) && defined(USE_UDEV)
#include "services/device/hid/input_service_linux.h"
#endif

namespace device {

#if defined(OS_ANDROID)
std::unique_ptr<service_manager::Service> CreateDeviceService(
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& geolocation_api_key,
    bool use_gms_core_location_provider,
    const WakeLockContextCallback& wake_lock_context_callback,
    const CustomLocationProviderCallback& custom_location_provider_callback,
    const base::android::JavaRef<jobject>& java_nfc_delegate) {
  GeolocationProviderImpl::SetGeolocationConfiguration(
      url_loader_factory, geolocation_api_key,
      custom_location_provider_callback, use_gms_core_location_provider);
  return std::make_unique<DeviceService>(
      std::move(file_task_runner), std::move(io_task_runner),
      std::move(url_loader_factory), geolocation_api_key,
      wake_lock_context_callback, java_nfc_delegate);
}
#else
std::unique_ptr<service_manager::Service> CreateDeviceService(
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& geolocation_api_key,
    const CustomLocationProviderCallback& custom_location_provider_callback) {
  GeolocationProviderImpl::SetGeolocationConfiguration(
      url_loader_factory, geolocation_api_key,
      custom_location_provider_callback);
  return std::make_unique<DeviceService>(
      std::move(file_task_runner), std::move(io_task_runner),
      std::move(url_loader_factory), geolocation_api_key);
}
#endif

#if defined(OS_ANDROID)
DeviceService::DeviceService(
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& geolocation_api_key,
    const WakeLockContextCallback& wake_lock_context_callback,
    const base::android::JavaRef<jobject>& java_nfc_delegate)
    : file_task_runner_(std::move(file_task_runner)),
      io_task_runner_(std::move(io_task_runner)),
      url_loader_factory_(std::move(url_loader_factory)),
      geolocation_api_key_(geolocation_api_key),
      wake_lock_context_callback_(wake_lock_context_callback),
      java_interface_provider_initialized_(false) {
  java_nfc_delegate_.Reset(java_nfc_delegate);
}
#else
DeviceService::DeviceService(
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& geolocation_api_key)
    : file_task_runner_(std::move(file_task_runner)),
      io_task_runner_(std::move(io_task_runner)),
      url_loader_factory_(std::move(url_loader_factory)),
      geolocation_api_key_(geolocation_api_key) {}
#endif

DeviceService::~DeviceService() {
#if !defined(OS_ANDROID)
  device::BatteryStatusService::GetInstance()->Shutdown();
#endif
}

void DeviceService::OnStart() {
  registry_.AddInterface<mojom::Fingerprint>(base::Bind(
      &DeviceService::BindFingerprintRequest, base::Unretained(this)));
  registry_.AddInterface<mojom::GeolocationConfig>(base::BindRepeating(
      &DeviceService::BindGeolocationConfigRequest, base::Unretained(this)));
  registry_.AddInterface<mojom::GeolocationContext>(base::Bind(
      &DeviceService::BindGeolocationContextRequest, base::Unretained(this)));
  registry_.AddInterface<mojom::GeolocationControl>(base::Bind(
      &DeviceService::BindGeolocationControlRequest, base::Unretained(this)));
  registry_.AddInterface<mojom::PowerMonitor>(base::Bind(
      &DeviceService::BindPowerMonitorRequest, base::Unretained(this)));
  registry_.AddInterface<mojom::PublicIpAddressGeolocationProvider>(
      base::Bind(&DeviceService::BindPublicIpAddressGeolocationProviderRequest,
                 base::Unretained(this)));
  registry_.AddInterface<mojom::ScreenOrientationListener>(
      base::Bind(&DeviceService::BindScreenOrientationListenerRequest,
                 base::Unretained(this)));
  registry_.AddInterface<mojom::SensorProvider>(base::Bind(
      &DeviceService::BindSensorProviderRequest, base::Unretained(this)));
  registry_.AddInterface<mojom::TimeZoneMonitor>(base::Bind(
      &DeviceService::BindTimeZoneMonitorRequest, base::Unretained(this)));
  registry_.AddInterface<mojom::WakeLockProvider>(base::Bind(
      &DeviceService::BindWakeLockProviderRequest, base::Unretained(this)));
  registry_.AddInterface<mojom::SerialDeviceEnumerator>(
      base::Bind(&DeviceService::BindSerialDeviceEnumeratorRequest,
                 base::Unretained(this)));
  registry_.AddInterface<mojom::SerialIoHandler>(base::Bind(
      &DeviceService::BindSerialIoHandlerRequest, base::Unretained(this)));
  registry_.AddInterface<mojom::UsbDeviceManager>(base::Bind(
      &DeviceService::BindUsbDeviceManagerRequest, base::Unretained(this)));

#if defined(OS_ANDROID)
  registry_.AddInterface(GetJavaInterfaceProvider()
                             ->CreateInterfaceFactory<mojom::BatteryMonitor>());
  registry_.AddInterface(
      GetJavaInterfaceProvider()->CreateInterfaceFactory<mojom::NFCProvider>());
  registry_.AddInterface(
      GetJavaInterfaceProvider()
          ->CreateInterfaceFactory<mojom::VibrationManager>());
#else
  registry_.AddInterface<mojom::BatteryMonitor>(base::Bind(
      &DeviceService::BindBatteryMonitorRequest, base::Unretained(this)));
  registry_.AddInterface<mojom::HidManager>(base::Bind(
      &DeviceService::BindHidManagerRequest, base::Unretained(this)));
  registry_.AddInterface<mojom::NFCProvider>(base::Bind(
      &DeviceService::BindNFCProviderRequest, base::Unretained(this)));
  registry_.AddInterface<mojom::VibrationManager>(base::Bind(
      &DeviceService::BindVibrationManagerRequest, base::Unretained(this)));
#endif

#if defined(OS_CHROMEOS)
  registry_.AddInterface<mojom::BluetoothSystemFactory>(
      base::BindRepeating(&DeviceService::BindBluetoothSystemFactoryRequest,
                          base::Unretained(this)));
  registry_.AddInterface<mojom::MtpManager>(base::BindRepeating(
      &DeviceService::BindMtpManagerRequest, base::Unretained(this)));
#endif

#if defined(OS_LINUX) && defined(USE_UDEV)
  registry_.AddInterface<mojom::InputDeviceManager>(base::Bind(
      &DeviceService::BindInputDeviceManagerRequest, base::Unretained(this)));
#endif
}

void DeviceService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

#if !defined(OS_ANDROID)
void DeviceService::BindBatteryMonitorRequest(
    mojom::BatteryMonitorRequest request) {
  BatteryMonitorImpl::Create(std::move(request));
}

void DeviceService::BindHidManagerRequest(mojom::HidManagerRequest request) {
  if (!hid_manager_)
    hid_manager_ = std::make_unique<HidManagerImpl>();
  hid_manager_->AddBinding(std::move(request));
}

void DeviceService::BindNFCProviderRequest(mojom::NFCProviderRequest request) {
  LOG(ERROR) << "NFC is only supported on Android";
  NOTREACHED();
}

void DeviceService::BindVibrationManagerRequest(
    mojom::VibrationManagerRequest request) {
  VibrationManagerImpl::Create(std::move(request));
}
#endif

#if defined(OS_CHROMEOS)
void DeviceService::BindBluetoothSystemFactoryRequest(
    mojom::BluetoothSystemFactoryRequest request) {
  BluetoothSystemFactory::CreateFactory(std::move(request));
}

void DeviceService::BindMtpManagerRequest(mojom::MtpManagerRequest request) {
  if (!mtp_device_manager_)
    mtp_device_manager_ = MtpDeviceManager::Initialize();
  mtp_device_manager_->AddBinding(std::move(request));
}
#endif

#if defined(OS_LINUX) && defined(USE_UDEV)
void DeviceService::BindInputDeviceManagerRequest(
    mojom::InputDeviceManagerRequest request) {
  file_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&InputServiceLinux::BindRequest, std::move(request)));
}
#endif

void DeviceService::BindFingerprintRequest(mojom::FingerprintRequest request) {
  Fingerprint::Create(std::move(request));
}

void DeviceService::BindGeolocationConfigRequest(
    mojom::GeolocationConfigRequest request) {
  GeolocationConfig::Create(std::move(request));
}

void DeviceService::BindGeolocationContextRequest(
    mojom::GeolocationContextRequest request) {
  GeolocationContext::Create(std::move(request));
}

void DeviceService::BindGeolocationControlRequest(
    mojom::GeolocationControlRequest request) {
  GeolocationProviderImpl::GetInstance()->BindGeolocationControlRequest(
      std::move(request));
}

void DeviceService::BindPowerMonitorRequest(
    mojom::PowerMonitorRequest request) {
  if (!power_monitor_message_broadcaster_) {
    power_monitor_message_broadcaster_ =
        std::make_unique<PowerMonitorMessageBroadcaster>();
  }
  power_monitor_message_broadcaster_->Bind(std::move(request));
}

void DeviceService::BindPublicIpAddressGeolocationProviderRequest(
    mojom::PublicIpAddressGeolocationProviderRequest request) {
  if (!public_ip_address_geolocation_provider_) {
    public_ip_address_geolocation_provider_ =
        std::make_unique<PublicIpAddressGeolocationProvider>(
            url_loader_factory_, geolocation_api_key_);
  }
  public_ip_address_geolocation_provider_->Bind(std::move(request));
}

void DeviceService::BindScreenOrientationListenerRequest(
    mojom::ScreenOrientationListenerRequest request) {
#if defined(OS_ANDROID)
  if (io_task_runner_) {
    io_task_runner_->PostTask(
        FROM_HERE, base::Bind(&ScreenOrientationListenerAndroid::Create,
                              base::Passed(&request)));
  }
#endif
}

void DeviceService::BindSensorProviderRequest(
    mojom::SensorProviderRequest request) {
  if (io_task_runner_) {
    io_task_runner_->PostTask(
        FROM_HERE, base::Bind(&device::SensorProviderImpl::Create,
                              file_task_runner_, base::Passed(&request)));
  }
}

void DeviceService::BindTimeZoneMonitorRequest(
    mojom::TimeZoneMonitorRequest request) {
  if (!time_zone_monitor_)
    time_zone_monitor_ = TimeZoneMonitor::Create(file_task_runner_);
  time_zone_monitor_->Bind(std::move(request));
}

void DeviceService::BindWakeLockProviderRequest(
    mojom::WakeLockProviderRequest request) {
  WakeLockProvider::Create(std::move(request), file_task_runner_,
                           wake_lock_context_callback_);
}

void DeviceService::BindSerialDeviceEnumeratorRequest(
    mojom::SerialDeviceEnumeratorRequest request) {
#if (defined(OS_LINUX) && defined(USE_UDEV)) || defined(OS_WIN) || \
    defined(OS_MACOSX)
  SerialDeviceEnumeratorImpl::Create(std::move(request));
#endif
}

void DeviceService::BindSerialIoHandlerRequest(
    mojom::SerialIoHandlerRequest request) {
#if (defined(OS_LINUX) && defined(USE_UDEV)) || defined(OS_WIN) || \
    defined(OS_MACOSX)
  if (io_task_runner_) {
    io_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&SerialIoHandlerImpl::Create, base::Passed(&request),
                   base::ThreadTaskRunnerHandle::Get()));
  }
#endif
}

void DeviceService::BindUsbDeviceManagerRequest(
    mojom::UsbDeviceManagerRequest request) {
  if (!usb_device_manager_)
    usb_device_manager_ = std::make_unique<usb::DeviceManagerImpl>();

  usb_device_manager_->AddBinding(std::move(request));
}

#if defined(OS_ANDROID)
service_manager::InterfaceProvider* DeviceService::GetJavaInterfaceProvider() {
  if (!java_interface_provider_initialized_) {
    service_manager::mojom::InterfaceProviderPtr provider;
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_InterfaceRegistrar_createInterfaceRegistryForContext(
        env, mojo::MakeRequest(&provider).PassMessagePipe().release().value(),
        java_nfc_delegate_);
    java_interface_provider_.Bind(std::move(provider));

    java_interface_provider_initialized_ = true;
  }

  return &java_interface_provider_;
}
#endif

}  // namespace device
