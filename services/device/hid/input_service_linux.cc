// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/hid/input_service_linux.h"

#include <memory>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/threading/scoped_blocking_call.h"
#include "device/base/device_monitor_linux.h"
#include "device/udev_linux/udev.h"

namespace device {

namespace {

const char kSubsystemHid[] = "hid";
const char kSubsystemInput[] = "input";
const char kSubsystemMisc[] = "misc";
const char kTypeBluetooth[] = "bluetooth";
const char kTypeUsb[] = "usb";
const char kTypeSerio[] = "serio";
const char kIdInputAccelerometer[] = "ID_INPUT_ACCELEROMETER";
const char kIdInputJoystick[] = "ID_INPUT_JOYSTICK";
const char kIdInputKey[] = "ID_INPUT_KEY";
const char kIdInputKeyboard[] = "ID_INPUT_KEYBOARD";
const char kIdInputMouse[] = "ID_INPUT_MOUSE";
const char kIdInputTablet[] = "ID_INPUT_TABLET";
const char kIdInputTouchpad[] = "ID_INPUT_TOUCHPAD";
const char kIdInputTouchscreen[] = "ID_INPUT_TOUCHSCREEN";

InputServiceLinux* g_input_service_linux = nullptr;

bool GetBoolProperty(udev_device* device, const char* key) {
  CHECK(device);
  CHECK(key);
  const char* property = udev_device_get_property_value(device, key);
  if (!property)
    return false;
  int value;
  if (!base::StringToInt(property, &value)) {
    LOG(ERROR) << "Not an integer value for " << key << " property";
    return false;
  }
  return (value != 0);
}

mojom::InputDeviceType GetDeviceType(udev_device* device) {
  // Bluetooth classic hid devices are registered under bluetooth subsystem.
  // Bluetooth LE hid devices are registered under virtual misc/hid subsystems.
  if (udev_device_get_parent_with_subsystem_devtype(device, kTypeBluetooth,
                                                    NULL) ||
      (udev_device_get_parent_with_subsystem_devtype(device, kSubsystemHid,
                                                     NULL) &&
       udev_device_get_parent_with_subsystem_devtype(device, kSubsystemMisc,
                                                     NULL))) {
    return mojom::InputDeviceType::TYPE_BLUETOOTH;
  }
  if (udev_device_get_parent_with_subsystem_devtype(device, kTypeUsb, NULL))
    return mojom::InputDeviceType::TYPE_USB;
  if (udev_device_get_parent_with_subsystem_devtype(device, kTypeSerio, NULL))
    return mojom::InputDeviceType::TYPE_SERIO;
  return mojom::InputDeviceType::TYPE_UNKNOWN;
}

std::string GetParentDeviceName(udev_device* device, const char* subsystem) {
  udev_device* parent =
      udev_device_get_parent_with_subsystem_devtype(device, subsystem, NULL);
  if (!parent)
    return std::string();
  const char* name = udev_device_get_property_value(parent, "NAME");
  if (!name)
    return std::string();
  std::string result;
  base::TrimString(name, "\"", &result);
  return result;
}

class InputServiceLinuxImpl : public InputServiceLinux,
                              public DeviceMonitorLinux::Observer {
 public:
  // Implements DeviceMonitorLinux::Observer:
  void OnDeviceAdded(udev_device* device) override;
  void OnDeviceRemoved(udev_device* device) override;

 private:
  friend class InputServiceLinux;

  InputServiceLinuxImpl();
  ~InputServiceLinuxImpl() override;

  ScopedObserver<DeviceMonitorLinux, DeviceMonitorLinux::Observer> observer_;

  DISALLOW_COPY_AND_ASSIGN(InputServiceLinuxImpl);
};

InputServiceLinuxImpl::InputServiceLinuxImpl() : observer_(this) {
  DeviceMonitorLinux* monitor = DeviceMonitorLinux::GetInstance();
  observer_.Add(monitor);
  monitor->Enumerate(base::Bind(&InputServiceLinuxImpl::OnDeviceAdded,
                                base::Unretained(this)));
}

InputServiceLinuxImpl::~InputServiceLinuxImpl() {
  // Never destroyed.
  NOTREACHED();
}

void InputServiceLinuxImpl::OnDeviceAdded(udev_device* device) {
  DCHECK(CalledOnValidThread());
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  if (!device)
    return;
  const char* devnode = udev_device_get_devnode(device);
  if (!devnode)
    return;

  auto info = mojom::InputDeviceInfo::New();
  info->id = devnode;

  const char* subsystem = udev_device_get_subsystem(device);
  if (!subsystem)
    return;
  if (strcmp(subsystem, kSubsystemHid) == 0) {
    info->subsystem = mojom::InputDeviceSubsystem::SUBSYSTEM_HID;
    info->name = GetParentDeviceName(device, kSubsystemHid);
  } else if (strcmp(subsystem, kSubsystemInput) == 0) {
    info->subsystem = mojom::InputDeviceSubsystem::SUBSYSTEM_INPUT;
    info->name = GetParentDeviceName(device, kSubsystemInput);
  } else {
    return;
  }

  info->type = GetDeviceType(device);

  info->is_accelerometer = GetBoolProperty(device, kIdInputAccelerometer);
  info->is_joystick = GetBoolProperty(device, kIdInputJoystick);
  info->is_key = GetBoolProperty(device, kIdInputKey);
  info->is_keyboard = GetBoolProperty(device, kIdInputKeyboard);
  info->is_mouse = GetBoolProperty(device, kIdInputMouse);
  info->is_tablet = GetBoolProperty(device, kIdInputTablet);
  info->is_touchpad = GetBoolProperty(device, kIdInputTouchpad);
  info->is_touchscreen = GetBoolProperty(device, kIdInputTouchscreen);

  AddDevice(std::move(info));
}

void InputServiceLinuxImpl::OnDeviceRemoved(udev_device* device) {
  DCHECK(CalledOnValidThread());
  if (!device)
    return;

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  const char* devnode = udev_device_get_devnode(device);
  if (devnode)
    RemoveDevice(devnode);
}

}  // namespace

InputServiceLinux::InputServiceLinux() {}

InputServiceLinux::~InputServiceLinux() {
  DCHECK(CalledOnValidThread());
}

// static
void InputServiceLinux::BindReceiver(
    mojo::PendingReceiver<mojom::InputDeviceManager> receiver) {
  GetInstance()->AddReceiver(std::move(receiver));
}

// static
InputServiceLinux* InputServiceLinux::GetInstance() {
  if (!HasInstance())
    g_input_service_linux = new InputServiceLinuxImpl();
  return g_input_service_linux;
}

// static
bool InputServiceLinux::HasInstance() {
  return !!g_input_service_linux;
}

// static
void InputServiceLinux::SetForTesting(
    std::unique_ptr<InputServiceLinux> service) {
  DCHECK(!HasInstance());
  DCHECK(service);
  // |service| will never be destroyed.
  g_input_service_linux = service.release();
}

void InputServiceLinux::AddReceiver(
    mojo::PendingReceiver<mojom::InputDeviceManager> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void InputServiceLinux::GetDevicesAndSetClient(
    mojo::PendingAssociatedRemote<mojom::InputDeviceManagerClient> client,
    GetDevicesCallback callback) {
  GetDevices(std::move(callback));

  if (!client.is_valid())
    return;

  clients_.Add(std::move(client));
}

void InputServiceLinux::GetDevices(GetDevicesCallback callback) {
  DCHECK(CalledOnValidThread());
  std::vector<mojom::InputDeviceInfoPtr> devices;
  for (auto& device : devices_)
    devices.push_back(device.second->Clone());

  std::move(callback).Run(std::move(devices));
}

void InputServiceLinux::AddDevice(mojom::InputDeviceInfoPtr info) {
  auto* device_info = info.get();
  for (auto& client : clients_)
    client->InputDeviceAdded(device_info->Clone());

  devices_[info->id] = std::move(info);
}

void InputServiceLinux::RemoveDevice(const std::string& id) {
  devices_.erase(id);

  for (auto& client : clients_)
    client->InputDeviceRemoved(id);
}

bool InputServiceLinux::CalledOnValidThread() const {
  return thread_checker_.CalledOnValidThread();
}

}  // namespace device
