// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/chromeos_buildflags.h"
#include "device/bluetooth/bluetooth_gatt_notify_session.h"
#include "device/bluetooth/bluetooth_remote_gatt_descriptor.h"

namespace device {

BluetoothRemoteGattCharacteristic::CommandStatus::CommandStatus(
    CommandType type,
    absl::optional<BluetoothRemoteGattService::GattErrorCode> error_code)
    : type(type), error_code(error_code) {}

BluetoothRemoteGattCharacteristic::CommandStatus::CommandStatus(
    CommandStatus&& other) = default;

BluetoothRemoteGattCharacteristic::BluetoothRemoteGattCharacteristic() {}

BluetoothRemoteGattCharacteristic::~BluetoothRemoteGattCharacteristic() {
  while (!pending_notify_commands_.empty()) {
    pending_notify_commands_.front()->Cancel();
  }
}

std::vector<BluetoothRemoteGattDescriptor*>
BluetoothRemoteGattCharacteristic::GetDescriptors() const {
  std::vector<BluetoothRemoteGattDescriptor*> descriptors;
  descriptors.reserve(descriptors_.size());
  for (const auto& pair : descriptors_)
    descriptors.push_back(pair.second.get());
  return descriptors;
}

BluetoothRemoteGattDescriptor* BluetoothRemoteGattCharacteristic::GetDescriptor(
    const std::string& identifier) const {
  auto iter = descriptors_.find(identifier);
  return iter != descriptors_.end() ? iter->second.get() : nullptr;
}

std::vector<BluetoothRemoteGattDescriptor*>
BluetoothRemoteGattCharacteristic::GetDescriptorsByUUID(
    const BluetoothUUID& uuid) const {
  std::vector<BluetoothRemoteGattDescriptor*> descriptors;
  for (const auto& pair : descriptors_) {
    if (pair.second->GetUUID() == uuid)
      descriptors.push_back(pair.second.get());
  }

  return descriptors;
}

base::WeakPtr<device::BluetoothRemoteGattCharacteristic>
BluetoothRemoteGattCharacteristic::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool BluetoothRemoteGattCharacteristic::IsNotifying() const {
  return !notify_sessions_.empty();
}

BluetoothRemoteGattCharacteristic::NotifySessionCommand::NotifySessionCommand(
    ExecuteCallback execute_callback,
    base::OnceClosure cancel_callback)
    : execute_callback_(std::move(execute_callback)),
      cancel_callback_(std::move(cancel_callback)) {}

BluetoothRemoteGattCharacteristic::NotifySessionCommand::
    ~NotifySessionCommand() = default;

void BluetoothRemoteGattCharacteristic::NotifySessionCommand::Execute() {
  std::move(execute_callback_).Run(CommandStatus());
}

void BluetoothRemoteGattCharacteristic::NotifySessionCommand::Execute(
    CommandStatus previous_command) {
  std::move(execute_callback_).Run(std::move(previous_command));
}

void BluetoothRemoteGattCharacteristic::NotifySessionCommand::Cancel() {
  std::move(cancel_callback_).Run();
}

void BluetoothRemoteGattCharacteristic::StartNotifySession(
    NotifySessionCallback callback,
    ErrorCallback error_callback) {
  StartNotifySessionInternal(absl::nullopt, std::move(callback),
                             std::move(error_callback));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void BluetoothRemoteGattCharacteristic::StartNotifySession(
    NotificationType notification_type,
    NotifySessionCallback callback,
    ErrorCallback error_callback) {
  StartNotifySessionInternal(notification_type, std::move(callback),
                             std::move(error_callback));
}
#endif

bool BluetoothRemoteGattCharacteristic::AddDescriptor(
    std::unique_ptr<BluetoothRemoteGattDescriptor> descriptor) {
  if (!descriptor)
    return false;

  auto* descriptor_raw = descriptor.get();
  return descriptors_
      .try_emplace(descriptor_raw->GetIdentifier(), std::move(descriptor))
      .second;
}

void BluetoothRemoteGattCharacteristic::StartNotifySessionInternal(
    const absl::optional<NotificationType>& notification_type,
    NotifySessionCallback callback,
    ErrorCallback error_callback) {
  auto split_error_callback =
      base::SplitOnceCallback(std::move(error_callback));
  NotifySessionCommand* command = new NotifySessionCommand(
      base::BindOnce(
          &BluetoothRemoteGattCharacteristic::ExecuteStartNotifySession,
          GetWeakPtr(), notification_type, std::move(callback),
          std::move(split_error_callback.first)),
      base::BindOnce(
          &BluetoothRemoteGattCharacteristic::CancelStartNotifySession,
          GetWeakPtr(),
          base::BindOnce(std::move(split_error_callback.second),
                         BluetoothGattService::GATT_ERROR_FAILED)));

  pending_notify_commands_.push(base::WrapUnique(command));
  if (pending_notify_commands_.size() == 1) {
    command->Execute();
  }
}

void BluetoothRemoteGattCharacteristic::ExecuteStartNotifySession(
    const absl::optional<NotificationType>& notification_type,
    NotifySessionCallback callback,
    ErrorCallback error_callback,
    CommandStatus previous_command) {
  // If the command that was resolved immediately before this command was run,
  // this command should be resolved with the same result.
  if (previous_command.type == CommandType::kStart) {
    if (!previous_command.error_code) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(
              &BluetoothRemoteGattCharacteristic::OnStartNotifySessionSuccess,
              GetWeakPtr(), std::move(callback)));
      return;
    } else {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(
              &BluetoothRemoteGattCharacteristic::OnStartNotifySessionError,
              GetWeakPtr(), std::move(error_callback),
              previous_command.error_code.value()));
      return;
    }
  }

  if (!IsNotificationTypeSupported(notification_type)) {
    if (notification_type)
      LOG(ERROR) << "Characteristic doesn't support specified "
                 << "notification_type";
    else
      LOG(ERROR) << "Characteristic needs NOTIFY or INDICATE";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &BluetoothRemoteGattCharacteristic::OnStartNotifySessionError,
            GetWeakPtr(), std::move(error_callback),
            BluetoothGattService::GATT_ERROR_NOT_SUPPORTED));
    return;
  }

  // If the characteristic is already notifying, then we don't need to
  // subscribe again. All we need to do is call the success callback, which
  // will create and return a session object to the caller.
  if (IsNotifying()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &BluetoothRemoteGattCharacteristic::OnStartNotifySessionSuccess,
            GetWeakPtr(), std::move(callback)));
    return;
  }

  // Find the Client Characteristic Configuration descriptor.
  std::vector<BluetoothRemoteGattDescriptor*> ccc_descriptor =
      GetDescriptorsByUUID(BluetoothRemoteGattDescriptor::
                               ClientCharacteristicConfigurationUuid());

  if (ccc_descriptor.size() != 1u) {
    LOG(ERROR) << "Found " << ccc_descriptor.size()
               << " client characteristic configuration descriptors.";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &BluetoothRemoteGattCharacteristic::OnStartNotifySessionError,
            GetWeakPtr(), std::move(error_callback),
            (ccc_descriptor.size() == 0)
                ? BluetoothGattService::GATT_ERROR_NOT_SUPPORTED
                : BluetoothGattService::GATT_ERROR_FAILED));
    return;
  }

  // Pass the Client Characteristic Configuration descriptor to
  // SubscribetoNotifications, which will write the correct value to it, and
  // do whatever else is needed to get the notifications flowing.
  SubscribeToNotifications(
      ccc_descriptor[0],
#if BUILDFLAG(IS_CHROMEOS_ASH)
      notification_type.value_or((GetProperties() & PROPERTY_NOTIFY)
                                     ? NotificationType::kNotification
                                     : NotificationType::kIndication),
#endif
      base::BindOnce(
          &BluetoothRemoteGattCharacteristic::OnStartNotifySessionSuccess,
          GetWeakPtr(), std::move(callback)),
      base::BindOnce(
          &BluetoothRemoteGattCharacteristic::OnStartNotifySessionError,
          GetWeakPtr(), std::move(error_callback)));
}

