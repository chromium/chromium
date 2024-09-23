// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "device/bluetooth/bluetooth_gatt_notify_session.h"
#include "device/bluetooth/bluetooth_remote_gatt_descriptor.h"

namespace device {

BluetoothRemoteGattCharacteristic::CommandStatus::CommandStatus(
    CommandType type,
    std::optional<BluetoothRemoteGattService::GattErrorCode> error_code)
    : type(type), error_code(error_code) {}

BluetoothRemoteGattCharacteristic::CommandStatus::CommandStatus(
    CommandStatus&& other) = default;

BluetoothRemoteGattCharacteristic::BluetoothRemoteGattCharacteristic() {}

BluetoothRemoteGattCharacteristic::~BluetoothRemoteGattCharacteristic() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  while (!pending_notify_commands_.empty()) {
    pending_notify_commands_.front().Cancel();
  }
}

std::vector<BluetoothRemoteGattDescriptor*>
BluetoothRemoteGattCharacteristic::GetDescriptors() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<BluetoothRemoteGattDescriptor*> descriptors;
  descriptors.reserve(descriptors_.size());
  for (const auto& pair : descriptors_)
    descriptors.push_back(pair.second.get());
  return descriptors;
}

BluetoothRemoteGattDescriptor* BluetoothRemoteGattCharacteristic::GetDescriptor(
    const std::string& identifier) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto iter = descriptors_.find(identifier);
  return iter != descriptors_.end() ? iter->second.get() : nullptr;
}

std::vector<BluetoothRemoteGattDescriptor*>
BluetoothRemoteGattCharacteristic::GetDescriptorsByUUID(
    const BluetoothUUID& uuid) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<BluetoothRemoteGattDescriptor*> descriptors;
  for (const auto& pair : descriptors_) {
    if (pair.second->GetUUID() == uuid)
      descriptors.push_back(pair.second.get());
  }

  return descriptors;
}

base::WeakPtr<device::BluetoothRemoteGattCharacteristic>
BluetoothRemoteGattCharacteristic::GetWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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

BluetoothRemoteGattCharacteristic::NotifySessionCommand::NotifySessionCommand(
    NotifySessionCommand&& other) = default;

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
  // Cancel() may be called on a currently executing command. This is only valid
  // when the BluetoothRemoteGattCharacteristic instance is being deleted
  // because the completion callback for the async task posted by Execute() will
  // never be called due to the characteristic weak pointer being invalidated.
  // This guarantees that one and only one success/error callback is called for
  // a NotifySessionCommand.
  std::move(cancel_callback_).Run();
}

void BluetoothRemoteGattCharacteristic::StartNotifySession(
    NotifySessionCallback callback,
    ErrorCallback error_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  StartNotifySessionInternal(std::nullopt, std::move(callback),
                             std::move(error_callback));
}

#if BUILDFLAG(IS_CHROMEOS)
void BluetoothRemoteGattCharacteristic::StartNotifySession(
    NotificationType notification_type,
    NotifySessionCallback callback,
    ErrorCallback error_callback) {
  StartNotifySessionInternal(notification_type, std::move(callback),
                             std::move(error_callback));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

bool BluetoothRemoteGattCharacteristic::AddDescriptor(
    std::unique_ptr<BluetoothRemoteGattDescriptor> descriptor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!descriptor)
    return false;

  auto* descriptor_raw = descriptor.get();
  return descriptors_
      .try_emplace(descriptor_raw->GetIdentifier(), std::move(descriptor))
      .second;
}

void BluetoothRemoteGattCharacteristic::StartNotifySessionInternal(
    const std::optional<NotificationType>& notification_type,
    NotifySessionCallback callback,
    ErrorCallback error_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto split_error_callback =
      base::SplitOnceCallback(std::move(error_callback));
  pending_notify_commands_.emplace(
      base::BindOnce(
          &BluetoothRemoteGattCharacteristic::ExecuteStartNotifySession,
          GetWeakPtr(), notification_type, std::move(callback),
          std::move(split_error_callback.first)),
      base::BindOnce(
          &BluetoothRemoteGattCharacteristic::CancelStartNotifySession,
          GetWeakPtr(),
          base::BindOnce(std::move(split_error_callback.second),
                         BluetoothGattService::GattErrorCode::kFailed)));

  if (!notify_command_running_ && pending_notify_commands_.size() == 1) {
    notify_command_running_ = true;
    pending_notify_commands_.front().Execute();
  }
}

void BluetoothRemoteGattCharacteristic::ExecuteStartNotifySession(
    const std::optional<NotificationType>& notification_type,
    NotifySessionCallback callback,
    ErrorCallback error_callback,
    CommandStatus previous_command) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If the command that was resolved immediately before this command was run,
  // this command should be resolved with the same result.
  if (previous_command.type == CommandType::kStart) {
    if (!previous_command.error_code) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(
              &BluetoothRemoteGattCharacteristic::OnStartNotifySessionSuccess,
              GetWeakPtr(), std::move(callback)));
      return;
    } else {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
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
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &BluetoothRemoteGattCharacteristic::OnStartNotifySessionError,
            GetWeakPtr(), std::move(error_callback),
            BluetoothGattService::GattErrorCode::kNotSupported));
    return;
  }

  // If the characteristic is already notifying, then we don't need to
  // subscribe again. All we need to do is call the success callback, which
  // will create and return a session object to the caller.
  if (IsNotifying()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
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
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &BluetoothRemoteGattCharacteristic::OnStartNotifySessionError,
            GetWeakPtr(), std::move(error_callback),
            (ccc_descriptor.size() == 0)
                ? BluetoothGattService::GattErrorCode::kNotSupported
                : BluetoothGattService::GattErrorCode::kFailed));
    return;
  }

  // Pass the Client Characteristic Configuration descriptor to
  // SubscribetoNotifications, which will write the correct value to it, and
  // do whatever else is needed to get the notifications flowing.
  SubscribeToNotifications(
      ccc_descriptor[0],
#if BUILDFLAG(IS_CHROMEOS)
      notification_type.value_or((GetProperties() & PROPERTY_NOTIFY)
                                     ? NotificationType::kNotification
                                     : NotificationType::kIndication),
#endif  // BUILDFLAG(IS_CHROMEOS)
      base::BindOnce(
          &BluetoothRemoteGattCharacteristic::OnStartNotifySessionSuccess,
          GetWeakPtr(), std::move(callback)),
      base::BindOnce(
          &BluetoothRemoteGattCharacteristic::OnStartNotifySessionError,
          GetWeakPtr(), std::move(error_callback)));
}

