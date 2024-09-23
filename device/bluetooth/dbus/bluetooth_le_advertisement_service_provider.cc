// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/bluetooth_le_advertisement_service_provider.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/threading/platform_thread.h"
#include "dbus/exported_object.h"
#include "dbus/message.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/fake_bluetooth_le_advertisement_service_provider.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace bluez {

namespace {
const char kErrorInvalidArgs[] = "org.freedesktop.DBus.Error.InvalidArgs";
}  // namespace

// The BluetoothAdvertisementServiceProvider implementation used in production.
class BluetoothAdvertisementServiceProviderImpl
    : public BluetoothLEAdvertisementServiceProvider {
 public:
  BluetoothAdvertisementServiceProviderImpl(
      dbus::Bus* bus,
      const dbus::ObjectPath& object_path,
      Delegate* delegate,
      bool adapter_support_ext_adv,
      AdvertisementType type,
      std::optional<UUIDList> service_uuids,
      std::optional<ManufacturerData> manufacturer_data,
      std::optional<UUIDList> solicit_uuids,
      std::optional<ServiceData> service_data,
      std::optional<ScanResponseData> scan_response_data)
      : origin_thread_id_(base::PlatformThread::CurrentId()),
        bus_(bus),
        delegate_(delegate),
        adapter_support_ext_adv_(adapter_support_ext_adv),
        type_(type),
        service_uuids_(std::move(service_uuids)),
        manufacturer_data_(std::move(manufacturer_data)),
        solicit_uuids_(std::move(solicit_uuids)),
        service_data_(std::move(service_data)),
        scan_response_data_(std::move(scan_response_data)) {
    DCHECK(bus);
    DCHECK(delegate);

    DVLOG(1) << "Creating Bluetooth Advertisement: " << object_path_.value();

    object_path_ = object_path;
    exported_object_ = bus_->GetExportedObject(object_path_);

    // Export Bluetooth Advertisement interface methods.
    exported_object_->ExportMethod(
        bluetooth_advertisement::kBluetoothAdvertisementInterface,
        bluetooth_advertisement::kRelease,
        base::BindRepeating(&BluetoothAdvertisementServiceProviderImpl::Release,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&BluetoothAdvertisementServiceProviderImpl::OnExported,
                       weak_ptr_factory_.GetWeakPtr()));

    // Export dbus property methods.
    exported_object_->ExportMethod(
        dbus::kDBusPropertiesInterface, dbus::kDBusPropertiesGet,
        base::BindRepeating(&BluetoothAdvertisementServiceProviderImpl::Get,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&BluetoothAdvertisementServiceProviderImpl::OnExported,
                       weak_ptr_factory_.GetWeakPtr()));

    exported_object_->ExportMethod(
        dbus::kDBusPropertiesInterface, dbus::kDBusPropertiesGetAll,
        base::BindRepeating(&BluetoothAdvertisementServiceProviderImpl::GetAll,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&BluetoothAdvertisementServiceProviderImpl::OnExported,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  BluetoothAdvertisementServiceProviderImpl(
      const BluetoothAdvertisementServiceProviderImpl&) = delete;
  BluetoothAdvertisementServiceProviderImpl& operator=(
      const BluetoothAdvertisementServiceProviderImpl&) = delete;

  ~BluetoothAdvertisementServiceProviderImpl() override {
    DVLOG(1) << "Cleaning up Bluetooth Advertisement: " << object_path_.value();

    // Unregister the object path so we can reuse with a new agent.
    bus_->UnregisterExportedObject(object_path_);
  }

 private:
  // Returns true if the current thread is on the origin thread.
  bool OnOriginThread() {
    return base::PlatformThread::CurrentId() == origin_thread_id_;
  }

  // Called by dbus:: when this advertisement is unregistered from the Bluetooth
  // daemon, generally by our request.
  void Release(dbus::MethodCall* method_call,
               dbus::ExportedObject::ResponseSender response_sender) {
    DCHECK(OnOriginThread());
    DCHECK(delegate_);

    delegate_->Released();
  }

  // Called by dbus:: when the Bluetooth daemon fetches a single property of
  // the descriptor.
  void Get(dbus::MethodCall* method_call,
           dbus::ExportedObject::ResponseSender response_sender) {
    DVLOG(2) << "BluetoothAdvertisementServiceProvider::Get: "
             << object_path_.value();
    DCHECK(OnOriginThread());

    dbus::MessageReader reader(method_call);

    std::string interface_name;
    std::string property_name;
    if (!reader.PopString(&interface_name) ||
        !reader.PopString(&property_name) || reader.HasMoreData()) {
      std::unique_ptr<dbus::ErrorResponse> error_response =
          dbus::ErrorResponse::FromMethodCall(method_call, kErrorInvalidArgs,
                                              "Expected 'ss'.");
      std::move(response_sender).Run(std::move(error_response));
      return;
    }

    // Only the advertisement interface is supported.
    if (interface_name !=
        bluetooth_advertisement::kBluetoothAdvertisementInterface) {
      std::unique_ptr<dbus::ErrorResponse> error_response =
          dbus::ErrorResponse::FromMethodCall(
              method_call, kErrorInvalidArgs,
              "No such interface: '" + interface_name + "'.");
      std::move(response_sender).Run(std::move(error_response));
      return;
    }

    std::unique_ptr<dbus::Response> response =
        dbus::Response::FromMethodCall(method_call);
    dbus::MessageWriter writer(response.get());
    dbus::MessageWriter variant_writer(NULL);

    if (property_name == bluetooth_advertisement::kTypeProperty) {
      writer.OpenVariant("s", &variant_writer);
      if (type_ == ADVERTISEMENT_TYPE_BROADCAST) {
        variant_writer.AppendString("broadcast");
      } else {
        variant_writer.AppendString("peripheral");
      }
    } else if ((property_name ==
                bluetooth_advertisement::kServiceUUIDsProperty) &&
               service_uuids_) {
      writer.OpenVariant("as", &variant_writer);
      variant_writer.AppendArrayOfStrings(*service_uuids_);
    } else if ((property_name ==
                bluetooth_advertisement::kSolicitUUIDsProperty) &&
               solicit_uuids_) {
      writer.OpenVariant("as", &variant_writer);
      variant_writer.AppendArrayOfStrings(*solicit_uuids_);
    } else if ((property_name ==
                bluetooth_advertisement::kManufacturerDataProperty) &&
               manufacturer_data_) {
      writer.OpenVariant("o", &variant_writer);
      AppendManufacturerDataVariant(&variant_writer);
    } else if ((property_name ==
                bluetooth_advertisement::kServiceDataProperty) &&
               service_data_) {
      writer.OpenVariant("o", &variant_writer);
      AppendServiceDataVariant(&variant_writer);
    } else if ((property_name ==
                bluetooth_advertisement::kScanResponseDataProperty) &&
               scan_response_data_) {
      writer.OpenVariant("o", &variant_writer);
      AppendScanResponseDataVariant(&variant_writer);
    } else if ((property_name ==
                bluetooth_advertisement::kSecondaryChannelProperty)) {
      if (UseSecondaryChannel()) {
        writer.OpenVariant("s", &variant_writer);
        variant_writer.AppendString(bluetooth_advertisement::kPhy1M);
      }
    } else {
      std::unique_ptr<dbus::ErrorResponse> error_response =
          dbus::ErrorResponse::FromMethodCall(
              method_call, kErrorInvalidArgs,
              "No such property: '" + property_name + "'.");
      std::move(response_sender).Run(std::move(error_response));
      return;
    }

    writer.CloseContainer(&variant_writer);
    std::move(response_sender).Run(std::move(response));
  }

  // Called by dbus:: when the Bluetooth daemon fetches all properties of the
  // descriptor.
  void GetAll(dbus::MethodCall* method_call,
              dbus::ExportedObject::ResponseSender response_sender) {
    DVLOG(2) << "BluetoothAdvertisementServiceProvider::GetAll: "
             << object_path_.value();
    DCHECK(OnOriginThread());

    dbus::MessageReader reader(method_call);

    std::string interface_name;
    if (!reader.PopString(&interface_name) || reader.HasMoreData()) {
      std::unique_ptr<dbus::ErrorResponse> error_response =
          dbus::ErrorResponse::FromMethodCall(method_call, kErrorInvalidArgs,
                                              "Expected 's'.");
      std::move(response_sender).Run(std::move(error_response));
      return;
    }

    // Only the advertisement interface is supported.
    if (interface_name !=
        bluetooth_advertisement::kBluetoothAdvertisementInterface) {
      std::unique_ptr<dbus::ErrorResponse> error_response =
          dbus::ErrorResponse::FromMethodCall(
              method_call, kErrorInvalidArgs,
              "No such interface: '" + interface_name + "'.");
      std::move(response_sender).Run(std::move(error_response));
      return;
    }

    std::move(response_sender).Run(CreateGetAllResponse(method_call));
  }

  // Called by dbus:: when a method is exported.
  void OnExported(const std::string& interface_name,
                  const std::string& method_name,
                  bool success) {
    DVLOG_IF(1, !success) << "Failed to export " << interface_name << "."
                          << method_name;
  }

  // Helper for populating the DBus response with the advertisement data.
  std::unique_ptr<dbus::Response> CreateGetAllResponse(
      dbus::MethodCall* method_call) {
    DVLOG(2) << "Descriptor value obtained from delegate. Responding to "
             << "GetAll.";

    std::unique_ptr<dbus::Response> response =
        dbus::Response::FromMethodCall(method_call);

    dbus::MessageWriter writer(response.get());
    dbus::MessageWriter array_writer(NULL);

    writer.OpenArray("{sv}", &array_writer);

    AppendType(&array_writer);
    AppendSecondaryChannel(&array_writer);
    AppendServiceUUIDs(&array_writer);
    AppendManufacturerData(&array_writer);
    AppendSolicitUUIDs(&array_writer);
    AppendServiceData(&array_writer);
    AppendScanResponseData(&array_writer);

    writer.CloseContainer(&array_writer);
    return response;
  }

  // Called by the Delegate in response to a successful method call to get the
  // descriptor value.
  void OnGet(dbus::MethodCall* method_call,
             dbus::ExportedObject::ResponseSender response_sender,
             const std::vector<uint8_t>& value) {
    DVLOG(2) << "Returning descriptor value obtained from delegate.";
    std::unique_ptr<dbus::Response> response =
        dbus::Response::FromMethodCall(method_call);
    dbus::MessageWriter writer(response.get());
    dbus::MessageWriter variant_writer(NULL);

    writer.OpenVariant("ay", &variant_writer);
    variant_writer.AppendArrayOfBytes(value);
    writer.CloseContainer(&variant_writer);

    std::move(response_sender).Run(std::move(response));
  }

  void AppendArrayVariantOfStrings(dbus::MessageWriter* dict_writer,
                                   const UUIDList& strings) {
    dbus::MessageWriter strings_array_variant(nullptr);
    dict_writer->OpenVariant("as", &strings_array_variant);
    strings_array_variant.AppendArrayOfStrings(strings);
    dict_writer->CloseContainer(&strings_array_variant);
  }

  void AppendType(dbus::MessageWriter* array_writer) {
    dbus::MessageWriter dict_entry_writer(NULL);
    array_writer->OpenDictEntry(&dict_entry_writer);
    dict_entry_writer.AppendString(bluetooth_advertisement::kTypeProperty);
    if (type_ == ADVERTISEMENT_TYPE_BROADCAST) {
      dict_entry_writer.AppendVariantOfString("broadcast");
    } else {
      dict_entry_writer.AppendVariantOfString("peripheral");
    }
    array_writer->CloseContainer(&dict_entry_writer);
  }

  void AppendSecondaryChannel(dbus::MessageWriter* array_writer) {
    if (!UseSecondaryChannel()) {
      return;
    }

    dbus::MessageWriter dict_entry_writer(nullptr);
    array_writer->OpenDictEntry(&dict_entry_writer);
    dict_entry_writer.AppendString(
        bluetooth_advertisement::kSecondaryChannelProperty);
    dict_entry_writer.AppendVariantOfString(bluetooth_advertisement::kPhy1M);
    array_writer->CloseContainer(&dict_entry_writer);
  }

  void AppendServiceUUIDs(dbus::MessageWriter* array_writer) {
    if (!service_uuids_)
      return;
    dbus::MessageWriter dict_entry_writer(NULL);
    array_writer->OpenDictEntry(&dict_entry_writer);
    dict_entry_writer.AppendString(
        bluetooth_advertisement::kServiceUUIDsProperty);
    AppendArrayVariantOfStrings(&dict_entry_writer, *service_uuids_);
    array_writer->CloseContainer(&dict_entry_writer);
  }

  void AppendManufacturerData(dbus::MessageWriter* array_writer) {
    if (!manufacturer_data_)
      return;
    dbus::MessageWriter dict_entry_writer(NULL);
    array_writer->OpenDictEntry(&dict_entry_writer);
    dict_entry_writer.AppendString(
        bluetooth_advertisement::kManufacturerDataProperty);
    dbus::MessageWriter variant_writer(NULL);
    dict_entry_writer.OpenVariant("a{qv}", &variant_writer);
    AppendManufacturerDataVariant(&variant_writer);
    dict_entry_writer.CloseContainer(&variant_writer);
    array_writer->CloseContainer(&dict_entry_writer);
  }

  void AppendSolicitUUIDs(dbus::MessageWriter* array_writer) {
    if (!solicit_uuids_)
      return;
    dbus::MessageWriter dict_entry_writer(NULL);
    array_writer->OpenDictEntry(&dict_entry_writer);
    dict_entry_writer.AppendString(
        bluetooth_advertisement::kSolicitUUIDsProperty);
    AppendArrayVariantOfStrings(&dict_entry_writer, *solicit_uuids_);
    array_writer->CloseContainer(&dict_entry_writer);
  }

  void AppendServiceData(dbus::MessageWriter* array_writer) {
    if (!service_data_)
      return;
    dbus::MessageWriter dict_entry_writer(NULL);
    array_writer->OpenDictEntry(&dict_entry_writer);
    dict_entry_writer.AppendString(
        bluetooth_advertisement::kServiceDataProperty);
    dbus::MessageWriter variant_writer(NULL);
    dict_entry_writer.OpenVariant("a{sv}", &variant_writer);
    AppendServiceDataVariant(&variant_writer);
    dict_entry_writer.CloseContainer(&variant_writer);
    array_writer->CloseContainer(&dict_entry_writer);
  }

  void AppendScanResponseData(dbus::MessageWriter* array_writer) {
    if (!scan_response_data_)
      return;
    dbus::MessageWriter dict_entry_writer(nullptr);
    array_writer->OpenDictEntry(&dict_entry_writer);
    dict_entry_writer.AppendString(
        bluetooth_advertisement::kScanResponseDataProperty);
    dbus::MessageWriter variant_writer(nullptr);
    dict_entry_writer.OpenVariant("a{yv}", &variant_writer);
    AppendScanResponseDataVariant(&variant_writer);
    dict_entry_writer.CloseContainer(&variant_writer);
    array_writer->CloseContainer(&dict_entry_writer);
  }

  void AppendManufacturerDataVariant(dbus::MessageWriter* writer) {
    DCHECK(manufacturer_data_);
    dbus::MessageWriter array_writer(nullptr);
    writer->OpenArray("{qv}", &array_writer);
    for (const auto& m : *manufacturer_data_) {
      dbus::MessageWriter entry_writer(nullptr);

      array_writer.OpenDictEntry(&entry_writer);

      entry_writer.AppendUint16(m.first);
      dbus::MessageWriter variant_writer(nullptr);
      entry_writer.OpenVariant("ay", &variant_writer);
      variant_writer.AppendArrayOfBytes(m.second);
      entry_writer.CloseContainer(&variant_writer);

      array_writer.CloseContainer(&entry_writer);
    }
    writer->CloseContainer(&array_writer);
  }

  void AppendServiceDataVariant(dbus::MessageWriter* writer) {
    DCHECK(service_data_);
    dbus::MessageWriter array_writer(nullptr);
    writer->OpenArray("{sv}", &array_writer);
    for (const auto& m : *service_data_) {
      dbus::MessageWriter entry_writer(nullptr);

      array_writer.OpenDictEntry(&entry_writer);

      entry_writer.AppendString(m.first);
      dbus::MessageWriter variant_writer(nullptr);
      entry_writer.OpenVariant("ay", &variant_writer);
      variant_writer.AppendArrayOfBytes(m.second);
      entry_writer.CloseContainer(&variant_writer);

      array_writer.CloseContainer(&entry_writer);
    }
    writer->CloseContainer(&array_writer);
  }

  void AppendScanResponseDataVariant(dbus::MessageWriter* writer) {
    DCHECK(scan_response_data_);
    dbus::MessageWriter array_writer(nullptr);
    writer->OpenArray("{yv}", &array_writer);
    for (const auto& m : *scan_response_data_) {
      dbus::MessageWriter entry_writer(nullptr);

      array_writer.OpenDictEntry(&entry_writer);

      entry_writer.AppendByte(m.first);
      dbus::MessageWriter variant_writer(nullptr);
      entry_writer.OpenVariant("ay", &variant_writer);
      variant_writer.AppendArrayOfBytes(m.second);
      entry_writer.CloseContainer(&variant_writer);

      array_writer.CloseContainer(&entry_writer);
    }
    writer->CloseContainer(&array_writer);
  }

  bool UseSecondaryChannel() {
    if (!adapter_support_ext_adv_) {
      return false;
    }

    // Use |scan_response_data| to determine if upper layer wants to use ext adv
    // because
    // 1. chrome actually knows if it wants to use ext adv or legacy adv,
    // but we are lack of interface to pass it to this object.
    // 2. when chrome registers advertisements, if ext adv is used, it doesn't
    // set scan response data (see
    // BleV2Medium::StartAdvertising in
    // /chrome/services/sharing/nearby/platform/ble_v2_medium.cc).
    // 3. some combinations of adv data + parameters + type are reserved by
    // the spec, which is usually disallowed by controllers, but it's not easy
    // to find them all.
    return !scan_response_data_;
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

  // Whether the adapter support extended advertisement or not.
  bool adapter_support_ext_adv_;

  // Advertisement data that needs to be provided to BlueZ when requested.
  AdvertisementType type_;
  std::optional<UUIDList> service_uuids_;
  std::optional<ManufacturerData> manufacturer_data_;
  std::optional<UUIDList> solicit_uuids_;
  std::optional<ServiceData> service_data_;
  std::optional<ScanResponseData> scan_response_data_;

  // D-Bus object we are exporting, owned by this object.
  scoped_refptr<dbus::ExportedObject> exported_object_;

  // Weak pointer factory for generating 'this' pointers that might live longer
  // than we do.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothAdvertisementServiceProviderImpl>
      weak_ptr_factory_{this};
};

BluetoothLEAdvertisementServiceProvider::
    BluetoothLEAdvertisementServiceProvider() = default;

BluetoothLEAdvertisementServiceProvider::
    ~BluetoothLEAdvertisementServiceProvider() = default;

// static
std::unique_ptr<BluetoothLEAdvertisementServiceProvider>
BluetoothLEAdvertisementServiceProvider::Create(
    dbus::Bus* bus,
    const dbus::ObjectPath& object_path,
    Delegate* delegate,
    bool adapter_support_ext_adv,
    AdvertisementType type,
    std::optional<UUIDList> service_uuids,
    std::optional<ManufacturerData> manufacturer_data,
    std::optional<UUIDList> solicit_uuids,
    std::optional<ServiceData> service_data,
    std::optional<ScanResponseData> scan_response_data) {
  if (!bluez::BluezDBusManager::Get()->IsUsingFakes()) {
    return std::make_unique<BluetoothAdvertisementServiceProviderImpl>(
        bus, object_path, delegate, adapter_support_ext_adv, type,
        std::move(service_uuids), std::move(manufacturer_data),
        std::move(solicit_uuids), std::move(service_data),
        std::move(scan_response_data));
  }
#if defined(USE_REAL_DBUS_CLIENTS)
  LOG(FATAL) << "Fake is unavailable if USE_REAL_DBUS_CLIENTS is defined.";
#else
  return std::make_unique<FakeBluetoothLEAdvertisementServiceProvider>(
      object_path, delegate);
#endif  // defined(USE_REAL_DBUS_CLIENTS)
}

}  // namespace bluez
