// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DBUS_BLUETOOTH_AGENT_SERVICE_PROVIDER_H_
#define DEVICE_BLUETOOTH_DBUS_BLUETOOTH_AGENT_SERVICE_PROVIDER_H_

#include <cstdint>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "dbus/bus.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_export.h"

namespace bluez {

// BluetoothAgentServiceProvider is used to provide a D-Bus object that
// the bluetooth daemon can communicate with during a remote device pairing
// request.
//
// Instantiate with a chosen D-Bus object path and delegate object, and pass
// the D-Bus object path as the |agent_path| argument to the
// bluez::BluetoothAgentManagerClient::RegisterAgent() method.
//
// After initiating the pairing process with a device, using the
// bluez::BluetoothDeviceClient::Pair() method, the Bluetooth daemon will
// make calls to this agent object and they will be passed on to your Delegate
// object for handling. Responses should be returned using the callbacks
// supplied to those methods.
class DEVICE_BLUETOOTH_EXPORT BluetoothAgentServiceProvider {
 public:
  // Interface for reacting to agent requests.
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Possible status values that may be returned to callbacks. Success
    // indicates that a pincode or passkey has been obtained, or permission
    // granted; rejected indicates the user rejected the request or denied
    // permission; cancelled indicates the user cancelled the request
    // without confirming either way.
    enum Status { SUCCESS, REJECTED, CANCELLED };

    // The PinCodeCallback is used for the RequestPinCode() method, it should
    // be called with two arguments, the |status| of the request (success,
    // rejected or cancelled) and the |pincode| requested.
    using PinCodeCallback =
        base::OnceCallback<void(Status, const std::string&)>;

    // The PasskeyCallback is used for the RequestPasskey() method, it should
    // be called with two arguments, the |status| of the request (success,
    // rejected or cancelled) and the |passkey| requested, a numeric in the
    // range 0-999999,
    using PasskeyCallback = base::OnceCallback<void(Status, uint32_t)>;

    // The ConfirmationCallback is used for methods which request confirmation
    // or authorization, it should be called with one argument, the |status|
    // of the request (success, rejected or cancelled).
    using ConfirmationCallback = base::OnceCallback<void(Status)>;

    // This method will be called when the agent is unregistered from the
    // Bluetooth daemon, generally at the end of a pairing request. It may be
    // used to perform cleanup tasks. This corresponds to the
    // org.bluez.Agent1.Release method and is renamed to avoid a conflict
    // with base::Refcounted<T>.
    virtual void Released() = 0;

    // This method will be called when the Bluetooth daemon requires a
    // PIN Code for authentication of the device with object path |device_path|,
    // the agent should obtain the code from the user and call |callback|
    // to provide it, or indicate rejection or cancellation of the request.
    //
    // PIN Codes are generally required for Bluetooth 2.0 and earlier devices
    // for which there is no automatic pairing or special handling.
    virtual void RequestPinCode(const dbus::ObjectPath& device_path,
                                PinCodeCallback callback) = 0;

    // This method will be called when the Bluetooth daemon requires that the
    // user enter the PIN code |pincode| into the device with object path
    // |device_path| so that it may be authenticated. The Cancel() method
    // will be called to dismiss the display once pairing is complete or
    // cancelled.
    //
    // This is used for Bluetooth 2.0 and earlier keyboard devices, the
    // |pincode| will always be a six-digit numeric in the range 000000-999999
    // for compatibilty with later specifications.
    virtual void DisplayPinCode(const dbus::ObjectPath& device_path,
                                const std::string& pincode) = 0;

    // This method will be called when the Bluetooth daemon requires a
    // Passkey for authentication of the device with object path |device_path|,
    // the agent should obtain the passkey from the user (a numeric in the
    // range 0-999999) and call |callback| to provide it, or indicate
    // rejection or cancellation of the request.
    //
    // Passkeys are generally required for Bluetooth 2.1 and later devices
    // which cannot provide input or display on their own, and don't accept
    // passkey-less pairing.
    virtual void RequestPasskey(const dbus::ObjectPath& device_path,
                                PasskeyCallback callback) = 0;

    // This method will be called when the Bluetooth daemon requires that the
    // user enter the Passkey |passkey| into the device with object path
    // |device_path| so that it may be authenticated. The Cancel() method
    // will be called to dismiss the display once pairing is complete or
    // cancelled.
    //
    // This is used for Bluetooth 2.1 and later devices that support input
    // but not display, such as keyboards. The Passkey is a numeric in the
    // range 0-999999 and should be always presented zero-padded to six
    // digits.
    //
    // As the user enters the passkey onto the device, |entered| will be
    // updated to reflect the number of digits entered so far.
    virtual void DisplayPasskey(const dbus::ObjectPath& device_path,
                                uint32_t passkey,
                                uint16_t entered) = 0;

    // This method will be called when the Bluetooth daemon requires that the
    // user confirm that the Passkey |passkey| is displayed on the screen
    // of the device with object path |object_path| so that it may be
    // authenticated. The agent should display to the user and ask for
    // confirmation, then call |callback| to provide their response (success,
    // rejected or cancelled).
    //
    // This is used for Bluetooth 2.1 and later devices that support display,
    // such as other computers or phones. The Passkey is a numeric in the
    // range 0-999999 and should be always present zero-padded to six
    // digits.
    virtual void RequestConfirmation(const dbus::ObjectPath& device_path,
                                     uint32_t passkey,
                                     ConfirmationCallback callback) = 0;

    // This method will be called when the Bluetooth daemon requires
    // authorization of an incoming pairing attempt from the device with object
    // path |device_path| that would have otherwised triggered the just-works
    // pairing model.
    //
    // The agent should confirm the incoming pairing with the user and call
    // |callback| to provide their response (success, rejected or cancelled).
    virtual void RequestAuthorization(const dbus::ObjectPath& device_path,
                                      ConfirmationCallback callback) = 0;

    // This method will be called when the Bluetooth daemon requires that the
    // user confirm that the device with object path |object_path| is
    // authorized to connect to the service with UUID |uuid|. The agent should
    // confirm with the user and call |callback| to provide their response
    // (success, rejected or cancelled).
    virtual void AuthorizeService(const dbus::ObjectPath& device_path,
                                  const std::string& uuid,
                                  ConfirmationCallback callback) = 0;

    // This method will be called by the Bluetooth daemon to indicate that
    // the request failed before a reply was returned from the device.
    virtual void Cancel() = 0;
  };

  virtual ~BluetoothAgentServiceProvider();

  // Creates the instance where |bus| is the D-Bus bus connection to export
  // the object onto, |object_path| is the object path that it should have
  // and |delegate| is the object to which all method calls will be passed
  // and responses generated from.
  static BluetoothAgentServiceProvider* Create(
      dbus::Bus* bus,
      const dbus::ObjectPath& object_path,
      Delegate* delegate);

 protected:
  BluetoothAgentServiceProvider();

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothAgentServiceProvider);
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_DBUS_BLUETOOTH_AGENT_SERVICE_PROVIDER_H_