void BluetoothRemoteGattCharacteristic::CancelStartNotifySession(
    base::OnceClosure callback) {
  std::unique_ptr<NotifySessionCommand> command =
      std::move(pending_notify_commands_.front());
  pending_notify_commands_.pop();
  std::move(callback).Run();
}

void BluetoothRemoteGattCharacteristic::OnStartNotifySessionSuccess(
    NotifySessionCallback callback) {
  std::unique_ptr<NotifySessionCommand> command =
      std::move(pending_notify_commands_.front());

  std::unique_ptr<device::BluetoothGattNotifySession> notify_session(
      new BluetoothGattNotifySession(weak_ptr_factory_.GetWeakPtr()));
  notify_sessions_.insert(notify_session.get());
  std::move(callback).Run(std::move(notify_session));

  pending_notify_commands_.pop();
  if (!pending_notify_commands_.empty()) {
    pending_notify_commands_.front()->Execute(
        CommandStatus(CommandType::kStart));
  }
}

void BluetoothRemoteGattCharacteristic::OnStartNotifySessionError(
    ErrorCallback error_callback,
    BluetoothGattService::GattErrorCode error) {
  std::unique_ptr<NotifySessionCommand> command =
      std::move(pending_notify_commands_.front());

  std::move(error_callback).Run(error);

  pending_notify_commands_.pop();
  if (!pending_notify_commands_.empty()) {
    pending_notify_commands_.front()->Execute(
        CommandStatus(CommandType::kStart, error));
  }
}

