// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/device_service.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/device/binder_overrides.h"
#include "services/device/fingerprint/fingerprint.h"
#include "services/device/generic_sensor/platform_sensor_provider.h"
#include "services/device/generic_sensor/sensor_provider_impl.h"
#include "services/device/geolocation/geolocation_config.h"
#include "services/device/geolocation/geolocation_context.h"
#include "services/device/geolocation/public_ip_address_geolocator.h"
#include "services/device/geolocation/public_ip_address_location_notifier.h"
#include "services/device/power_monitor/power_monitor_message_broadcaster.h"
#include "services/device/public/mojom/battery_monitor.mojom.h"
#include "services/device/serial/serial_port_manager_impl.h"
#include "services/device/time_zone_monitor/time_zone_monitor.h"
#include "services/device/vibration/vibration_manager_impl.h"
#include "services/device/wake_lock/wake_lock_provider.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/gfx/native_widget_types.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "services/device/device_service_jni_headers/InterfaceRegistrar_jni.h"
#include "services/device/screen_orientation/screen_orientation_listener_android.h"
#include "services/device/vibration/vibration_manager_android.h"
#else
#include "services/device/battery/battery_monitor_impl.h"
#include "services/device/battery/battery_status_service.h"
#include "services/device/hid/hid_manager_impl.h"
#endif

#if BUILDFLAG(ENABLE_COMPUTE_PRESSURE)
#include "services/device/compute_pressure/pressure_manager_impl.h"
#endif

#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && defined(USE_UDEV)
#include "services/device/hid/input_service_linux.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#endif

namespace {

#if !BUILDFLAG(IS_ANDROID)
constexpr bool IsLaCrOS() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return true;
#else
  return false;
#endif
}
#endif

#if !BUILDFLAG(IS_ANDROID)
void BindLaCrOSHidManager(
    mojo::PendingReceiver<device::mojom::HidManager> receiver) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // LaCrOS does not have direct access to the permission_broker service over
  // D-Bus. Use the HidManager interface from ash-chrome instead.
  auto* lacros_service = chromeos::LacrosService::Get();
  DCHECK(lacros_service);
  // If the Hid manager is not available, then the pending receiver is deleted.
  if (lacros_service->IsAvailable<device::mojom::HidManager>()) {
    lacros_service->GetRemote<device::mojom::HidManager>()->AddReceiver(
        std::move(receiver));
  }
#endif
}
#endif

}  // namespace

