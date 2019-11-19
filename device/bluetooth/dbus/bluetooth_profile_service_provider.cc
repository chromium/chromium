// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/bluetooth_profile_service_provider.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/platform_thread.h"
#include "dbus/exported_object.h"
#include "dbus/message.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/fake_bluetooth_profile_service_provider.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace bluez {

// The BluetoothProfileServiceProvider implementation used in production.
class BluetoothProfileServiceProviderImpl
    : public BluetoothProfileServiceProvider {
 public:
  BluetoothProfileServiceProviderImpl(dbus::Bus* bus,
                                      const dbus::ObjectPath& object_path,
                                      Delegate* delegate)
      : origin_thread_id_(base::PlatformThread::CurrentId()),
        bus_(bus),
        delegate_(delegate),
        object_path_(object_path) {
    VLOG(1) << "Creating Bluetooth Profile: " << object_path_.value();

    exported_object_ = bus_->GetExportedObject(object_path_);

    exported_object_->ExportMethod(
        bluetooth_profile::kBluetoothProfileInterface,
        bluetooth_profile::kRelease,
        base::Bind(&BluetoothProfileServiceProviderImpl::Release,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&BluetoothProfileServiceProviderImpl::OnExported,
                   weak_ptr_factory_.GetWeakPtr()));

    exported_object_->ExportMethod(
        bluetooth_profile::kBluetoothProfileInterface,
        bluetooth_profile::kNewConnection,
        base::Bind(&BluetoothProfileServiceProviderImpl::NewConnection,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&BluetoothProfileServiceProviderImpl::OnExported,
                   weak_ptr_factory_.GetWeakPtr()));

    exported_object_->ExportMethod(
        bluetooth_profile::kBluetoothProfileInterface,
        bluetooth_profile::kRequestDisconnection,
        base::Bind(&BluetoothProfileServiceProviderImpl::RequestDisconnection,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&BluetoothProfileServiceProviderImpl::OnExported,
                   weak_ptr_factory_.GetWeakPtr()));

    exported_object_->ExportMethod(
        bluetooth_profile::kBluetoothProfileInterface,
        bluetooth_profile::kCancel,
        base::Bind(&BluetoothProfileServiceProviderImpl::Cancel,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&BluetoothProfileServiceProviderImpl::OnExported,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  ~BluetoothProfileServiceProviderImpl() override {
    VLOG(1) << "Cleaning up Bluetooth Profile: " << object_path_.value();

    // Unregister the object path so we can reuse with a new agent.
    bus_->UnregisterExportedObject(object_path_);
  }

 private:
  // Returns true if the current thread is on the origin thread.
  bool OnOriginThread() {
    return base::PlatformThread::CurrentId() == origin_thread_id_;
  }

  // Called by dbus:: when the profile is unregistered from the Bluetooth
  // daemon, generally by our request.
  void Release(dbus::MethodCall* method_call,
               dbus::ExportedObject::ResponseSender response_sender) {
    DCHECK(OnOriginThread());
    DCHECK(delegate_);

    delegate_->Released();

    response_sender.Run(dbus::Response::FromMethodCall(method_call));
  }

  // Called by dbus:: when the Bluetooth daemon establishes a new connection
  // to the profile.
  void NewConnection(dbus::MethodCall* method_call,
                     dbus::ExportedObject::ResponseSender response_sender) {
    DCHECK(OnOriginThread());
    DCHECK(delegate_);

    dbus::MessageReader reader(method_call);
    dbus::ObjectPath device_path;
    base::ScopedFD fd;
    dbus::MessageReader array_reader(NULL);
    if (!reader.PopObjectPath(&device_path) || !reader.PopFileDescriptor(&fd) ||
        !reader.PopArray(&array_reader)) {
      LOG(WARNING) << "NewConnection called with incorrect paramters: "
                   << method_call->ToString();
      return;
    }

    Delegate::Options options;
    while (array_reader.HasMoreData()) {
      dbus::MessageReader dict_entry_reader(NULL);
      std::string key;
      if (!array_reader.PopDictEntry(&dict_entry_reader) ||
          !dict_entry_reader.PopString(&key)) {
        LOG(WARNING) << "NewConnection called with incorrect paramters: "
                     << method_call->ToString();
      } else {
        if (key == bluetooth_profile::kVersionProperty)
          dict_entry_reader.PopVariantOfUint16(&options.version);
        else if (key == bluetooth_profile::kFeaturesProperty)
          dict_entry_reader.PopVariantOfUint16(&options.features);
      }
    }

    Delegate::ConfirmationCallback callback = base::BindOnce(
        &BluetoothProfileServiceProviderImpl::OnConfirmation,
        weak_ptr_factory_.GetWeakPtr(), method_call, response_sender);

    delegate_->NewConnection(device_path, std::move(fd), options,
                             std::move(callback));
  }

  // Called by dbus:: when the Bluetooth daemon is about to disconnect the
  // profile.
  void RequestDisconnection(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender) {
    DCHECK(OnOriginThread());
    DCHECK(delegate_);

    dbus::MessageReader reader(method_call);
    dbus::ObjectPath device_path;
    if (!reader.PopObjectPath(&device_path)) {
      LOG(WARNING) << "RequestDisconnection called with incorrect paramters: "
                   << method_call->ToString();
      return;
    }

    Delegate::ConfirmationCallback callback = base::BindOnce(
        &BluetoothProfileServiceProviderImpl::OnConfirmation,
        weak_ptr_factory_.GetWeakPtr(), method_call, response_sender);

    delegate_->RequestDisconnection(device_path, std::move(callback));
  }

  // Called by dbus:: when the request failed before a reply was returned
  // from the device.
  void Cancel(dbus::MethodCall* method_call,
              dbus::ExportedObject::ResponseSender response_sender) {
    DCHECK(OnOriginThread());
    DCHECK(delegate_);

    delegate_->Cancel();

    response_sender.Run(dbus::Response::FromMethodCall(method_call));
  }

  // Called by dbus:: when a method is exported.
  void OnExported(const std::string& interface_name,
                  const std::string& method_name,
                  bool success) {
    LOG_IF(WARNING, !success) << "Failed to export " << interface_name << "."
                              << method_name;
  }

  // Called by the Delegate in response to a method requiring confirmation.
  void OnConfirmation(dbus::MethodCall* method_call,
                      dbus::ExportedObject::ResponseSender response_sender,
                      Delegate::Status status) {
    DCHECK(OnOriginThread());

    switch (status) {
      case Delegate::SUCCESS: {
        response_sender.Run(dbus::Response::FromMethodCall(method_call));
        break;
      }
      case Delegate::REJECTED: {
        response_sender.Run(dbus::ErrorResponse::FromMethodCall(
            method_call, bluetooth_profile::kErrorRejected, "rejected"));
        break;
      }
      case Delegate::CANCELLED: {
        response_sender.Run(dbus::ErrorResponse::FromMethodCall(
            method_call, bluetooth_profile::kErrorCanceled, "canceled"));
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
  dbus::Bus* bus_;

  // All incoming method calls are passed on to the Delegate and a callback
  // passed to generate the reply. |delegate_| is generally the object that
  // owns this one, and must outlive it.
  Delegate* delegate_;

  // D-Bus object path of object we are exporting, kept so we can unregister
  // again in our destructor.
  dbus::ObjectPath object_path_;

  // D-Bus object we are exporting, owned by this object.
  scoped_refptr<dbus::ExportedObject> exported_object_;

  // Weak pointer factory for generating 'this' pointers that might live longer
  // than we do.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothProfileServiceProviderImpl> weak_ptr_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(BluetoothProfileServiceProviderImpl);
};

BluetoothProfileServiceProvider::BluetoothProfileServiceProvider() = default;

BluetoothProfileServiceProvider::~BluetoothProfileServiceProvider() = default;

// static
BluetoothProfileServiceProvider* BluetoothProfileServiceProvider::Create(
    dbus::Bus* bus,
    const dbus::ObjectPath& object_path,
    Delegate* delegate) {
  if (!bluez::BluezDBusManager::Get()->IsUsingFakes()) {
    return new BluetoothProfileServiceProviderImpl(bus, object_path, delegate);
  }
  return new FakeBluetoothProfileServiceProvider(object_path, delegate);
}

}  // namespace bluez
