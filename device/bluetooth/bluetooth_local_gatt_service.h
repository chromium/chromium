// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_LOCAL_GATT_SERVICE_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_LOCAL_GATT_SERVICE_H_

#include <stdint.h>

#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_gatt_service.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace device {

class BluetoothLocalGattCharacteristic;
class BluetoothLocalGattDescriptor;
class BluetoothDevice;

// BluetoothLocalGattService represents a local GATT service.
//
// Instances of the BluetoothLocalGattService class are used to represent a
// locally hosted GATT attribute hierarchy when the local
// adapter is used in the "peripheral" role. Such instances are meant to be
// constructed directly and registered. Once registered, a GATT attribute
// hierarchy will be visible to remote devices in the "central" role.
// BT local GATT services will be owned by the adapter they are created with.
//
// Note: We use virtual inheritance on the gatt service since it will be
// inherited by platform specific versions of the gatt service classes also. The
// platform specific local gatt service classes will inherit both this class and
// their gatt service class, hence causing an inheritance diamond.
class DEVICE_BLUETOOTH_EXPORT BluetoothLocalGattService
    : public virtual BluetoothGattService {
 public:
  // The Delegate class is used to send certain events that need to be handled
  // when the device is in peripheral mode. The delegate handles read and write
  // requests that are issued from remote clients.
  class Delegate {
   public:
    // Callbacks used for communicating GATT request responses.
    using ValueCallback = base::OnceCallback<void(
        std::optional<BluetoothGattService::GattErrorCode> error_code,
        const std::vector<uint8_t>&)>;
    using ErrorCallback = base::OnceClosure;

    // Called when a remote device |device| requests to read the value of the
    // characteristic |characteristic| starting at offset |offset|. To respond
    // to the request with failure (e.g. if an invalid offset was given),
    // delegates must invoke |callback| with the appropriate error code. If
    // |callback| is not invoked, the request will time out resulting in an
    // error. Therefore, delegates MUST invoke |callback| regardless of success
    // or failure.
    //
    // To respond to the request with success and return the requested value,
    // the delegate must invoke |callback| with the value (and without an error
    // code). Doing so will automatically update the value property of
    // |characteristic|.
    virtual void OnCharacteristicReadRequest(
        const BluetoothDevice* device,
        const BluetoothLocalGattCharacteristic* characteristic,
        int offset,
        ValueCallback callback) = 0;

    // Called when a remote device |device| requests to write the value of the
    // characteristic |characteristic| starting at offset |offset|.
    // This method is only called if the characteristic was specified as
    // writable and any authentication and authorization challenges were
    // satisfied by the remote device.
    //
    // To respond to the request with success the delegate must invoke
    // |callback|. To respond to the request with failure delegates must invoke
    // |error_callback|. If neither callback parameter is invoked, the request
    // will time out and result in an error. Therefore, delegates MUST invoke
    // either |callback| or |error_callback|.
    virtual void OnCharacteristicWriteRequest(
        const BluetoothDevice* device,
        const BluetoothLocalGattCharacteristic* characteristic,
        const std::vector<uint8_t>& value,
        int offset,
        base::OnceClosure callback,
        ErrorCallback error_callback) = 0;

    // Called when a remote device |device| requests to prepare write the value
    // of the characteristic |characteristic| starting at offset |offset|.
    // This method is only called if the characteristic was specified as
    // reliable writable and any authentication and authorization challenges
    // were satisfied by the remote device.
    //
    // |has_subsequent_request| is true when the reliable write session is still
    // ongoing, false otherwise. When |has_subsequent_request| is false,
    // delegates MUST tear down the current reliable write session with |device|
    // and commit all the prepare writes in that session in order.
    //
    // To respond to the request with success the delegate must invoke
    // |callback|. To respond to the request with failure delegates must invoke
    // |error_callback|. If neither callback parameter is invoked, the request
    // will time out and result in an error. Therefore, delegates MUST invoke
    // either |callback| or |error_callback|.
    virtual void OnCharacteristicPrepareWriteRequest(
        const BluetoothDevice* device,
        const BluetoothLocalGattCharacteristic* characteristic,
        const std::vector<uint8_t>& value,
        int offset,
        bool has_subsequent_request,
        base::OnceClosure callback,
        ErrorCallback error_callback) = 0;

    // Called when a remote device |device| requests to read the value of the
    // descriptor |descriptor| starting at offset |offset|. To respond
    // to the request with failure (e.g. if an invalid offset was given),
    // delegates must invoke |callback| with the appropriate error code. If
    // |callback| is not invoked, the request will time out resulting in an
    // error. Therefore, delegates MUST invoke |callback| regardless of success
    // or failure.
    //
    // To respond to the request with success and return the requested value,
    // the delegate must invoke |callback| with the value (and without an error
    // code). Doing so will automatically update the value property of
    // |descriptor|.
    virtual void OnDescriptorReadRequest(
        const BluetoothDevice* device,
        const BluetoothLocalGattDescriptor* descriptor,
        int offset,
        ValueCallback callback) = 0;

    // Called when a remote device |devie| requests to write the value of the
    // descriptor |descriptor| starting at offset |offset|.
    // This method is only called if the descriptor was specified as
    // writable and any authentication and authorization challenges were
    // satisfied by the remote device.
    //
    // To respond to the request with success the delegate must invoke
    // |callback|. To respond to the request with failure delegates must invoke
    // |error_callback|. If neither callback parameter is invoked, the request
    // will time out and result in an error. Therefore, delegates MUST invoke
    // either |callback| or |error_callback|.
    virtual void OnDescriptorWriteRequest(
        const BluetoothDevice* device,
        const BluetoothLocalGattDescriptor* descriptor,
        const std::vector<uint8_t>& value,
        int offset,
        base::OnceClosure callback,
        ErrorCallback error_callback) = 0;

    // Called when a remote device |device| requests notifications to start for
    // |characteristic|. |notification_type| is either notify or indicate,
    // depending on the request from |device|. This is only called if the
    // characteristic has specified the notify or indicate property.
    virtual void OnNotificationsStart(
        const BluetoothDevice* device,
        device::BluetoothGattCharacteristic::NotificationType notification_type,
        const BluetoothLocalGattCharacteristic* characteristic) = 0;

    // Called when a remote device |device| requests notifications to stop for
    // |characteristic|. This is only called if the characteristic has
    // specified the notify or indicate property.
    virtual void OnNotificationsStop(
        const BluetoothDevice* device,
        const BluetoothLocalGattCharacteristic* characteristic) = 0;
  };

  BluetoothLocalGattService(const BluetoothLocalGattService&) = delete;
  BluetoothLocalGattService& operator=(const BluetoothLocalGattService&) =
      delete;
  ~BluetoothLocalGattService() override = default;

  // Registers this GATT service. Calling Register will make this service and
  // all of its associated attributes available on the local adapters GATT
  // database. Call Unregister to make this service no longer available.
  virtual void Register(base::OnceClosure callback,
                        ErrorCallback error_callback) = 0;

  // Unregisters this GATT service. This will remove the service from the list
  // of services exposed by the adapter this service was registered on.
  virtual void Unregister(base::OnceClosure callback,
                          ErrorCallback error_callback) = 0;

  // Returns if this service is currently registered.
  virtual bool IsRegistered() = 0;

  // Deletes this service, invaliding the weak pointer returned by create and
  // unregistering the service if it was registered.
  virtual void Delete() = 0;

  virtual BluetoothLocalGattCharacteristic* GetCharacteristic(
      const std::string& identifier) = 0;

  // Constructs a BluetoothLocalGattCharacteristic associated with a local GATT
  // service when the adapter is in the peripheral role.
  //
  // This method constructs a characteristic with UUID |uuid|,
  // properties |properties|, and permissions |permissions|. The service
  // instance will contain this characteristic.
  // TODO(rkc): Investigate how to handle |PROPERTY_EXTENDED_PROPERTIES|
  // correctly.
  virtual base::WeakPtr<BluetoothLocalGattCharacteristic> CreateCharacteristic(
      const BluetoothUUID& uuid,
      BluetoothGattCharacteristic::Properties properties,
      BluetoothGattCharacteristic::Permissions permissions) = 0;

 protected:
  BluetoothLocalGattService() = default;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_LOCAL_GATT_SERVICE_H_