namespace device {

DeviceServiceParams::DeviceServiceParams() = default;

DeviceServiceParams::~DeviceServiceParams() = default;

std::unique_ptr<DeviceService> CreateDeviceService(
    std::unique_ptr<DeviceServiceParams> params,
    mojo::PendingReceiver<mojom::DeviceService> receiver) {
  GeolocationProviderImpl::SetGeolocationConfiguration(
      params->url_loader_factory, params->geolocation_api_key,
      params->custom_location_provider_callback,
      params->geolocation_system_permission_manager,
      params->use_gms_core_location_provider);
  return std::make_unique<DeviceService>(std::move(params),
                                         std::move(receiver));
}

DeviceService::DeviceService(
    std::unique_ptr<DeviceServiceParams> params,
    mojo::PendingReceiver<mojom::DeviceService> receiver)
    : file_task_runner_(std::move(params->file_task_runner)),
      io_task_runner_(std::move(params->io_task_runner)),
      url_loader_factory_(std::move(params->url_loader_factory)),
      network_connection_tracker_(params->network_connection_tracker),
      geolocation_api_key_(params->geolocation_api_key),
      wake_lock_context_callback_(params->wake_lock_context_callback),
      wake_lock_provider_(file_task_runner_,
                          params->wake_lock_context_callback) {
  receivers_.Add(this, std::move(receiver));

#if BUILDFLAG(IS_ANDROID)
  java_nfc_delegate_.Reset(params->java_nfc_delegate);
#endif

#if defined(IS_SERIAL_ENABLED_PLATFORM)
#if BUILDFLAG(IS_MAC)
  // On macOS the SerialDeviceEnumerator needs to run on the UI thread so that
  // it has access to a CFRunLoop where it can register a notification source.
  auto serial_port_manager_task_runner =
      base::SingleThreadTaskRunner::GetCurrentDefault();
#else
  // On other platforms it must be allowed to do blocking IO.
  auto serial_port_manager_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
#endif
  serial_port_manager_.emplace(
      std::move(serial_port_manager_task_runner), io_task_runner_,
      base::SingleThreadTaskRunner::GetCurrentDefault());
#endif  // defined(IS_SERIAL_ENABLED_PLATFORM)

#if !BUILDFLAG(IS_ANDROID)
  // Ensure that the battery backend is initialized now; otherwise it may end up
  // getting initialized on access during destruction, when it's no longer safe
  // to initialize.
  device::BatteryStatusService::GetInstance();
#endif  // !BUILDFLAG(IS_ANDROID)
}

DeviceService::~DeviceService() {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
  // NOTE: We don't call this on Chrome OS due to https://crbug.com/856771, as
  // Shutdown() implicitly depends on DBusThreadManager, which may already be
  // destroyed by the time DeviceService is destroyed. Fortunately on Chrome OS
  // it's not really important that this runs anyway.
  device::BatteryStatusService::GetInstance()->Shutdown();
#endif
}

void DeviceService::AddReceiver(
    mojo::PendingReceiver<mojom::DeviceService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void DeviceService::SetSensorProviderImplForTesting(
    std::unique_ptr<SensorProviderImpl> sensor_provider) {
  CHECK(!sensor_provider_);
  sensor_provider_ = std::move(sensor_provider);
}

// static
void DeviceService::OverrideGeolocationContextBinderForTesting(
    GeolocationContextBinder binder) {
  internal::GetGeolocationContextBinderOverride() = std::move(binder);
}

#if BUILDFLAG(ENABLE_COMPUTE_PRESSURE)
// static
void DeviceService::OverridePressureManagerBinderForTesting(
    PressureManagerBinder binder) {
  internal::GetPressureManagerBinderOverride() = std::move(binder);
}
#endif

// static
void DeviceService::OverrideTimeZoneMonitorBinderForTesting(
    TimeZoneMonitorBinder binder) {
  internal::GetTimeZoneMonitorBinderOverride() = std::move(binder);
}

void DeviceService::BindBatteryMonitor(
    mojo::PendingReceiver<mojom::BatteryMonitor> receiver) {
#if BUILDFLAG(IS_ANDROID)
  GetJavaInterfaceProvider()->GetInterface(std::move(receiver));
#else
  BatteryMonitorImpl::Create(std::move(receiver));
#endif
}

#if BUILDFLAG(ENABLE_COMPUTE_PRESSURE)
void DeviceService::BindPressureManager(
    mojo::PendingReceiver<mojom::PressureManager> receiver) {
  const auto& binder_override = internal::GetPressureManagerBinderOverride();
  if (binder_override) {
    binder_override.Run(std::move(receiver));
    return;
  }

  if (!pressure_manager_)
    pressure_manager_ = PressureManagerImpl::Create();
  pressure_manager_->Bind(std::move(receiver));
}
#endif

#if BUILDFLAG(IS_ANDROID)
// static
void DeviceService::OverrideNFCProviderBinderForTesting(
    NFCProviderBinder binder) {
  internal::GetNFCProviderBinderOverride() = std::move(binder);
}

void DeviceService::BindNFCProvider(
    mojo::PendingReceiver<mojom::NFCProvider> receiver) {
  const auto& binder_override = internal::GetNFCProviderBinderOverride();
  if (binder_override)
    binder_override.Run(std::move(receiver));
  else
    GetJavaInterfaceProvider()->GetInterface(std::move(receiver));
}
#endif

void DeviceService::BindVibrationManager(
    mojo::PendingReceiver<mojom::VibrationManager> receiver,
    mojo::PendingRemote<mojom::VibrationManagerListener> listener) {
#if BUILDFLAG(IS_ANDROID)
  VibrationManagerAndroid::Create(std::move(receiver), std::move(listener));
#else
  VibrationManagerImpl::Create(std::move(receiver), std::move(listener));
#endif
}

#if !BUILDFLAG(IS_ANDROID)
void DeviceService::BindHidManager(
    mojo::PendingReceiver<mojom::HidManager> receiver) {
  if (IsLaCrOS() && !HidManagerImpl::IsHidServiceTesting()) {
    BindLaCrOSHidManager(std::move(receiver));
  } else {
    if (!hid_manager_)
      hid_manager_ = std::make_unique<HidManagerImpl>();
    hid_manager_->AddReceiver(std::move(receiver));
  }
}
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
void DeviceService::BindMtpManager(
    mojo::PendingReceiver<mojom::MtpManager> receiver) {
  if (!mtp_device_manager_)
    mtp_device_manager_ = MtpDeviceManager::Initialize();
  mtp_device_manager_->AddReceiver(std::move(receiver));
}
#endif

#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && defined(USE_UDEV)
void DeviceService::BindInputDeviceManager(
    mojo::PendingReceiver<mojom::InputDeviceManager> receiver) {
  file_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&InputServiceLinux::BindReceiver, std::move(receiver)));
}
#endif

void DeviceService::BindFingerprint(
    mojo::PendingReceiver<mojom::Fingerprint> receiver) {
  Fingerprint::Create(std::move(receiver));
}

void DeviceService::BindGeolocationConfig(
    mojo::PendingReceiver<mojom::GeolocationConfig> receiver) {
  GeolocationConfig::Create(std::move(receiver));
}

void DeviceService::BindGeolocationContext(
    mojo::PendingReceiver<mojom::GeolocationContext> receiver) {
  const auto& binder_override = internal::GetGeolocationContextBinderOverride();
  if (binder_override) {
    binder_override.Run(std::move(receiver));
    return;
  }

  GeolocationContext::Create(std::move(receiver));
}

void DeviceService::BindGeolocationControl(
    mojo::PendingReceiver<mojom::GeolocationControl> receiver) {
  GeolocationProviderImpl::GetInstance()->BindGeolocationControlReceiver(
      std::move(receiver));
}

