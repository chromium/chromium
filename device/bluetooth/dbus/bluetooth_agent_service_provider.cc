// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/bluetooth_agent_service_provider.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/threading/platform_thread.h"
#include "dbus/exported_object.h"
#include "dbus/message.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/fake_bluetooth_agent_service_provider.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace bluez {

// The BluetoothAgentServiceProvider implementation used in production.
class BluetoothAgentServiceProviderImpl : public BluetoothAgentServiceProvider {
 public:
  BluetoothAgentServiceProviderImpl(dbus::Bus* bus,
                                    const dbus::ObjectPath& object_path,
                                    Delegate* delegate)
      : origin_thread_id_(base::PlatformThread::CurrentId()),
        bus_(bus),
        delegate_(delegate),
        object_path_(object_path) {
    DVLOG(1) << "Creating Bluetooth Agent: " << object_path_.value();

    exported_object_ = bus_->GetExportedObject(object_path_);

    exported_object_->ExportMethod(
        bluetooth_agent::kBluetoothAgentInterface, bluetooth_agent::kRelease,
        base::BindRepeating(&BluetoothAgentServiceProviderImpl::Release,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&BluetoothAgentServiceProviderImpl::OnExported,
                       weak_ptr_factory_.GetWeakPtr()));

    exported_object_->ExportMethod(
        bluetooth_agent::kBluetoothAgentInterface,
        bluetooth_agent::kRequestPinCode,
        base::BindRepeating(&BluetoothAgentServiceProviderImpl::RequestPinCode,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&BluetoothAgentServiceProviderImpl::OnExported,
                       weak_ptr_factory_.GetWeakPtr()));

    exported_object_->ExportMethod(
        bluetooth_agent::kBluetoothAgentInterface,
        bluetooth_agent::kDisplayPinCode,
        base::BindRepeating(&BluetoothAgentServiceProviderImpl::DisplayPinCode,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&BluetoothAgentServiceProviderImpl::OnExported,
                       weak_ptr_factory_.GetWeakPtr()));

    exported_object_->ExportMethod(
        bluetooth_agent::kBluetoothAgentInterface,
        bluetooth_agent::kRequestPasskey,
        base::BindRepeating(&BluetoothAgentServiceProviderImpl::RequestPasskey,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&BluetoothAgentServiceProviderImpl::OnExported,
                       weak_ptr_factory_.GetWeakPtr()));

    exported_object_->ExportMethod(
        bluetooth_agent::kBluetoothAgentInterface,
        bluetooth_agent::kDisplayPasskey,
        base::BindRepeating(&BluetoothAgentServiceProviderImpl::DisplayPasskey,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&BluetoothAgentServiceProviderImpl::OnExported,
                       weak_ptr_factory_.GetWeakPtr()));

    exported_object_->ExportMethod(
        bluetooth_agent::kBluetoothAgentInterface,
        bluetooth_agent::kRequestConfirmation,
        base::BindRepeating(
            &BluetoothAgentServiceProviderImpl::RequestConfirmation,
            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&BluetoothAgentServiceProviderImpl::OnExported,
                       weak_ptr_factory_.GetWeakPtr()));

    exported_object_->ExportMethod(
        bluetooth_agent::kBluetoothAgentInterface,
        bluetooth_agent::kRequestAuthorization,
        base::BindRepeating(
            &BluetoothAgentServiceProviderImpl::RequestAuthorization,
            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&BluetoothAgentServiceProviderImpl::OnExported,
                       weak_ptr_factory_.GetWeakPtr()));

    exported_object_->ExportMethod(
        bluetooth_agent::kBluetoothAgentInterface,
        bluetooth_agent::kAuthorizeService,
        base::BindRepeating(
            &BluetoothAgentServiceProviderImpl::AuthorizeService,
            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&BluetoothAgentServiceProviderImpl::OnExported,
                       weak_ptr_factory_.GetWeakPtr()));

    exported_object_->ExportMethod(
        bluetooth_agent::kBluetoothAgentInterface, bluetooth_agent::kCancel,
        base::BindRepeating(&BluetoothAgentServiceProviderImpl::Cancel,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&BluetoothAgentServiceProviderImpl::OnExported,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  BluetoothAgentServiceProviderImpl(const BluetoothAgentServiceProviderImpl&) =
      delete;
  BluetoothAgentServiceProviderImpl& operator=(
      const BluetoothAgentServiceProviderImpl&) = delete;

  ~BluetoothAgentServiceProviderImpl() override {
    DVLOG(1) << "Cleaning up Bluetooth Agent: " << object_path_.value();

    // Unregister the object path so we can reuse with a new agent.
    bus_->UnregisterExportedObject(object_path_);
  }

 private:
  // Returns true if the current thread is on the origin thread.
  bool OnOriginThread() {
    return base::PlatformThread::CurrentId() == origin_thread_id_;
  }

  // Called by dbus:: when the agent is unregistered from the Bluetooth
  // daemon, generally at the end of a pairing request.
  void Release(dbus::MethodCall* method_call,
               dbus::ExportedObject::ResponseSender response_sender) {
    DCHECK(OnOriginThread());
    DCHECK(delegate_);

    delegate_->Released();

    std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
  }

  // Called by dbus:: when the Bluetooth daemon requires a PIN Code for
  // device authentication.
  void RequestPinCode(dbus::MethodCall* method_call,
                      dbus::ExportedObject::ResponseSender response_sender) {
    DCHECK(OnOriginThread());
    DCHECK(delegate_);

    dbus::MessageReader reader(method_call);
    dbus::ObjectPath device_path;
    if (!reader.PopObjectPath(&device_path)) {
      LOG(WARNING) << "RequestPinCode called with incorrect paramters: "
                   << method_call->ToString();
      return;
    }

    Delegate::PinCodeCallback callback =
        base::BindOnce(&BluetoothAgentServiceProviderImpl::OnPinCode,
                       weak_ptr_factory_.GetWeakPtr(), method_call,
                       std::move(response_sender));

    delegate_->RequestPinCode(device_path, std::move(callback));
  }

  // Called by dbus:: when the Bluetooth daemon requires that the user
  // enter a PIN Code into the remote device so that it may be
  // authenticated.
  void DisplayPinCode(dbus::MethodCall* method_call,
                      dbus::ExportedObject::ResponseSender response_sender) {
    DCHECK(OnOriginThread());
    DCHECK(delegate_);

    dbus::MessageReader reader(method_call);
    dbus::ObjectPath device_path;
    std::string pincode;
    if (!reader.PopObjectPath(&device_path) || !reader.PopString(&pincode)) {
      LOG(WARNING) << "DisplayPinCode called with incorrect paramters: "
                   << method_call->ToString();
      return;
    }

    delegate_->DisplayPinCode(device_path, pincode);

    std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
  }

  // Called by dbus:: when the Bluetooth daemon requires a Passkey for
  // device authentication.
  void RequestPasskey(dbus::MethodCall* method_call,
                      dbus::ExportedObject::ResponseSender response_sender) {
    DCHECK(OnOriginThread());
    DCHECK(delegate_);

    dbus::MessageReader reader(method_call);
    dbus::ObjectPath device_path;
    if (!reader.PopObjectPath(&device_path)) {
      LOG(WARNING) << "RequestPasskey called with incorrect paramters: "
                   << method_call->ToString();
      return;
    }

    Delegate::PasskeyCallback callback =
        base::BindOnce(&BluetoothAgentServiceProviderImpl::OnPasskey,
                       weak_ptr_factory_.GetWeakPtr(), method_call,
                       std::move(response_sender));

    delegate_->RequestPasskey(device_path, std::move(callback));
  }

  // Called by dbus:: when the Bluetooth daemon requires that the user
  // enter a Passkey into the remote device so that it may be
  // authenticated.
  void DisplayPasskey(dbus::MethodCall* method_call,
                      dbus::ExportedObject::ResponseSender response_sender) {
    DCHECK(OnOriginThread());
    DCHECK(delegate_);

    dbus::MessageReader reader(method_call);
    dbus::ObjectPath device_path;
    uint32_t passkey;
    uint16_t entered;
    if (!reader.PopObjectPath(&device_path) || !reader.PopUint32(&passkey) ||
        !reader.PopUint16(&entered)) {
      LOG(WARNING) << "DisplayPasskey called with incorrect paramters: "
                   << method_call->ToString();
      return;
    }

    delegate_->DisplayPasskey(device_path, passkey, entered);

    std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
  }

  // Called by dbus:: when the Bluetooth daemon requires that the user
  // confirm that a Passkey is displayed on the screen of the remote
  // device so that it may be authenticated.
  void RequestConfirmation(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender) {
    DCHECK(OnOriginThread());
    DCHECK(delegate_);

    dbus::MessageReader reader(method_call);
    dbus::ObjectPath device_path;
    uint32_t passkey;
    if (!reader.PopObjectPath(&device_path) || !reader.PopUint32(&passkey)) {
      LOG(WARNING) << "RequestConfirmation called with incorrect paramters: "
                   << method_call->ToString();
      return;
    }

    Delegate::ConfirmationCallback callback =
        base::BindOnce(&BluetoothAgentServiceProviderImpl::OnConfirmation,
                       weak_ptr_factory_.GetWeakPtr(), method_call,
                       std::move(response_sender));

    delegate_->RequestConfirmation(device_path, passkey, std::move(callback));
  }

  // Called by dbus:: when the Bluetooth daemon requires that the user
  // confirm an incoming just-works pairing.
  void RequestAuthorization(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender) {
    DCHECK(OnOriginThread());
    DCHECK(delegate_);

    dbus::MessageReader reader(method_call);
    dbus::ObjectPath device_path;
    if (!reader.PopObjectPath(&device_path)) {
      LOG(WARNING) << "RequestAuthorization called with incorrect paramters: "
                   << method_call->ToString();
      return;
    }

    Delegate::ConfirmationCallback callback =
        base::BindOnce(&BluetoothAgentServiceProviderImpl::OnConfirmation,
                       weak_ptr_factory_.GetWeakPtr(), method_call,
                       std::move(response_sender));

    delegate_->RequestAuthorization(device_path, std::move(callback));
  }

  // Called by dbus:: when the Bluetooth daemon requires that the user
  // confirm that that a remote device is authorized to connect to a service
  // UUID.
  void AuthorizeService(dbus::MethodCall* method_call,
                        dbus::ExportedObject::ResponseSender response_sender) {
    DCHECK(OnOriginThread());
    DCHECK(delegate_);

    dbus::MessageReader reader(method_call);
    dbus::ObjectPath device_path;
    std::string uuid;
    if (!reader.PopObjectPath(&device_path) || !reader.PopString(&uuid)) {
      LOG(WARNING) << "AuthorizeService called with incorrect paramters: "
                   << method_call->ToString();
      return;
    }

    Delegate::ConfirmationCallback callback =
        base::BindOnce(&BluetoothAgentServiceProviderImpl::OnConfirmation,
                       weak_ptr_factory_.GetWeakPtr(), method_call,
                       std::move(response_sender));

    delegate_->AuthorizeService(device_path, uuid, std::move(callback));
  }

  // Called by dbus:: when the request failed before a reply was returned
  // from the device.
  void Cancel(dbus::MethodCall* method_call,
              dbus::ExportedObject::ResponseSender response_sender) {
    DCHECK(OnOriginThread());
    DCHECK(delegate_);

    delegate_->Cancel();

    std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
  }

  // Called by dbus:: when a method is exported.
  void OnExported(const std::string& interface_name,
                  const std::string& method_name,
                  bool success) {
    DVLOG_IF(1, !success) << "Failed to export " << interface_name << "."
                          << method_name;
  }

  // Called by the Delegate to response to a method requesting a PIN code.
  void OnPinCode(dbus::MethodCall* method_call,
                 dbus::ExportedObject::ResponseSender response_sender,
                 Delegate::Status status,
                 const std::string& pincode) {
    DCHECK(OnOriginThread());

    switch (status) {
      case Delegate::SUCCESS: {
        std::unique_ptr<dbus::Response> response(
            dbus::Response::FromMethodCall(method_call));
        dbus::MessageWriter writer(response.get());
        writer.AppendString(pincode);
        std::move(response_sender).Run(std::move(response));
        break;
      }
      case Delegate::REJECTED: {
        std::move(response_sender)
            .Run(dbus::ErrorResponse::FromMethodCall(
                method_call, bluetooth_agent::kErrorRejected, "rejected"));
        break;
      }
      case Delegate::CANCELLED: {
        std::move(response_sender)
            .Run(dbus::ErrorResponse::FromMethodCall(
                method_call, bluetooth_agent::kErrorCanceled, "canceled"));
        break;
      }
      default:
        NOTREACHED() << "Unexpected status code from delegate: " << status;
    }
  }

  // Called by the Delegate to response to a method requesting a Passkey.
  void OnPasskey(dbus::MethodCall* method_call,
                 dbus::ExportedObject::ResponseSender response_sender,
                 Delegate::Status status,
                 uint32_t passkey) {
    DCHECK(OnOriginThread());

    switch (status) {
      case Delegate::SUCCESS: {
        std::unique_ptr<dbus::Response> response(
            dbus::Response::FromMethodCall(method_call));
        dbus::MessageWriter writer(response.get());
        writer.AppendUint32(passkey);
        std::move(response_sender).Run(std::move(response));
        break;
      }
      case Delegate::REJECTED: {
        std::move(response_sender)
            .Run(dbus::ErrorResponse::FromMethodCall(
                method_call, bluetooth_agent::kErrorRejected, "rejected"));
        break;
      }
      case Delegate::CANCELLED: {
        std::move(response_sender)
            .Run(dbus::ErrorResponse::FromMethodCall(
                method_call, bluetooth_agent::kErrorCanceled, "canceled"));
        break;
      }
      default:
        NOTREACHED() << "Unexpected status code from delegate: " << status;
    }
  }

  // Called by the Delegate in response to a method requiring confirmation.
  void OnConfirmation(dbus::MethodCall* method_call,
                      dbus::ExportedObject::ResponseSender response_sender,
                      Delegate::Status status) {
    DCHECK(OnOriginThread());

    switch (status) {
      case Delegate::SUCCESS: {
        std::move(response_sender)
            .Run(dbus::Response::FromMethodCall(method_call));
        break;
      }
      case Delegate::REJECTED: {
        std::move(response_sender)
            .Run(dbus::ErrorResponse::FromMethodCall(
                method_call, bluetooth_agent::kErrorRejected, "rejected"));
        break;
      }
      case Delegate::CANCELLED: {
        std::move(response_sender)
            .Run(dbus::ErrorResponse::FromMethodCall(
                method_call, bluetooth_agent::kErrorCanceled, "canceled"));
        break;
      }
      default:
        NOTREACHED() << "Unexpected status code from delegate: " << status;
    }
  }

  // Origin thread (i.e. the UI thread in production).
  base::PlatformThreadId origin_thread_id_;

  // D-Bus bus object is exported on, not owned by this object and must
  // outlive it.
  raw_ptr<dbus::Bus> bus_;

  // All incoming method calls are passed on to the Delegate and a callback
  // passed to generate the reply. |delegate_| is generally the object that
  // owns this one, and must outlive it.
  raw_ptr<Delegate> delegate_;

  // D-Bus object path of object we are exporting, kept so we can unregister
  // again in our destructor.
  dbus::ObjectPath object_path_;

  // D-Bus object we are exporting, owned by this object.
  scoped_refptr<dbus::ExportedObject> exported_object_;

  // Weak pointer factory for generating 'this' pointers that might live longer
  // than we do.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothAgentServiceProviderImpl> weak_ptr_factory_{
      this};
};

BluetoothAgentServiceProvider::BluetoothAgentServiceProvider() = default;

BluetoothAgentServiceProvider::~BluetoothAgentServiceProvider() = default;

// static
BluetoothAgentServiceProvider* BluetoothAgentServiceProvider::Create(
    dbus::Bus* bus,
    const dbus::ObjectPath& object_path,
    Delegate* delegate) {
  if (!bluez::BluezDBusManager::Get()->IsUsingFakes()) {
    return new BluetoothAgentServiceProviderImpl(bus, object_path, delegate);
  }
#if defined(USE_REAL_DBUS_CLIENTS)
  LOG(FATAL) << "Fake is unavailable if USE_REAL_DBUS_CLIENTS is defined.";
#else
  return new FakeBluetoothAgentServiceProvider(object_path, delegate);
#endif  // defined(USE_REAL_DBUS_CLIENTS)
}

}  // namespace bluez
