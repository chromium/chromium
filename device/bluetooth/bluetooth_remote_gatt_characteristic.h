// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_CHARACTERISTIC_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_CHARACTERISTIC_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/containers/queue.h"
#include "base/containers/span.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_remote_gatt_service.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace device {

class BluetoothGattNotifySession;
class BluetoothRemoteGattDescriptor;

// BluetoothRemoteGattCharacteristic represents a remote GATT characteristic.
// This class is used to represent GATT characteristics that belong to a service
// hosted by a remote device. In this case the characteristic will be
// constructed by the subsystem.
//
// Note: We use virtual inheritance on the GATT characteristic since it will be
// inherited by platform specific versions of the GATT characteristic classes
// also. The platform specific remote GATT characteristic classes will inherit
// both this class and their GATT characteristic class, hence causing an
// inheritance diamond.
class DEVICE_BLUETOOTH_EXPORT BluetoothRemoteGattCharacteristic
    : public virtual BluetoothGattCharacteristic {
 public:
  // The ValueCallback is used to return the value of a remote characteristic
  // upon a read request.
  using ValueCallback = base::OnceCallback<void(const std::vector<uint8_t>&)>;

  // The NotifySessionCallback is used to return sessions after they have
  // been successfully started.
  using NotifySessionCallback =
      base::OnceCallback<void(std::unique_ptr<BluetoothGattNotifySession>)>;

  ~BluetoothRemoteGattCharacteristic() override;

  // Returns the value of the characteristic. For remote characteristics, this
  // is the most recently cached value. For local characteristics, this is the
  // most recently updated value or the value retrieved from the delegate.
  virtual const std::vector<uint8_t>& GetValue() const = 0;

  // Returns a pointer to the GATT service this characteristic belongs to.
  virtual BluetoothRemoteGattService* GetService() const = 0;

  // Returns the list of GATT characteristic descriptors that provide more
  // information about this characteristic.
  virtual std::vector<BluetoothRemoteGattDescriptor*> GetDescriptors() const;

  // Returns the GATT characteristic descriptor with identifier |identifier| if
  // it belongs to this GATT characteristic.
  virtual BluetoothRemoteGattDescriptor* GetDescriptor(
      const std::string& identifier) const;

  // Returns the GATT characteristic descriptors that match |uuid|. There may be
  // multiple, as illustrated by Core Bluetooth Specification [V4.2 Vol 3 Part G
  // 3.3.3.5 Characteristic Presentation Format].
  virtual std::vector<BluetoothRemoteGattDescriptor*> GetDescriptorsByUUID(
      const BluetoothUUID& uuid) const;

  // Get a weak pointer to the characteristic.
  base::WeakPtr<BluetoothRemoteGattCharacteristic> GetWeakPtr();

  // Returns whether or not this characteristic is currently sending value
  // updates in the form of a notification or indication.
  //
  // If your code wants to receive notifications, you MUST call
  // StartNotifySession and hold on to the resulting session object for as long
  // as you want to keep receiving notifications. Even if this method returns
  // true, and you are able to see the notifications coming in, you have no
  // guarantee that the notifications will keep flowing for as long as you
  // need, unless you open your own session.
  virtual bool IsNotifying() const;

  // Starts a notify session for the remote characteristic, if it supports
  // notifications/indications. On success, the characteristic starts sending
  // value notifications and |callback| is called with a session object whose
  // ownership belongs to the caller. |error_callback| is called on errors.
  //
  // This method handles all logic regarding multiple sessions so that
  // specific platform implementations of the remote characteristic class
  // do not have to. Rather than overriding this method, it is recommended
  // to override the SubscribeToNotifications method below.
  //
  // The code in SubscribeToNotifications writes to the Client Characteristic
  // Configuration descriptor to enable notifications/indications. Core
  // Bluetooth Specification [V4.2 Vol 3 Part G Section 3.3.1.1. Characteristic
  // Properties] requires this descriptor to be present when
  // notifications/indications are supported. If the descriptor is not present
  // |error_callback| will be run.
  //
  // Writing a non-zero value to the remote characteristic's Client
  // Characteristic Configuration descriptor, causes the remote characteristic
  // to start sending us notifications whenever the characteristic's value
  // changes. When a new notification is received,
  // BluetoothAdapterObserver::GattCharacteristicValueChanged is called with
  // the characteristic's new value.
  //
  // To stop the flow of notifications, simply call the Stop method on the
  // BluetoothGattNotifySession object that you received in |callback|.
  virtual void StartNotifySession(NotifySessionCallback callback,
                                  ErrorCallback error_callback);
#if defined(OS_CHROMEOS)
  // TODO(https://crbug.com/849359): This method should also be implemented on
  // Android and Windows.
  // macOS does not support specifying a notification type. According to macOS
  // documentation if the characteristic supports both notify and indicate, only
  // notifications will be enabled.
  // https://developer.apple.com/documentation/corebluetooth/cbperipheral/1518949-setnotifyvalue?language=objc#discussion
  virtual void StartNotifySession(NotificationType notification_type,
                                  NotifySessionCallback callback,
                                  ErrorCallback error_callback);
#endif

  // Sends a read request to a remote characteristic to read its value.
  // |callback| is called to return the read value on success and
  // |error_callback| is called for failures.
  virtual void ReadRemoteCharacteristic(ValueCallback callback,
                                        ErrorCallback error_callback) = 0;

  // Sends a write request to a remote characteristic with the value |value|.
  // |callback| is called to signal success and |error_callback| for failures.
  // This method only applies to remote characteristics and will fail for those
  // that are locally hosted.
  virtual void WriteRemoteCharacteristic(const std::vector<uint8_t>& value,
                                         base::OnceClosure callback,
                                         ErrorCallback error_callback) = 0;

#if defined(OS_CHROMEOS)
  // Sends a prepare write request to a remote characteristic with the value
  // |value|. |callback| is called to signal success and |error_callback| for
  // failures. This method only applies to remote characteristics and will fail
  // for those that are locally hosted.
  // Callers should use BluetoothDevice::ExecuteWrite() to commit or
  // BluetoothDevice::AbortWrite() to abort the change.
  virtual void PrepareWriteRemoteCharacteristic(
      const std::vector<uint8_t>& value,
      base::OnceClosure callback,
      ErrorCallback error_callback) = 0;
#endif

  // Sends a write request to a remote characteristic with the value |value|
  // without waiting for a response. This method returns false to signal
  // failures. When attempting to write the remote characteristic true is
  // returned without a guarantee of success. This method only applies to remote
  // characteristics and will fail for those that are locally hosted.
  // This method is currently implemented only on macOS.
  // TODO(https://crbug.com/831524): Implement it on other platforms as well.
  virtual bool WriteWithoutResponse(base::span<const uint8_t> value);

 protected:
  using DescriptorMap =
      base::flat_map<std::string,
                     std::unique_ptr<BluetoothRemoteGattDescriptor>>;

  BluetoothRemoteGattCharacteristic();

  // Writes to the Client Characteristic Configuration descriptor to enable
  // notifications/indications. This method is meant to be called from
  // StartNotifySession and should contain only the code necessary to start
  // listening to characteristic notifications on a particular platform.
#if defined(OS_CHROMEOS)
  // |notification_type| specifies the type of notifications that will be
  // enabled: notifications or indications.
  // TODO(https://crbug.com/849359): This method should also be implemented on
  // Android and Windows.
  virtual void SubscribeToNotifications(
      BluetoothRemoteGattDescriptor* ccc_descriptor,
      NotificationType notification_type,
      base::OnceClosure callback,
      ErrorCallback error_callback) = 0;
#else
  virtual void SubscribeToNotifications(
      BluetoothRemoteGattDescriptor* ccc_descriptor,
      base::OnceClosure callback,
      ErrorCallback error_callback) = 0;
#endif

  // Writes to the Client Characteristic Configuration descriptor to disable
  // notifications/indications. This method is meant to be called from
  // StopNotifySession and should contain only the code necessary to stop
  // listening to characteristic notifications on a particular platform.
  virtual void UnsubscribeFromNotifications(
      BluetoothRemoteGattDescriptor* ccc_descriptor,
      base::OnceClosure callback,
      ErrorCallback error_callback) = 0;

  // Utility function to add a |descriptor| to the map of |descriptors_|.
  bool AddDescriptor(std::unique_ptr<BluetoothRemoteGattDescriptor> descriptor);

  // Descriptors owned by the chracteristic. The descriptors' identifiers serve
  // as keys.
  DescriptorMap descriptors_;

 private:
  friend class BluetoothGattNotifySession;

  // Stops an active notify session for the remote characteristic. On success,
  // the characteristic removes this session from the list of active sessions.
  // If there are no more active sessions, notifications/indications are
  // turned off.
  //
  // This method is, and should only be, called from
  // BluetoothGattNotifySession::Stop().
  //
  // The code in UnsubscribeFromNotifications writes to the Client
  // Characteristic Configuration descriptor to disable
  // notifications/indications. Core Bluetooth Specification [V4.2 Vol 3 Part G
  // Section 3.3.1.1. Characteristic Properties] requires this descriptor to be
  // present when notifications/indications are supported.
  virtual void StopNotifySession(BluetoothGattNotifySession* session,
                                 base::OnceClosure callback);

  class NotifySessionCommand {
   public:
    enum Type { COMMAND_NONE, COMMAND_START, COMMAND_STOP };
    enum Result { RESULT_SUCCESS, RESULT_ERROR };

    using ExecuteCallback = base::OnceCallback<
        void(Type, Result, BluetoothRemoteGattService::GattErrorCode)>;

    ExecuteCallback execute_callback_;
    base::OnceClosure cancel_callback_;

    NotifySessionCommand(ExecuteCallback execute_callback,
                         base::OnceClosure cancel_callback);
    ~NotifySessionCommand();

    void Execute();
    void Execute(
        Type previous_command_type,
        Result previous_command_result,
        BluetoothRemoteGattService::GattErrorCode previous_command_error_code);
    void Cancel();
  };

  void StartNotifySessionInternal(
      const base::Optional<NotificationType>& notification_type,
      NotifySessionCallback callback,
      ErrorCallback error_callback);
  void ExecuteStartNotifySession(
      const base::Optional<NotificationType>& notification_type,
      NotifySessionCallback callback,
      ErrorCallback error_callback,
      NotifySessionCommand::Type previous_command_type,
      NotifySessionCommand::Result previous_command_result,
      BluetoothRemoteGattService::GattErrorCode previous_command_error_code);
  void CancelStartNotifySession(base::OnceClosure callback);
  void OnStartNotifySessionSuccess(NotifySessionCallback callback);
  void OnStartNotifySessionError(
      ErrorCallback error_callback,
      BluetoothRemoteGattService::GattErrorCode error);

  void ExecuteStopNotifySession(
      BluetoothGattNotifySession* session,
      base::OnceClosure callback,
      NotifySessionCommand::Type previous_command_type,
      NotifySessionCommand::Result previous_command_result,
      BluetoothRemoteGattService::GattErrorCode previous_command_error_code);
  void CancelStopNotifySession(base::OnceClosure callback);
  void OnStopNotifySessionSuccess(BluetoothGattNotifySession* session,
                                  base::OnceClosure callback);
  void OnStopNotifySessionError(
      BluetoothGattNotifySession* session,
      base::OnceClosure callback,
      BluetoothRemoteGattService::GattErrorCode error);
  bool IsNotificationTypeSupported(
      const base::Optional<NotificationType>& notification_type);

  // Pending StartNotifySession / StopNotifySession calls.
  base::queue<std::unique_ptr<NotifySessionCommand>> pending_notify_commands_;

  // Set of active notify sessions.
  std::set<BluetoothGattNotifySession*> notify_sessions_;

  base::WeakPtrFactory<BluetoothRemoteGattCharacteristic> weak_ptr_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(BluetoothRemoteGattCharacteristic);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_CHARACTERISTIC_H_
