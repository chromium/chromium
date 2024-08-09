// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_DEVICE_SERVICE_H_
#define SERVICES_DEVICE_DEVICE_SERVICE_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/threading/sequence_bound.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/device/geolocation/geolocation_provider.h"
#include "services/device/geolocation/geolocation_provider_impl.h"
#include "services/device/geolocation/public_ip_address_geolocation_provider.h"
#include "services/device/public/cpp/compute_pressure/buildflags.h"
#include "services/device/public/mojom/battery_monitor.mojom.h"
#include "services/device/public/mojom/device_service.mojom.h"
#include "services/device/public/mojom/fingerprint.mojom.h"
#include "services/device/public/mojom/geolocation.mojom.h"
#include "services/device/public/mojom/geolocation_config.mojom.h"
#include "services/device/public/mojom/geolocation_context.mojom.h"
#include "services/device/public/mojom/geolocation_control.mojom.h"
#include "services/device/public/mojom/geolocation_internals.mojom.h"
#include "services/device/public/mojom/power_monitor.mojom.h"
#include "services/device/public/mojom/screen_orientation.mojom.h"
#include "services/device/public/mojom/sensor_provider.mojom.h"
#include "services/device/public/mojom/serial.mojom.h"
#include "services/device/public/mojom/time_zone_monitor.mojom.h"
#include "services/device/public/mojom/usb_manager.mojom.h"
#include "services/device/public/mojom/usb_manager_test.mojom.h"
#include "services/device/public/mojom/vibration_manager.mojom.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"
#include "services/device/usb/mojo/device_manager_impl.h"
#include "services/device/usb/mojo/device_manager_test.h"
#include "services/device/wake_lock/wake_lock_context.h"
#include "services/device/wake_lock/wake_lock_provider.h"
#include "services/service_manager/public/cpp/interface_provider.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"
#include "services/device/public/mojom/nfc_provider.mojom.h"
#else
#include "services/device/public/mojom/hid.mojom.h"
#endif

#if BUILDFLAG(ENABLE_COMPUTE_PRESSURE)
#include "services/device/public/mojom/pressure_manager.mojom.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "services/device/media_transfer_protocol/mtp_device_manager.h"
#endif

#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && defined(USE_UDEV)
#include "services/device/public/mojom/input_service.mojom.h"
#endif

namespace base {
class SingleThreadTaskRunner;
}

namespace network {
class NetworkConnectionTracker;
class SharedURLLoaderFactory;
}  // namespace network

namespace device {

#if !BUILDFLAG(IS_ANDROID)
class HidManagerImpl;
class SerialPortManagerImpl;
#endif

#if BUILDFLAG(ENABLE_COMPUTE_PRESSURE)
class PressureManagerImpl;
#endif

class DeviceService;
class GeolocationSystemPermissionManager;
class PowerMonitorMessageBroadcaster;
class PublicIpAddressLocationNotifier;
class SensorProviderImpl;
class TimeZoneMonitor;

// NOTE: See the comments on the definitions of PublicIpAddressLocationNotifier,
// |WakeLockContextCallback|, |CustomLocationProviderCallback| and
// NFCDelegate.java to understand the semantics and usage of these parameters.
struct DeviceServiceParams {
  DeviceServiceParams();
  ~DeviceServiceParams();

  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory;
  raw_ptr<network::NetworkConnectionTracker> network_connection_tracker =
      nullptr;
  std::string geolocation_api_key;
  CustomLocationProviderCallback custom_location_provider_callback;
  bool use_gms_core_location_provider = false;
  raw_ptr<GeolocationSystemPermissionManager>
      geolocation_system_permission_manager = nullptr;
  WakeLockContextCallback wake_lock_context_callback;

#if BUILDFLAG(IS_ANDROID)
  base::android::ScopedJavaGlobalRef<jobject> java_nfc_delegate;
#endif  // BUILDFLAG(IS_ANDROID)
};

std::unique_ptr<DeviceService> CreateDeviceService(
    std::unique_ptr<DeviceServiceParams> params,
    mojo::PendingReceiver<mojom::DeviceService> receiver);

class DeviceService : public mojom::DeviceService {
 public:
  DeviceService(std::unique_ptr<DeviceServiceParams> params,
                mojo::PendingReceiver<mojom::DeviceService> receiver);

  DeviceService(const DeviceService&) = delete;
  DeviceService& operator=(const DeviceService&) = delete;

  ~DeviceService() override;

  void AddReceiver(mojo::PendingReceiver<mojom::DeviceService> receiver);

  void SetSensorProviderImplForTesting(
      std::unique_ptr<SensorProviderImpl> sensor_provider);

  // Supports global override of GeolocationContext binding within the service.
  using GeolocationContextBinder = base::RepeatingCallback<void(
      mojo::PendingReceiver<mojom::GeolocationContext>)>;
  static void OverrideGeolocationContextBinderForTesting(
      GeolocationContextBinder binder);

#if BUILDFLAG(ENABLE_COMPUTE_PRESSURE)
  // Supports global override of PressureManager binding within the service.
  using PressureManagerBinder = base::RepeatingCallback<void(
      mojo::PendingReceiver<mojom::PressureManager>)>;
  static void OverridePressureManagerBinderForTesting(
      PressureManagerBinder binder);
#endif

