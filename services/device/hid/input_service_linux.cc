// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/hid/input_service_linux.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/sequence_bound.h"
#include "device/udev_linux/scoped_udev.h"
#include "device/udev_linux/udev_watcher.h"

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

class InputServiceLinuxImpl : public InputServiceLinux {
 public:
  InputServiceLinuxImpl(const InputServiceLinuxImpl&) = delete;
  InputServiceLinuxImpl& operator=(const InputServiceLinuxImpl&) = delete;

 private:
  friend class InputServiceLinux;

  class BlockingTaskRunnerHelper;

  InputServiceLinuxImpl();
  ~InputServiceLinuxImpl() override;

  base::SequenceBound<BlockingTaskRunnerHelper> helper_;
  base::WeakPtrFactory<InputServiceLinuxImpl> weak_factory_{this};
};

class InputServiceLinuxImpl::BlockingTaskRunnerHelper
    : public UdevWatcher::Observer {
 public:
  BlockingTaskRunnerHelper(base::WeakPtr<InputServiceLinuxImpl> service,
                           scoped_refptr<base::SequencedTaskRunner> task_runner)
      : service_(service), task_runner_(std::move(task_runner)) {
    watcher_ = UdevWatcher::StartWatching(
        this, {{
                  UdevWatcher::Filter(kSubsystemHid, ""),
                  UdevWatcher::Filter(kSubsystemInput, ""),
              }});
    watcher_->EnumerateExistingDevices();
  }

  BlockingTaskRunnerHelper(const BlockingTaskRunnerHelper&) = delete;
  BlockingTaskRunnerHelper& operator=(const BlockingTaskRunnerHelper&) = delete;

  ~BlockingTaskRunnerHelper() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  // UdevWatcher::Observer
  void OnDeviceAdded(ScopedUdevDevicePtr device) override;
  void OnDeviceRemoved(ScopedUdevDevicePtr device) override;
  void OnDeviceChanged(ScopedUdevDevicePtr) override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);
  std::unique_ptr<UdevWatcher> watcher_;

  // This weak pointer is only valid when checked on this task runner.
  base::WeakPtr<InputServiceLinuxImpl> service_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

InputServiceLinuxImpl::InputServiceLinuxImpl() {
  helper_ = base::SequenceBound<BlockingTaskRunnerHelper>(
      // These task traits are to be used for posting blocking tasks to the
      // thread pool. This helper is for running device event handler from
      // UdevWatcher so it uses USER_VISIBLE priority for being important to
      // notify users new device events, and CONTINUE_ON_SHUTDOWN behavior for
      // not blocking shutdown.
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}),
      weak_factory_.GetWeakPtr(),
      base::SequencedTaskRunner::GetCurrentDefault());
}

InputServiceLinuxImpl::~InputServiceLinuxImpl() {
  // Never destroyed.
  NOTREACHED_IN_MIGRATION();
}

void InputServiceLinuxImpl::BlockingTaskRunnerHelper::OnDeviceAdded(
    ScopedUdevDevicePtr device) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  if (!device)
    return;
  const char* devnode = udev_device_get_devnode(device.get());
  if (!devnode)
    return;

  auto info = mojom::InputDeviceInfo::New();
  info->id = devnode;

  const char* subsystem = udev_device_get_subsystem(device.get());
  if (!subsystem)
    return;
  if (strcmp(subsystem, kSubsystemHid) == 0) {
    info->subsystem = mojom::InputDeviceSubsystem::SUBSYSTEM_HID;
    info->name = GetParentDeviceName(device.get(), kSubsystemHid);
  } else if (strcmp(subsystem, kSubsystemInput) == 0) {
    info->subsystem = mojom::InputDeviceSubsystem::SUBSYSTEM_INPUT;
    info->name = GetParentDeviceName(device.get(), kSubsystemInput);
  } else {
    return;
  }

  info->type = GetDeviceType(device.get());

  info->is_accelerometer = GetBoolProperty(device.get(), kIdInputAccelerometer);
  info->is_joystick = GetBoolProperty(device.get(), kIdInputJoystick);
  info->is_key = GetBoolProperty(device.get(), kIdInputKey);
  info->is_keyboard = GetBoolProperty(device.get(), kIdInputKeyboard);
  info->is_mouse = GetBoolProperty(device.get(), kIdInputMouse);
  info->is_tablet = GetBoolProperty(device.get(), kIdInputTablet);
  info->is_touchpad = GetBoolProperty(device.get(), kIdInputTouchpad);
  info->is_touchscreen = GetBoolProperty(device.get(), kIdInputTouchscreen);

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&InputServiceLinux::AddDevice, service_, std::move(info)));
}

void InputServiceLinuxImpl::BlockingTaskRunnerHelper::OnDeviceRemoved(
    ScopedUdevDevicePtr device) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!device)
    return;

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  const char* devnode = udev_device_get_devnode(device.get());
  if (devnode) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&InputServiceLinux::RemoveDevice, service_,
                                  std::string(devnode)));
  }
}

void InputServiceLinuxImpl::BlockingTaskRunnerHelper::OnDeviceChanged(
    ScopedUdevDevicePtr device) {}

}  // namespace

InputServiceLinux::InputServiceLinux() = default;

InputServiceLinux::~InputServiceLinux() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
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
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
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

}  // namespace device
