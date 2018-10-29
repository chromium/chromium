// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "device/bluetooth/bluetooth_gatt_notify_session.h"
#include "device/bluetooth/bluetooth_remote_gatt_descriptor.h"

namespace device {

BluetoothRemoteGattCharacteristic::BluetoothRemoteGattCharacteristic()
    : weak_ptr_factory_(this) {}

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
    const ExecuteCallback& execute_callback,
    const base::Closure& cancel_callback)
    : execute_callback_(execute_callback), cancel_callback_(cancel_callback) {}

BluetoothRemoteGattCharacteristic::NotifySessionCommand::
    ~NotifySessionCommand() = default;

void BluetoothRemoteGattCharacteristic::NotifySessionCommand::Execute() {
  execute_callback_.Run(COMMAND_NONE, RESULT_SUCCESS,
                        BluetoothRemoteGattService::GATT_ERROR_UNKNOWN);
}

void BluetoothRemoteGattCharacteristic::NotifySessionCommand::Execute(
    Type previous_command_type,
    Result previous_command_result,
    BluetoothRemoteGattService::GattErrorCode previous_command_error_code) {
  execute_callback_.Run(previous_command_type, previous_command_result,
                        previous_command_error_code);
}

void BluetoothRemoteGattCharacteristic::NotifySessionCommand::Cancel() {
  cancel_callback_.Run();
}

void BluetoothRemoteGattCharacteristic::StartNotifySession(
    const NotifySessionCallback& callback,
    const ErrorCallback& error_callback) {
  StartNotifySessionInternal(base::nullopt, callback, error_callback);
}

#if defined(OS_CHROMEOS)
void BluetoothRemoteGattCharacteristic::StartNotifySession(
    NotificationType notification_type,
    const NotifySessionCallback& callback,
    const ErrorCallback& error_callback) {
  StartNotifySessionInternal(notification_type, callback, error_callback);
}
#endif

bool BluetoothRemoteGattCharacteristic::WriteWithoutResponse(
    base::span<const uint8_t> value) {
  NOTIMPLEMENTED();
  return false;
}

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
    const base::Optional<NotificationType>& notification_type,
    const NotifySessionCallback& callback,
    const ErrorCallback& error_callback) {
  NotifySessionCommand* command = new NotifySessionCommand(
      base::Bind(&BluetoothRemoteGattCharacteristic::ExecuteStartNotifySession,
                 GetWeakPtr(), notification_type, callback, error_callback),
      base::Bind(&BluetoothRemoteGattCharacteristic::CancelStartNotifySession,
                 GetWeakPtr(),
                 base::Bind(error_callback,
                            BluetoothRemoteGattService::GATT_ERROR_FAILED)));

  pending_notify_commands_.push(std::unique_ptr<NotifySessionCommand>(command));
  if (pending_notify_commands_.size() == 1) {
    command->Execute();
  }
}

void BluetoothRemoteGattCharacteristic::ExecuteStartNotifySession(
    const base::Optional<NotificationType>& notification_type,
    NotifySessionCallback callback,
    ErrorCallback error_callback,
    NotifySessionCommand::Type previous_command_type,
    NotifySessionCommand::Result previous_command_result,
    BluetoothRemoteGattService::GattErrorCode previous_command_error_code) {
  // If the command that was resolved immediately before this command was run,
  // this command should be resolved with the same result.
  if (previous_command_type == NotifySessionCommand::COMMAND_START) {
    if (previous_command_result == NotifySessionCommand::RESULT_SUCCESS) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(
              &BluetoothRemoteGattCharacteristic::OnStartNotifySessionSuccess,
              GetWeakPtr(), callback));
      return;
    } else {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(
              &BluetoothRemoteGattCharacteristic::OnStartNotifySessionError,
              GetWeakPtr(), error_callback, previous_command_error_code));
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
            GetWeakPtr(), error_callback,
            BluetoothRemoteGattService::GATT_ERROR_NOT_SUPPORTED));
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
            GetWeakPtr(), callback));
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
            GetWeakPtr(), error_callback,
            (ccc_descriptor.size() == 0)
                ? BluetoothRemoteGattService::GATT_ERROR_NOT_SUPPORTED
                : BluetoothRemoteGattService::GATT_ERROR_FAILED));
    return;
  }

  // Pass the Client Characteristic Configuration descriptor to
  // SubscribetoNotifications, which will write the correct value to it, and
  // do whatever else is needed to get the notifications flowing.
  SubscribeToNotifications(
      ccc_descriptor[0],
#if defined(OS_CHROMEOS)
      notification_type.value_or((GetProperties() & PROPERTY_NOTIFY)
                                     ? NotificationType::kNotification
                                     : NotificationType::kIndication),
#endif
      base::Bind(
          &BluetoothRemoteGattCharacteristic::OnStartNotifySessionSuccess,
          GetWeakPtr(), callback),
      base::Bind(&BluetoothRemoteGattCharacteristic::OnStartNotifySessionError,
                 GetWeakPtr(), error_callback));
}

void BluetoothRemoteGattCharacteristic::CancelStartNotifySession(
    base::Closure callback) {
  std::unique_ptr<NotifySessionCommand> command =
      std::move(pending_notify_commands_.front());
  pending_notify_commands_.pop();
  callback.Run();
}