  // Supports global override of TimeZoneMonitor binding within the service.
  using TimeZoneMonitorBinder = base::RepeatingCallback<void(
      mojo::PendingReceiver<mojom::TimeZoneMonitor>)>;
  static void OverrideTimeZoneMonitorBinderForTesting(
      TimeZoneMonitorBinder binder);

#if BUILDFLAG(IS_ANDROID)
  // Allows tests to override how frame hosts bind NFCProvider receivers.
  using NFCProviderBinder = base::RepeatingCallback<void(
      mojo::PendingReceiver<device::mojom::NFCProvider>)>;
  static void OverrideNFCProviderBinderForTesting(NFCProviderBinder binder);
#endif

 private:
  // mojom::DeviceService implementation:
  void BindFingerprint(
      mojo::PendingReceiver<mojom::Fingerprint> receiver) override;
  void BindGeolocationConfig(
      mojo::PendingReceiver<mojom::GeolocationConfig> receiver) override;
  void BindGeolocationContext(
      mojo::PendingReceiver<mojom::GeolocationContext> receiver) override;
  void BindGeolocationControl(
      mojo::PendingReceiver<mojom::GeolocationControl> receiver) override;
  void BindGeolocationInternals(
      mojo::PendingReceiver<mojom::GeolocationInternals> receiver) override;

#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && defined(USE_UDEV)
  void BindInputDeviceManager(
      mojo::PendingReceiver<mojom::InputDeviceManager> receiver) override;
#endif

  void BindBatteryMonitor(
      mojo::PendingReceiver<mojom::BatteryMonitor> receiver) override;

#if BUILDFLAG(ENABLE_COMPUTE_PRESSURE)
  void BindPressureManager(
      mojo::PendingReceiver<mojom::PressureManager> receiver) override;
#endif

#if BUILDFLAG(IS_ANDROID)
  void BindNFCProvider(
      mojo::PendingReceiver<mojom::NFCProvider> receiver) override;
#endif

  void BindVibrationManager(
      mojo::PendingReceiver<mojom::VibrationManager> receiver,
      mojo::PendingRemote<mojom::VibrationManagerListener> listener) override;

#if !BUILDFLAG(IS_ANDROID)
  void BindHidManager(
      mojo::PendingReceiver<mojom::HidManager> receiver) override;
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void BindMtpManager(
      mojo::PendingReceiver<mojom::MtpManager> receiver) override;
#endif

  void BindPowerMonitor(
      mojo::PendingReceiver<mojom::PowerMonitor> receiver) override;

  void BindPublicIpAddressGeolocationProvider(
      mojo::PendingReceiver<mojom::PublicIpAddressGeolocationProvider> receiver)
      override;

  void BindScreenOrientationListener(
      mojo::PendingReceiver<mojom::ScreenOrientationListener> receiver)
      override;

  void BindSensorProvider(
      mojo::PendingReceiver<mojom::SensorProvider> receiver) override;

  void BindSerialPortManager(
      mojo::PendingReceiver<mojom::SerialPortManager> receiver) override;

  void BindTimeZoneMonitor(
      mojo::PendingReceiver<mojom::TimeZoneMonitor> receiver) override;

  void BindWakeLockProvider(
      mojo::PendingReceiver<mojom::WakeLockProvider> receiver) override;

  void BindUsbDeviceManager(
      mojo::PendingReceiver<mojom::UsbDeviceManager> receiver) override;

  void BindUsbDeviceManagerTest(
      mojo::PendingReceiver<mojom::UsbDeviceManagerTest> receiver) override;

  mojo::ReceiverSet<mojom::DeviceService> receivers_;
  std::unique_ptr<PowerMonitorMessageBroadcaster>
      power_monitor_message_broadcaster_;
  std::unique_ptr<PublicIpAddressGeolocationProvider>
      public_ip_address_geolocation_provider_;
  std::unique_ptr<SensorProviderImpl> sensor_provider_;
  std::unique_ptr<TimeZoneMonitor> time_zone_monitor_;
  std::unique_ptr<usb::DeviceManagerImpl> usb_device_manager_;
  std::unique_ptr<usb::DeviceManagerTest> usb_device_manager_test_;
  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  raw_ptr<network::NetworkConnectionTracker> network_connection_tracker_;

  const std::string geolocation_api_key_;
  WakeLockContextCallback wake_lock_context_callback_;
  WakeLockProvider wake_lock_provider_;

#if BUILDFLAG(IS_ANDROID)
  // Binds |java_interface_provider_| to an interface registry that exposes
  // factories for the interfaces that are provided via Java on Android.
  service_manager::InterfaceProvider* GetJavaInterfaceProvider();

  // InterfaceProvider that is bound to the Java-side interface registry.
  service_manager::InterfaceProvider java_interface_provider_{
      base::SingleThreadTaskRunner::GetCurrentDefault()};

  bool java_interface_provider_initialized_ = false;

  base::android::ScopedJavaGlobalRef<jobject> java_nfc_delegate_;
#else
  std::unique_ptr<HidManagerImpl> hid_manager_;
#endif

#if BUILDFLAG(ENABLE_COMPUTE_PRESSURE)
  std::unique_ptr<PressureManagerImpl> pressure_manager_;
#endif

#if defined(IS_SERIAL_ENABLED_PLATFORM)
  base::SequenceBound<SerialPortManagerImpl> serial_port_manager_;
#endif  // defined(IS_SERIAL_ENABLED_PLATFORM)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<MtpDeviceManager> mtp_device_manager_;
#endif
};

}  // namespace device

#endif  // SERVICES_DEVICE_DEVICE_SERVICE_H_