void BluetoothRemoteGattCharacteristic::StopNotifySession(
    BluetoothGattNotifySession* session,
    base::OnceClosure callback) {
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  NotifySessionCommand* command = new NotifySessionCommand(
      base::BindOnce(
          &BluetoothRemoteGattCharacteristic::ExecuteStopNotifySession,
          GetWeakPtr(), session, std::move(split_callback.first)),
      base::BindOnce(
          &BluetoothRemoteGattCharacteristic::CancelStopNotifySession,
          GetWeakPtr(), std::move(split_callback.second)));

  pending_notify_commands_.push(std::unique_ptr<NotifySessionCommand>(command));
  if (pending_notify_commands_.size() == 1) {
    command->Execute();
  }
}

void BluetoothRemoteGattCharacteristic::ExecuteStopNotifySession(
    BluetoothGattNotifySession* session,
    base::OnceClosure callback,
    CommandStatus previous_command) {
  auto session_iterator = notify_sessions_.find(session);

  // If the session does not even belong to this characteristic, we return an
  // error right away.
  if (session_iterator == notify_sessions_.end()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &BluetoothRemoteGattCharacteristic::OnStopNotifySessionError,
            GetWeakPtr(), session, std::move(callback),
            BluetoothGattService::GATT_ERROR_FAILED));
    return;
  }

  // If there are more active sessions, then we return right away.
  if (notify_sessions_.size() > 1) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &BluetoothRemoteGattCharacteristic::OnStopNotifySessionSuccess,
            GetWeakPtr(), session, std::move(callback)));
    return;
  }

  // Find the Client Characteristic Configuration descriptor.
  std::vector<BluetoothRemoteGattDescriptor*> ccc_descriptor =
      GetDescriptorsByUUID(BluetoothRemoteGattDescriptor::
                               ClientCharacteristicConfigurationUuid());

  if (ccc_descriptor.size() != 1u) {
    LOG(ERROR) << "Found " << ccc_descriptor.size()
               << " client characteristic configuration descriptors.";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &BluetoothRemoteGattCharacteristic::OnStopNotifySessionError,
            GetWeakPtr(), session, std::move(callback),
            BluetoothGattService::GATT_ERROR_FAILED));
    return;
  }

  auto split_callback = base::SplitOnceCallback(std::move(callback));
  UnsubscribeFromNotifications(
      ccc_descriptor[0],
      base::BindOnce(
          &BluetoothRemoteGattCharacteristic::OnStopNotifySessionSuccess,
          GetWeakPtr(), session, std::move(split_callback.first)),
      base::BindOnce(
          &BluetoothRemoteGattCharacteristic::OnStopNotifySessionError,
          GetWeakPtr(), session, std::move(split_callback.second)));
}

void BluetoothRemoteGattCharacteristic::CancelStopNotifySession(
    base::OnceClosure callback) {
  std::unique_ptr<NotifySessionCommand> command =
      std::move(pending_notify_commands_.front());
  pending_notify_commands_.pop();
  std::move(callback).Run();
}

void BluetoothRemoteGattCharacteristic::OnStopNotifySessionSuccess(
    BluetoothGattNotifySession* session,
    base::OnceClosure callback) {
  std::unique_ptr<NotifySessionCommand> command =
      std::move(pending_notify_commands_.front());

  notify_sessions_.erase(session);

  std::move(callback).Run();

  pending_notify_commands_.pop();
  if (!pending_notify_commands_.empty()) {
    pending_notify_commands_.front()->Execute(
        CommandStatus(CommandType::kStop));
  }
}

void BluetoothRemoteGattCharacteristic::OnStopNotifySessionError(
    BluetoothGattNotifySession* session,
    base::OnceClosure callback,
    BluetoothGattService::GattErrorCode error) {
  std::unique_ptr<NotifySessionCommand> command =
      std::move(pending_notify_commands_.front());

  notify_sessions_.erase(session);

  std::move(callback).Run();

  pending_notify_commands_.pop();
  if (!pending_notify_commands_.empty()) {
    pending_notify_commands_.front()->Execute(
        CommandStatus(CommandType::kStop, error));
  }
}

bool BluetoothRemoteGattCharacteristic::IsNotificationTypeSupported(
    const absl::optional<NotificationType>& notification_type) {
  Properties properties = GetProperties();
  bool hasNotify = (properties & PROPERTY_NOTIFY) != 0;
  bool hasIndicate = (properties & PROPERTY_INDICATE) != 0;
  if (!notification_type)
    return hasNotify || hasIndicate;
  switch (notification_type.value()) {
    case NotificationType::kNotification:
      return hasNotify;
    case NotificationType::kIndication:
      return hasIndicate;
  }
}

}  // namespace device