void BluetoothRemoteGattCharacteristic::OnStartNotifySessionSuccess(
    NotifySessionCallback callback) {
  std::unique_ptr<NotifySessionCommand> command =
      std::move(pending_notify_commands_.front());

  std::unique_ptr<device::BluetoothGattNotifySession> notify_session(
      new BluetoothGattNotifySession(weak_ptr_factory_.GetWeakPtr()));
  notify_sessions_.insert(notify_session.get());
  callback.Run(std::move(notify_session));

  pending_notify_commands_.pop();
  if (!pending_notify_commands_.empty()) {
    pending_notify_commands_.front()->Execute(
        NotifySessionCommand::COMMAND_START,
        NotifySessionCommand::RESULT_SUCCESS,
        BluetoothRemoteGattService::GATT_ERROR_UNKNOWN);
  }
}

void BluetoothRemoteGattCharacteristic::OnStartNotifySessionError(
    ErrorCallback error_callback,
    BluetoothRemoteGattService::GattErrorCode error) {
  std::unique_ptr<NotifySessionCommand> command =
      std::move(pending_notify_commands_.front());

  error_callback.Run(error);

  pending_notify_commands_.pop();
  if (!pending_notify_commands_.empty()) {
    pending_notify_commands_.front()->Execute(
        NotifySessionCommand::COMMAND_START, NotifySessionCommand::RESULT_ERROR,
        error);
  }
}

void BluetoothRemoteGattCharacteristic::StopNotifySession(
    BluetoothGattNotifySession* session,
    const base::Closure& callback) {
  NotifySessionCommand* command = new NotifySessionCommand(
      base::Bind(&BluetoothRemoteGattCharacteristic::ExecuteStopNotifySession,
                 GetWeakPtr(), session, callback),
      base::Bind(&BluetoothRemoteGattCharacteristic::CancelStopNotifySession,
                 GetWeakPtr(), callback));

  pending_notify_commands_.push(std::unique_ptr<NotifySessionCommand>(command));
  if (pending_notify_commands_.size() == 1) {
    command->Execute();
  }
}

void BluetoothRemoteGattCharacteristic::ExecuteStopNotifySession(
    BluetoothGattNotifySession* session,
    base::Closure callback,
    NotifySessionCommand::Type previous_command_type,
    NotifySessionCommand::Result previous_command_result,
    BluetoothRemoteGattService::GattErrorCode previous_command_error_code) {
  auto session_iterator = notify_sessions_.find(session);

  // If the session does not even belong to this characteristic, we return an
  // error right away.
  if (session_iterator == notify_sessions_.end()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &BluetoothRemoteGattCharacteristic::OnStopNotifySessionError,
            GetWeakPtr(), session, callback,
            BluetoothRemoteGattService::GATT_ERROR_FAILED));
    return;
  }

  // If there are more active sessions, then we return right away.
  if (notify_sessions_.size() > 1) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &BluetoothRemoteGattCharacteristic::OnStopNotifySessionSuccess,
            GetWeakPtr(), session, callback));
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
            GetWeakPtr(), session, callback,
            BluetoothRemoteGattService::GATT_ERROR_FAILED));
    return;
  }

  UnsubscribeFromNotifications(
      ccc_descriptor[0],
      base::Bind(&BluetoothRemoteGattCharacteristic::OnStopNotifySessionSuccess,
                 GetWeakPtr(), session, callback),
      base::Bind(&BluetoothRemoteGattCharacteristic::OnStopNotifySessionError,
                 GetWeakPtr(), session, callback));
}

void BluetoothRemoteGattCharacteristic::CancelStopNotifySession(
    base::Closure callback) {
  std::unique_ptr<NotifySessionCommand> command =
      std::move(pending_notify_commands_.front());
  pending_notify_commands_.pop();
  callback.Run();
}

void BluetoothRemoteGattCharacteristic::OnStopNotifySessionSuccess(
    BluetoothGattNotifySession* session,
    base::Closure callback) {
  std::unique_ptr<NotifySessionCommand> command =
      std::move(pending_notify_commands_.front());

  notify_sessions_.erase(session);

  callback.Run();

  pending_notify_commands_.pop();
  if (!pending_notify_commands_.empty()) {
    pending_notify_commands_.front()->Execute(
        NotifySessionCommand::COMMAND_STOP,
        NotifySessionCommand::RESULT_SUCCESS,
        BluetoothRemoteGattService::GATT_ERROR_UNKNOWN);
  }
}

void BluetoothRemoteGattCharacteristic::OnStopNotifySessionError(
    BluetoothGattNotifySession* session,
    base::Closure callback,
    BluetoothRemoteGattService::GattErrorCode error) {
  std::unique_ptr<NotifySessionCommand> command =
      std::move(pending_notify_commands_.front());

  notify_sessions_.erase(session);

  callback.Run();

  pending_notify_commands_.pop();
  if (!pending_notify_commands_.empty()) {
    pending_notify_commands_.front()->Execute(
        NotifySessionCommand::COMMAND_STOP, NotifySessionCommand::RESULT_ERROR,
        error);
  }
}

bool BluetoothRemoteGattCharacteristic::IsNotificationTypeSupported(
    const base::Optional<NotificationType>& notification_type) {
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