void BluetoothRemoteGattCharacteristic::CancelStartNotifySession(
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(notify_command_running_);
  pending_notify_commands_.pop();
  std::move(callback).Run();
  notify_command_running_ = false;
}

void BluetoothRemoteGattCharacteristic::OnStartNotifySessionSuccess(
    NotifySessionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(notify_command_running_);
  pending_notify_commands_.pop();

  auto notify_session = std::make_unique<device::BluetoothGattNotifySession>(
      weak_ptr_factory_.GetWeakPtr());
  notify_sessions_.insert(notify_session->unique_id());

  auto this_ptr = GetWeakPtr();
  std::move(callback).Run(std::move(notify_session));
  if (!this_ptr)  // If this object was deleted by callback.
    return;

  notify_command_running_ = false;
  if (!pending_notify_commands_.empty()) {
    notify_command_running_ = true;
    pending_notify_commands_.front().Execute(
        CommandStatus(CommandType::kStart));
  }
}

void BluetoothRemoteGattCharacteristic::OnStartNotifySessionError(
    ErrorCallback error_callback,
    BluetoothGattService::GattErrorCode error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(notify_command_running_);
  pending_notify_commands_.pop();

  auto this_ptr = GetWeakPtr();
  std::move(error_callback).Run(error);
  if (!this_ptr)  // If this object was deleted by callback.
    return;

  notify_command_running_ = false;
  if (!pending_notify_commands_.empty()) {
    notify_command_running_ = true;
    pending_notify_commands_.front().Execute(
        CommandStatus(CommandType::kStart, error));
  }
}

void BluetoothRemoteGattCharacteristic::StopNotifySession(
    BluetoothGattNotifySession::Id session,
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  pending_notify_commands_.emplace(
      base::BindOnce(
          &BluetoothRemoteGattCharacteristic::ExecuteStopNotifySession,
          GetWeakPtr(), session, std::move(split_callback.first)),
      base::BindOnce(
          &BluetoothRemoteGattCharacteristic::CancelStopNotifySession,
          GetWeakPtr(), std::move(split_callback.second)));
  if (!notify_command_running_ && pending_notify_commands_.size() == 1) {
    notify_command_running_ = true;
    pending_notify_commands_.front().Execute();
  }
}

void BluetoothRemoteGattCharacteristic::ExecuteStopNotifySession(
    BluetoothGattNotifySession::Id session,
    base::OnceClosure callback,
    CommandStatus previous_command) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto session_iterator = notify_sessions_.find(session);

  // If the session does not even belong to this characteristic, we return an
  // error right away.
  if (session_iterator == notify_sessions_.end()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &BluetoothRemoteGattCharacteristic::OnStopNotifySessionError,
            GetWeakPtr(), session, std::move(callback),
            BluetoothGattService::GattErrorCode::kFailed));
    return;
  }

  // If there are more active sessions, then we return right away.
  if (notify_sessions_.size() > 1) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
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
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &BluetoothRemoteGattCharacteristic::OnStopNotifySessionError,
            GetWeakPtr(), session, std::move(callback),
            BluetoothGattService::GattErrorCode::kFailed));
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pending_notify_commands_.pop();
  std::move(callback).Run();
}

void BluetoothRemoteGattCharacteristic::OnStopNotifySessionSuccess(
    BluetoothGattNotifySession::Id session,
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(notify_command_running_);
  pending_notify_commands_.pop();

  notify_sessions_.erase(session);

  auto this_ptr = GetWeakPtr();
  std::move(callback).Run();
  if (!this_ptr)  // If this object was deleted by callback.
    return;

  notify_command_running_ = false;
  if (!pending_notify_commands_.empty()) {
    notify_command_running_ = true;
    pending_notify_commands_.front().Execute(CommandStatus(CommandType::kStop));
  }
}

void BluetoothRemoteGattCharacteristic::OnStopNotifySessionError(
    BluetoothGattNotifySession::Id session,
    base::OnceClosure callback,
    BluetoothGattService::GattErrorCode error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(notify_command_running_);
  pending_notify_commands_.pop();

  notify_sessions_.erase(session);

  auto this_ptr = GetWeakPtr();
  std::move(callback).Run();
  if (!this_ptr)  // If this object was deleted by callback.
    return;
  notify_command_running_ = false;

  if (!pending_notify_commands_.empty()) {
    notify_command_running_ = true;
    pending_notify_commands_.front().Execute(
        CommandStatus(CommandType::kStop, error));
  }
}

bool BluetoothRemoteGattCharacteristic::IsNotificationTypeSupported(
    const std::optional<NotificationType>& notification_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
    case NotificationType::kNone:
      LOG(WARNING) << __func__ << ": Unexpected NotificationType "
                   << static_cast<uint16_t>(NotificationType::kNone);
      return false;
  }
}

}  // namespace device