void DeviceService::BindGeolocationInternals(
    mojo::PendingReceiver<mojom::GeolocationInternals> receiver) {
  GeolocationProviderImpl::GetInstance()->BindGeolocationInternalsReceiver(
      std::move(receiver));
}

void DeviceService::BindPowerMonitor(
    mojo::PendingReceiver<mojom::PowerMonitor> receiver) {
  if (!power_monitor_message_broadcaster_) {
    power_monitor_message_broadcaster_ =
        std::make_unique<PowerMonitorMessageBroadcaster>();
  }
  power_monitor_message_broadcaster_->Bind(std::move(receiver));
}

void DeviceService::BindPublicIpAddressGeolocationProvider(
    mojo::PendingReceiver<mojom::PublicIpAddressGeolocationProvider> receiver) {
  if (!public_ip_address_geolocation_provider_) {
    public_ip_address_geolocation_provider_ =
        std::make_unique<PublicIpAddressGeolocationProvider>(
            url_loader_factory_, network_connection_tracker_,
            geolocation_api_key_);
  }
  public_ip_address_geolocation_provider_->Bind(std::move(receiver));
}

void DeviceService::BindScreenOrientationListener(
    mojo::PendingReceiver<mojom::ScreenOrientationListener> receiver) {
#if BUILDFLAG(IS_ANDROID)
  if (io_task_runner_) {
    io_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ScreenOrientationListenerAndroid::Create,
                                  std::move(receiver)));
  }
#endif
}

void DeviceService::BindSensorProvider(
    mojo::PendingReceiver<mojom::SensorProvider> receiver) {
  if (!sensor_provider_) {
    auto platform_provider = PlatformSensorProvider::Create();
    if (!platform_provider)
      return;
    sensor_provider_ =
        std::make_unique<SensorProviderImpl>(std::move(platform_provider));
  }
  sensor_provider_->Bind(std::move(receiver));
}

void DeviceService::BindSerialPortManager(
    mojo::PendingReceiver<mojom::SerialPortManager> receiver) {
#if defined(IS_SERIAL_ENABLED_PLATFORM)
  serial_port_manager_.AsyncCall(&SerialPortManagerImpl::Bind, FROM_HERE)
      .WithArgs(std::move(receiver));
#else   // defined(IS_SERIAL_ENABLED_PLATFORM)
  NOTREACHED_IN_MIGRATION() << "Serial devices not supported on this platform.";
#endif  // defined(IS_SERIAL_ENABLED_PLATFORM)
}

void DeviceService::BindTimeZoneMonitor(
    mojo::PendingReceiver<mojom::TimeZoneMonitor> receiver) {
  const auto& binder_override = internal::GetTimeZoneMonitorBinderOverride();
  if (binder_override) {
    binder_override.Run(std::move(receiver));
    return;
  }

  if (!time_zone_monitor_)
    time_zone_monitor_ = TimeZoneMonitor::Create(file_task_runner_);
  time_zone_monitor_->Bind(std::move(receiver));
}

void DeviceService::BindWakeLockProvider(
    mojo::PendingReceiver<mojom::WakeLockProvider> receiver) {
  wake_lock_provider_.AddBinding(std::move(receiver));
}

void DeviceService::BindUsbDeviceManager(
    mojo::PendingReceiver<mojom::UsbDeviceManager> receiver) {
  // TODO(crbug.com/40141825): usb::DeviceManagerImpl depends on the
  // permission_broker service on Chromium OS. We will need to redirect
  // connections for LaCrOS here.
  if (!usb_device_manager_)
    usb_device_manager_ = std::make_unique<usb::DeviceManagerImpl>();

  usb_device_manager_->AddReceiver(std::move(receiver));
}

void DeviceService::BindUsbDeviceManagerTest(
    mojo::PendingReceiver<mojom::UsbDeviceManagerTest> receiver) {
  // TODO(crbug.com/40141825): usb::DeviceManagerImpl depends on the
  // permission_broker service on Chromium OS. We will need to redirect
  // connections for LaCrOS here.
  if (!usb_device_manager_)
    usb_device_manager_ = std::make_unique<usb::DeviceManagerImpl>();

  if (!usb_device_manager_test_) {
    usb_device_manager_test_ = std::make_unique<usb::DeviceManagerTest>(
        usb_device_manager_->GetUsbService());
  }

  usb_device_manager_test_->BindReceiver(std::move(receiver));
}

#if BUILDFLAG(IS_ANDROID)
service_manager::InterfaceProvider* DeviceService::GetJavaInterfaceProvider() {
  if (!java_interface_provider_initialized_) {
    mojo::PendingRemote<service_manager::mojom::InterfaceProvider> provider;
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_InterfaceRegistrar_createInterfaceRegistryForContext(
        env,
        provider.InitWithNewPipeAndPassReceiver().PassPipe().release().value(),
        java_nfc_delegate_);
    java_interface_provider_.Bind(std::move(provider));

    java_interface_provider_initialized_ = true;
  }

  return &java_interface_provider_;
}
#endif

}  // namespace device
