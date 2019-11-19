// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_adapter_winrt.h"

#include <windows.foundation.collections.h>
#include <windows.foundation.h>
#include <windows.storage.streams.h>
#include <wrl/event.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_native_library.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/win/core_winrt_util.h"
#include "base/win/post_async_results.h"
#include "device/bluetooth/bluetooth_advertisement_winrt.h"
#include "device/bluetooth/bluetooth_device_winrt.h"
#include "device/bluetooth/bluetooth_discovery_filter.h"
#include "device/bluetooth/bluetooth_discovery_session_outcome.h"
#include "device/bluetooth/event_utils_winrt.h"

namespace device {

namespace {

// In order to avoid a name clash with device::BluetoothAdapter we need this
// auxiliary namespace.
namespace uwp {
using ABI::Windows::Devices::Bluetooth::BluetoothAdapter;
}  // namespace uwp
using ABI::Windows::Devices::Bluetooth::IBluetoothAdapter;
using ABI::Windows::Devices::Bluetooth::IBluetoothAdapterStatics;
using ABI::Windows::Devices::Bluetooth::IID_IBluetoothAdapterStatics;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementDataSection;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementFlags;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementWatcherStatus;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementWatcherStatus_Aborted;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEManufacturerData;
using ABI::Windows::Devices::Bluetooth::Advertisement::BluetoothLEScanningMode;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEScanningMode_Active;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    IBluetoothLEAdvertisement;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    IBluetoothLEAdvertisementDataSection;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    IBluetoothLEAdvertisementPublisherFactory;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    IBluetoothLEAdvertisementReceivedEventArgs;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    IBluetoothLEAdvertisementWatcher;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    IBluetoothLEManufacturerData;
using ABI::Windows::Devices::Enumeration::DeviceInformation;
using ABI::Windows::Devices::Enumeration::IDeviceInformation;
using ABI::Windows::Devices::Enumeration::IDeviceInformationStatics;
using ABI::Windows::Devices::Enumeration::IDeviceInformationUpdate;
using ABI::Windows::Devices::Enumeration::IDeviceWatcher;
using ABI::Windows::Devices::Enumeration::IID_IDeviceInformationStatics;
using ABI::Windows::Devices::Radios::IID_IRadioStatics;
using ABI::Windows::Devices::Radios::IRadio;
using ABI::Windows::Devices::Radios::IRadioStatics;
using ABI::Windows::Devices::Radios::Radio;
using ABI::Windows::Devices::Radios::RadioAccessStatus;
using ABI::Windows::Devices::Radios::RadioAccessStatus_Allowed;
using ABI::Windows::Devices::Radios::RadioAccessStatus_DeniedBySystem;
using ABI::Windows::Devices::Radios::RadioAccessStatus_DeniedByUser;
using ABI::Windows::Devices::Radios::RadioAccessStatus_Unspecified;
using ABI::Windows::Devices::Radios::RadioState;
using ABI::Windows::Devices::Radios::RadioState_Off;
using ABI::Windows::Devices::Radios::RadioState_On;
using ABI::Windows::Foundation::IAsyncOperation;
using ABI::Windows::Foundation::IReference;
using ABI::Windows::Foundation::Collections::IVector;
using ABI::Windows::Foundation::Collections::IVectorView;
using ABI::Windows::Storage::Streams::IBuffer;
using ABI::Windows::Storage::Streams::IDataReader;
using ABI::Windows::Storage::Streams::IDataReaderStatics;
using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

bool ResolveCoreWinRT() {
  return base::win::ResolveCoreWinRTDelayload() &&
         base::win::ScopedHString::ResolveCoreWinRTStringDelayload();
}

// Query string for powered Bluetooth radios. GUID Reference:
// https://docs.microsoft.com/en-us/windows-hardware/drivers/install/guid-bthport-device-interface
// TODO(https://crbug.com/821766): Consider adding WindowsCreateStringReference
// to base::win::ScopedHString to avoid allocating memory for this string.
constexpr wchar_t kPoweredRadiosAqsFilter[] =
    L"System.Devices.InterfaceClassGuid:=\"{0850302A-B344-4fda-9BE9-"
    L"90576B8D46F0}\" AND "
    L"System.Devices.InterfaceEnabled:=System.StructuredQueryType.Boolean#True";

// Utility functions to pretty print enum values.
constexpr const char* ToCString(RadioAccessStatus access_status) {
  switch (access_status) {
    case RadioAccessStatus_Unspecified:
      return "RadioAccessStatus::Unspecified";
    case RadioAccessStatus_Allowed:
      return "RadioAccessStatus::Allowed";
    case RadioAccessStatus_DeniedByUser:
      return "RadioAccessStatus::DeniedByUser";
    case RadioAccessStatus_DeniedBySystem:
      return "RadioAccessStatus::DeniedBySystem";
  }

  NOTREACHED();
  return "";
}

template <typename VectorView, typename T>
bool ToStdVector(VectorView* view, std::vector<T>* vector) {
  unsigned size;
  HRESULT hr = view->get_Size(&size);
  if (FAILED(hr)) {
    VLOG(2) << "get_Size() failed: " << logging::SystemErrorCodeToString(hr);
    return false;
  }

  vector->resize(size);
  for (size_t i = 0; i < size; ++i) {
    hr = view->GetAt(i, &(*vector)[i]);
    DCHECK(SUCCEEDED(hr)) << "GetAt(" << i << ") failed: "
                          << logging::SystemErrorCodeToString(hr);
  }

  return true;
}

base::Optional<std::vector<uint8_t>> ExtractVector(IBuffer* buffer) {
  ComPtr<IDataReaderStatics> data_reader_statics;
  HRESULT hr = base::win::GetActivationFactory<
      IDataReaderStatics, RuntimeClass_Windows_Storage_Streams_DataReader>(
      &data_reader_statics);
  if (FAILED(hr)) {
    VLOG(2) << "Getting DataReaderStatics Activation Factory failed: "
            << logging::SystemErrorCodeToString(hr);
    return base::nullopt;
  }

  ComPtr<IDataReader> data_reader;
  hr = data_reader_statics->FromBuffer(buffer, &data_reader);
  if (FAILED(hr)) {
    VLOG(2) << "FromBuffer() failed: " << logging::SystemErrorCodeToString(hr);
    return base::nullopt;
  }

  uint32_t buffer_length;
  hr = buffer->get_Length(&buffer_length);
  if (FAILED(hr)) {
    VLOG(2) << "get_Length() failed: " << logging::SystemErrorCodeToString(hr);
    return base::nullopt;
  }

  std::vector<uint8_t> bytes(buffer_length);
  hr = data_reader->ReadBytes(buffer_length, bytes.data());
  if (FAILED(hr)) {
    VLOG(2) << "ReadBytes() failed: " << logging::SystemErrorCodeToString(hr);
    return base::nullopt;
  }

  return bytes;
}

base::Optional<uint8_t> ExtractFlags(IBluetoothLEAdvertisement* advertisement) {
  if (!advertisement)
    return base::nullopt;

  ComPtr<IReference<BluetoothLEAdvertisementFlags>> flags_ref;
  HRESULT hr = advertisement->get_Flags(&flags_ref);
  if (FAILED(hr)) {
    VLOG(2) << "get_Flags() failed: " << logging::SystemErrorCodeToString(hr);
    return base::nullopt;
  }

  if (!flags_ref) {
    VLOG(2) << "No advertisement flags found.";
    return base::nullopt;
  }

  BluetoothLEAdvertisementFlags flags;
  hr = flags_ref->get_Value(&flags);
  if (FAILED(hr)) {
    VLOG(2) << "get_Value() failed: " << logging::SystemErrorCodeToString(hr);
    return base::nullopt;
  }

  return flags;
}

BluetoothDevice::UUIDList ExtractAdvertisedUUIDs(
    IBluetoothLEAdvertisement* advertisement) {
  if (!advertisement)
    return {};

  ComPtr<IVector<GUID>> service_uuids;
  HRESULT hr = advertisement->get_ServiceUuids(&service_uuids);
  if (FAILED(hr)) {
    VLOG(2) << "get_ServiceUuids() failed: "
            << logging::SystemErrorCodeToString(hr);
    return {};
  }

  std::vector<GUID> guids;
  if (!ToStdVector(service_uuids.Get(), &guids))
    return {};

  BluetoothDevice::UUIDList advertised_uuids;
  advertised_uuids.reserve(guids.size());
  for (const auto& guid : guids)
    advertised_uuids.emplace_back(guid);

  return advertised_uuids;
}

// This method populates service data for a particular sized UUID. Given the
// lack of tailored platform APIs, we need to parse the raw advertisement data
// sections ourselves. These data sections are effectively a list of blobs,
// where each blob starts with the corresponding UUID in little endian order,
// followed by the corresponding service data.
void PopulateServiceData(
    BluetoothDevice::ServiceDataMap* service_data,
    const std::vector<ComPtr<IBluetoothLEAdvertisementDataSection>>&
        data_sections,
    size_t num_bytes_uuid) {
  for (const auto& data_section : data_sections) {
    ComPtr<IBuffer> buffer;
    HRESULT hr = data_section->get_Data(&buffer);
    if (FAILED(hr)) {
      VLOG(2) << "get_Data() failed: " << logging::SystemErrorCodeToString(hr);
      continue;
    }

    auto bytes = ExtractVector(buffer.Get());
    if (!bytes)
      continue;

    auto bytes_span = base::make_span(*bytes);
    if (bytes_span.size() < num_bytes_uuid) {
      VLOG(2) << "Buffer Length is too small: " << bytes_span.size() << " vs. "
              << num_bytes_uuid;
      continue;
    }

    auto uuid_span = bytes_span.first(num_bytes_uuid);
    // The UUID is specified in little endian format, thus we reverse the bytes
    // here.
    std::vector<uint8_t> uuid_bytes(uuid_span.rbegin(), uuid_span.rend());

    // HexEncode the bytes and add dashes as required.
    std::string uuid_str;
    for (char c : base::HexEncode(uuid_bytes.data(), uuid_bytes.size())) {
      const size_t size = uuid_str.size();
      if (size == 8 || size == 13 || size == 18 || size == 23)
        uuid_str.push_back('-');
      uuid_str.push_back(c);
    }

    auto service_data_span = bytes_span.subspan(num_bytes_uuid);
    auto result = service_data->emplace(
        BluetoothUUID(uuid_str), std::vector<uint8_t>(service_data_span.begin(),
                                                      service_data_span.end()));
    // Check that an insertion happened.
    DCHECK(result.second);
    // Check that the inserted UUID is valid.
    DCHECK(result.first->first.IsValid());
  }
}

BluetoothDevice::ServiceDataMap ExtractServiceData(
    IBluetoothLEAdvertisement* advertisement) {
  BluetoothDevice::ServiceDataMap service_data;
  if (!advertisement)
    return service_data;

  static constexpr std::pair<uint8_t, size_t> kServiceDataTypesAndNumBits[] = {
      {BluetoothDeviceWinrt::k16BitServiceDataSection, 16},
      {BluetoothDeviceWinrt::k32BitServiceDataSection, 32},
      {BluetoothDeviceWinrt::k128BitServiceDataSection, 128},
  };

  for (const auto& data_type_and_num_bits : kServiceDataTypesAndNumBits) {
    ComPtr<IVectorView<BluetoothLEAdvertisementDataSection*>> data_sections;
    HRESULT hr = advertisement->GetSectionsByType(data_type_and_num_bits.first,
                                                  &data_sections);
    if (FAILED(hr)) {
      VLOG(2) << "GetSectionsByType() failed: "
              << logging::SystemErrorCodeToString(hr);
      continue;
    }

    std::vector<ComPtr<IBluetoothLEAdvertisementDataSection>> vector;
    if (!ToStdVector(data_sections.Get(), &vector))
      continue;

    PopulateServiceData(&service_data, vector,
                        data_type_and_num_bits.second / 8);
  }

  return service_data;
}

BluetoothDevice::ManufacturerDataMap ExtractManufacturerData(
    IBluetoothLEAdvertisement* advertisement) {
  if (!advertisement)
    return {};

  ComPtr<IVector<BluetoothLEManufacturerData*>> manufacturer_data_ptr;
  HRESULT hr = advertisement->get_ManufacturerData(&manufacturer_data_ptr);
  if (FAILED(hr)) {
    VLOG(2) << "GetManufacturerData() failed: "
            << logging::SystemErrorCodeToString(hr);
    return {};
  }

  std::vector<ComPtr<IBluetoothLEManufacturerData>> manufacturer_data;
  if (!ToStdVector(manufacturer_data_ptr.Get(), &manufacturer_data))
    return {};

  BluetoothDevice::ManufacturerDataMap manufacturer_data_map;
  for (const auto& manufacturer_datum : manufacturer_data) {
    uint16_t company_id;
    hr = manufacturer_datum->get_CompanyId(&company_id);
    if (FAILED(hr)) {
      VLOG(2) << "get_CompanyId() failed: "
              << logging::SystemErrorCodeToString(hr);
      continue;
    }

    ComPtr<IBuffer> buffer;
    hr = manufacturer_datum->get_Data(&buffer);
    if (FAILED(hr)) {
      VLOG(2) << "get_Data() failed: " << logging::SystemErrorCodeToString(hr);
      continue;
    }

    auto bytes = ExtractVector(buffer.Get());
    if (!bytes)
      continue;

    manufacturer_data_map.emplace(company_id, std::move(*bytes));
  }

  return manufacturer_data_map;
}

// Similarly to extracting the service data Windows does not provide a specific
// API to extract the tx power. Thus we also parse the raw data sections here.
// If present, we expect a single entry for tx power with a blob of size 1 byte.
base::Optional<int8_t> ExtractTxPower(
    IBluetoothLEAdvertisement* advertisement) {
  if (!advertisement)
    return base::nullopt;

  ComPtr<IVectorView<BluetoothLEAdvertisementDataSection*>> data_sections;
  HRESULT hr = advertisement->GetSectionsByType(
      BluetoothDeviceWinrt::kTxPowerLevelDataSection, &data_sections);
  if (FAILED(hr)) {
    VLOG(2) << "GetSectionsByType() failed: "
            << logging::SystemErrorCodeToString(hr);
    return base::nullopt;
  }

  std::vector<ComPtr<IBluetoothLEAdvertisementDataSection>> vector;
  if (!ToStdVector(data_sections.Get(), &vector) || vector.empty())
    return base::nullopt;

  if (vector.size() != 1u) {
    VLOG(2) << "Unexpected number of data sections: " << vector.size();
    return base::nullopt;
  }

  ComPtr<IBuffer> buffer;
  hr = vector.front()->get_Data(&buffer);
  if (FAILED(hr)) {
    VLOG(2) << "get_Data() failed: " << logging::SystemErrorCodeToString(hr);
    return base::nullopt;
  }

  auto bytes = ExtractVector(buffer.Get());
  if (!bytes)
    return base::nullopt;

  if (bytes->size() != 1) {
    VLOG(2) << "Unexpected number of bytes: " << bytes->size();
    return base::nullopt;
  }

  return bytes->front();
}

ComPtr<IBluetoothLEAdvertisement> GetAdvertisement(
    IBluetoothLEAdvertisementReceivedEventArgs* received) {
  ComPtr<IBluetoothLEAdvertisement> advertisement;
  HRESULT hr = received->get_Advertisement(&advertisement);
  if (FAILED(hr)) {
    VLOG(2) << "get_Advertisement() failed: "
            << logging::SystemErrorCodeToString(hr);
  }

  return advertisement;
}

base::Optional<std::string> ExtractDeviceName(
    IBluetoothLEAdvertisement* advertisement) {
  if (!advertisement)
    return base::nullopt;

  HSTRING local_name;
  HRESULT hr = advertisement->get_LocalName(&local_name);
  if (FAILED(hr)) {
    VLOG(2) << "Getting Local Name failed: "
            << logging::SystemErrorCodeToString(hr);
    return base::nullopt;
  }

  // Return early otherwise ScopedHString will create an empty string.
  if (!local_name)
    return base::nullopt;

  return base::win::ScopedHString(local_name).GetAsUTF8();
}

void ExtractAndUpdateAdvertisementData(
    IBluetoothLEAdvertisementReceivedEventArgs* received,
    BluetoothDevice* device) {
  int16_t rssi = 0;
  HRESULT hr = received->get_RawSignalStrengthInDBm(&rssi);
  if (FAILED(hr)) {
    VLOG(2) << "get_RawSignalStrengthInDBm() failed: "
            << logging::SystemErrorCodeToString(hr);
  }

  ComPtr<IBluetoothLEAdvertisement> advertisement = GetAdvertisement(received);
  static_cast<BluetoothDeviceWinrt*>(device)->UpdateLocalName(
      ExtractDeviceName(advertisement.Get()));
  device->UpdateAdvertisementData(rssi, ExtractFlags(advertisement.Get()),
                                  ExtractAdvertisedUUIDs(advertisement.Get()),
                                  ExtractTxPower(advertisement.Get()),
                                  ExtractServiceData(advertisement.Get()),
                                  ExtractManufacturerData(advertisement.Get()));
}

}  // namespace

std::string BluetoothAdapterWinrt::GetAddress() const {
  return address_;
}

std::string BluetoothAdapterWinrt::GetName() const {
  return name_;
}

void BluetoothAdapterWinrt::SetName(const std::string& name,
                                    const base::Closure& callback,
                                    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

bool BluetoothAdapterWinrt::IsInitialized() const {
  return is_initialized_;
}

bool BluetoothAdapterWinrt::IsPresent() const {
  // Obtaining the default adapter will fail if no physical adapter is present.
  // Thus a non-zero |adapter| implies that a physical adapter is present.
  return adapter_ != nullptr;
}

bool BluetoothAdapterWinrt::CanPower() const {
  return radio_ != nullptr;
}

bool BluetoothAdapterWinrt::IsPowered() const {
  // Due to an issue on WoW64 we might fail to obtain the radio in OnGetRadio().
  // This is why it can be null here.
  if (!radio_)
    return num_powered_radios_ != 0;

  RadioState state;
  HRESULT hr = radio_->get_State(&state);
  if (FAILED(hr)) {
    VLOG(2) << "Getting Radio State failed: "
            << logging::SystemErrorCodeToString(hr);
    return false;
  }

  return state == RadioState_On;
}

bool BluetoothAdapterWinrt::IsDiscoverable() const {
  NOTIMPLEMENTED();
  return false;
}

void BluetoothAdapterWinrt::SetDiscoverable(
    bool discoverable,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

bool BluetoothAdapterWinrt::IsDiscovering() const {
  return NumDiscoverySessions() > 0;
}

BluetoothAdapter::UUIDList BluetoothAdapterWinrt::GetUUIDs() const {
  NOTIMPLEMENTED();
  return UUIDList();
}

void BluetoothAdapterWinrt::CreateRfcommService(
    const BluetoothUUID& uuid,
    const ServiceOptions& options,
    const CreateServiceCallback& callback,
    const CreateServiceErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothAdapterWinrt::CreateL2capService(
    const BluetoothUUID& uuid,
    const ServiceOptions& options,
    const CreateServiceCallback& callback,
    const CreateServiceErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothAdapterWinrt::RegisterAdvertisement(
    std::unique_ptr<BluetoothAdvertisement::Data> advertisement_data,
    const CreateAdvertisementCallback& callback,
    const AdvertisementErrorCallback& error_callback) {
  auto advertisement = CreateAdvertisement();
  if (!advertisement->Initialize(std::move(advertisement_data))) {
    VLOG(2) << "Failed to Initialize Advertisement.";
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(error_callback,
                       BluetoothAdvertisement::ERROR_STARTING_ADVERTISEMENT));
    return;
  }

  // In order to avoid |advertisement| holding a strong reference to itself, we
  // pass only a weak reference to the callbacks, and store a strong reference
  // in |pending_advertisements_|. When the callbacks are run, they will remove
  // the corresponding advertisement from the list of pending advertisements.
  advertisement->Register(
      base::Bind(&BluetoothAdapterWinrt::OnRegisterAdvertisement,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Unretained(advertisement.get()), callback),
      base::Bind(&BluetoothAdapterWinrt::OnRegisterAdvertisementError,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Unretained(advertisement.get()), error_callback));

  pending_advertisements_.push_back(std::move(advertisement));
}

std::vector<BluetoothAdvertisement*>
BluetoothAdapterWinrt::GetPendingAdvertisementsForTesting() const {
  std::vector<BluetoothAdvertisement*> pending_advertisements;
  for (const auto& pending_advertisement : pending_advertisements_)
    pending_advertisements.push_back(pending_advertisement.get());
  return pending_advertisements;
}

BluetoothLocalGattService* BluetoothAdapterWinrt::GetGattService(
    const std::string& identifier) const {
  NOTIMPLEMENTED();
  return nullptr;
}

IRadio* BluetoothAdapterWinrt::GetRadioForTesting() {
  return radio_.Get();
}

IDeviceWatcher* BluetoothAdapterWinrt::GetPoweredRadioWatcherForTesting() {
  return powered_radio_watcher_.Get();
}

BluetoothAdapterWinrt::BluetoothAdapterWinrt() {
  ui_task_runner_ = base::ThreadTaskRunnerHandle::Get();
}

BluetoothAdapterWinrt::~BluetoothAdapterWinrt() {
  // Explicitly move |pending_advertisements_| into a local variable and clear
  // them out. Any remaining pending advertisement will attempt to remove itself
  // from |pending_advertisements_|, which would result in a double-free
  // otherwise.
  auto pending_advertisements = std::move(pending_advertisements_);
  pending_advertisements_.clear();

  if (radio_)
    TryRemoveRadioStateChangedHandler();

  if (powered_radio_watcher_) {
    TryRemovePoweredRadioEventHandlers();
    HRESULT hr = powered_radio_watcher_->Stop();
    if (FAILED(hr)) {
      VLOG(2) << "Stopping powered radio watcher failed: "
              << logging::SystemErrorCodeToString(hr);
    }
  }
}

BluetoothAdapterWinrt::StaticsInterfaces::StaticsInterfaces(
    ComPtr<IAgileReference> adapter_statics_in,
    ComPtr<IAgileReference> device_information_statics_in,
    ComPtr<IAgileReference> radio_statics_in)
    : adapter_statics(std::move(adapter_statics_in)),
      device_information_statics(std::move(device_information_statics_in)),
      radio_statics(std::move(radio_statics_in)) {}

BluetoothAdapterWinrt::StaticsInterfaces::StaticsInterfaces(
    const StaticsInterfaces& copy_from) = default;

BluetoothAdapterWinrt::StaticsInterfaces::StaticsInterfaces() = default;

BluetoothAdapterWinrt::StaticsInterfaces::~StaticsInterfaces() {}

void BluetoothAdapterWinrt::Init(InitCallback init_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Some of the initialization work requires loading libraries and should not
  // be run on the browser main thread.
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&BluetoothAdapterWinrt::PerformSlowInitTasks),
      base::BindOnce(&BluetoothAdapterWinrt::CompleteInitAgile,
                     weak_ptr_factory_.GetWeakPtr(), std::move(init_cb)));
}

void BluetoothAdapterWinrt::InitForTests(
    InitCallback init_cb,
    ComPtr<IBluetoothAdapterStatics> bluetooth_adapter_statics,
    ComPtr<IDeviceInformationStatics> device_information_statics,
    ComPtr<IRadioStatics> radio_statics) {
  if (!ResolveCoreWinRT()) {
    CompleteInit(std::move(init_cb), std::move(bluetooth_adapter_statics),
                 std::move(device_information_statics),
                 std::move(radio_statics));
    return;
  }

  auto statics = PerformSlowInitTasks();

  // This allows any passed in values (which would be fakes) to replace
  // the return values of PerformSlowInitTasks().
  if (!bluetooth_adapter_statics)
    statics.adapter_statics->Resolve(IID_IBluetoothAdapterStatics,
                                     &bluetooth_adapter_statics);
  if (!device_information_statics)
    statics.device_information_statics->Resolve(IID_IDeviceInformationStatics,
                                                &device_information_statics);
  if (!radio_statics)
    statics.radio_statics->Resolve(IID_IRadioStatics, &radio_statics);

  StaticsInterfaces agile_statics = GetAgileReferencesForStatics(
      std::move(bluetooth_adapter_statics),
      std::move(device_information_statics), std::move(radio_statics));
  CompleteInitAgile(std::move(init_cb), std::move(agile_statics));
}

// static
BluetoothAdapterWinrt::StaticsInterfaces
BluetoothAdapterWinrt::PerformSlowInitTasks() {
  if (!ResolveCoreWinRT())
    return BluetoothAdapterWinrt::StaticsInterfaces();

  ComPtr<IBluetoothAdapterStatics> adapter_statics;
  HRESULT hr = base::win::GetActivationFactory<
      IBluetoothAdapterStatics,
      RuntimeClass_Windows_Devices_Bluetooth_BluetoothAdapter>(
      &adapter_statics);
  if (FAILED(hr)) {
    VLOG(2) << "GetBluetoothAdapterStaticsActivationFactory failed: "
            << logging::SystemErrorCodeToString(hr);
    return BluetoothAdapterWinrt::StaticsInterfaces();
  }

  ComPtr<IDeviceInformationStatics> device_information_statics;
  hr = base::win::GetActivationFactory<
      IDeviceInformationStatics,
      RuntimeClass_Windows_Devices_Enumeration_DeviceInformation>(
      &device_information_statics);
  if (FAILED(hr)) {
    VLOG(2) << "GetDeviceInformationStaticsActivationFactory failed: "
            << logging::SystemErrorCodeToString(hr);
    return BluetoothAdapterWinrt::StaticsInterfaces();
  }

  ComPtr<IRadioStatics> radio_statics;
  hr = base::win::GetActivationFactory<
      IRadioStatics, RuntimeClass_Windows_Devices_Radios_Radio>(&radio_statics);
  if (FAILED(hr)) {
    VLOG(2) << "GetRadioStaticsActivationFactory failed: "
            << logging::SystemErrorCodeToString(hr);
    return BluetoothAdapterWinrt::StaticsInterfaces();
  }

  return GetAgileReferencesForStatics(std::move(adapter_statics),
                                      std::move(device_information_statics),
                                      std::move(radio_statics));
}

// static
BluetoothAdapterWinrt::StaticsInterfaces
BluetoothAdapterWinrt::GetAgileReferencesForStatics(
    ComPtr<IBluetoothAdapterStatics> adapter_statics,
    ComPtr<IDeviceInformationStatics> device_information_statics,
    ComPtr<IRadioStatics> radio_statics) {
  base::ScopedNativeLibrary ole32_library(base::FilePath(L"Ole32.dll"));
  CHECK(ole32_library.is_valid());

  auto ro_get_agile_reference =
      reinterpret_cast<decltype(&::RoGetAgileReference)>(
          ole32_library.GetFunctionPointer("RoGetAgileReference"));
  CHECK(ro_get_agile_reference);

  ComPtr<IAgileReference> adapter_statics_agileref;
  HRESULT hr = ro_get_agile_reference(
      AGILEREFERENCE_DEFAULT,
      ABI::Windows::Devices::Bluetooth::IID_IBluetoothAdapterStatics,
      adapter_statics.Get(), &adapter_statics_agileref);
  if (FAILED(hr))
    return StaticsInterfaces();

  ComPtr<IAgileReference> device_information_statics_agileref;
  hr = ro_get_agile_reference(
      AGILEREFERENCE_DEFAULT,
      ABI::Windows::Devices::Enumeration::IID_IDeviceInformationStatics,
      device_information_statics.Get(), &device_information_statics_agileref);
  if (FAILED(hr))
    return StaticsInterfaces();

  ComPtr<IAgileReference> radio_statics_agileref;
  hr = ro_get_agile_reference(AGILEREFERENCE_DEFAULT,
                              ABI::Windows::Devices::Radios::IID_IRadioStatics,
                              radio_statics.Get(), &radio_statics_agileref);
  if (FAILED(hr))
    return StaticsInterfaces();

  return StaticsInterfaces(std::move(adapter_statics_agileref),
                           std::move(device_information_statics_agileref),
                           std::move(radio_statics_agileref));
}

void BluetoothAdapterWinrt::CompleteInitAgile(InitCallback init_cb,
                                              StaticsInterfaces agile_statics) {
  if (!agile_statics.adapter_statics ||
      !agile_statics.device_information_statics ||
      !agile_statics.radio_statics) {
    CompleteInit(std::move(init_cb), nullptr, nullptr, nullptr);
    return;
  }
  ComPtr<IBluetoothAdapterStatics> bluetooth_adapter_statics;
  HRESULT hr = agile_statics.adapter_statics->Resolve(
      IID_IBluetoothAdapterStatics, &bluetooth_adapter_statics);
  DCHECK(SUCCEEDED(hr));
  ComPtr<IDeviceInformationStatics> device_information_statics;
  hr = agile_statics.device_information_statics->Resolve(
      IID_IDeviceInformationStatics, &device_information_statics);
  DCHECK(SUCCEEDED(hr));
  ComPtr<IRadioStatics> radio_statics;
  hr = agile_statics.radio_statics->Resolve(IID_IRadioStatics, &radio_statics);
  DCHECK(SUCCEEDED(hr));

  CompleteInit(std::move(init_cb), std::move(bluetooth_adapter_statics),
               std::move(device_information_statics), std::move(radio_statics));
}

void BluetoothAdapterWinrt::CompleteInit(
    InitCallback init_cb,
    ComPtr<IBluetoothAdapterStatics> bluetooth_adapter_statics,
    ComPtr<IDeviceInformationStatics> device_information_statics,
    ComPtr<IRadioStatics> radio_statics) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // We are wrapping |init_cb| in a ScopedClosureRunner to ensure it gets run
  // no matter how the function exits. Furthermore, we set |is_initialized_|
  // to true if adapter is still active when the callback gets run.
  base::ScopedClosureRunner on_init(base::BindOnce(
      [](base::WeakPtr<BluetoothAdapterWinrt> adapter, InitCallback init_cb) {
        if (adapter)
          adapter->is_initialized_ = true;
        std::move(init_cb).Run();
      },
      weak_ptr_factory_.GetWeakPtr(), std::move(init_cb)));

  bluetooth_adapter_statics_ = bluetooth_adapter_statics;
  device_information_statics_ = device_information_statics;
  radio_statics_ = radio_statics;

  if (!bluetooth_adapter_statics_ || !device_information_statics_ ||
      !radio_statics_) {
    return;
  }

  ComPtr<IAsyncOperation<uwp::BluetoothAdapter*>> get_default_adapter_op;
  HRESULT hr =
      bluetooth_adapter_statics_->GetDefaultAsync(&get_default_adapter_op);
  if (FAILED(hr)) {
    VLOG(2) << "BluetoothAdapter::GetDefaultAsync failed: "
            << logging::SystemErrorCodeToString(hr);
    return;
  }

  hr = base::win::PostAsyncResults(
      std::move(get_default_adapter_op),
      base::BindOnce(&BluetoothAdapterWinrt::OnGetDefaultAdapter,
                     weak_ptr_factory_.GetWeakPtr(), std::move(on_init)));

  if (FAILED(hr)) {
    VLOG(2) << "PostAsyncResults failed: "
            << logging::SystemErrorCodeToString(hr);
  }
}

base::WeakPtr<BluetoothAdapter> BluetoothAdapterWinrt::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool BluetoothAdapterWinrt::SetPoweredImpl(bool powered) {
  // Due to an issue on WoW64 we might fail to obtain the radio in
  // OnGetRadio(). This is why it can be null here.
  if (!radio_)
    return false;

  const RadioState state = powered ? RadioState_On : RadioState_Off;
  ComPtr<IAsyncOperation<RadioAccessStatus>> set_state_op;
  HRESULT hr = radio_->SetStateAsync(state, &set_state_op);
  if (FAILED(hr)) {
    VLOG(2) << "Radio::SetStateAsync failed: "
            << logging::SystemErrorCodeToString(hr);
    return false;
  }

  hr = base::win::PostAsyncResults(
      std::move(set_state_op),
      base::BindOnce(&BluetoothAdapterWinrt::OnSetRadioState,
                     weak_ptr_factory_.GetWeakPtr()));

  if (FAILED(hr)) {
    VLOG(2) << "PostAsyncResults failed: "
            << logging::SystemErrorCodeToString(hr);
    return false;
  }

  return true;
}

void BluetoothAdapterWinrt::UpdateFilter(
    std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter,
    DiscoverySessionResultCallback callback) {
  ui_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), /*is_error=*/false,
                                UMABluetoothDiscoverySessionOutcome::SUCCESS));
}

void BluetoothAdapterWinrt::StartScanWithFilter(
    std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter,
    DiscoverySessionResultCallback callback) {
  // should only have 1 discovery session since we are just starting the scan
  // now
  DCHECK_EQ(NumDiscoverySessions(), 1);

  HRESULT hr = ActivateBluetoothAdvertisementLEWatcherInstance(
      &ble_advertisement_watcher_);
  if (FAILED(hr)) {
    VLOG(2) << "ActivateBluetoothAdvertisementLEWatcherInstance failed: "
            << logging::SystemErrorCodeToString(hr);
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), /*is_error=*/true,
                       UMABluetoothDiscoverySessionOutcome::UNKNOWN));
    return;
  }

  hr = ble_advertisement_watcher_->put_ScanningMode(
      BluetoothLEScanningMode_Active);
  if (FAILED(hr)) {
    VLOG(2) << "Setting ScanningMode to Active failed: "
            << logging::SystemErrorCodeToString(hr);
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), /*is_error=*/true,
                       UMABluetoothDiscoverySessionOutcome::UNKNOWN));
    return;
  }

  auto advertisement_received_token = AddTypedEventHandler(
      ble_advertisement_watcher_.Get(),
      &IBluetoothLEAdvertisementWatcher::add_Received,
      base::BindRepeating(&BluetoothAdapterWinrt::OnAdvertisementReceived,
                          weak_ptr_factory_.GetWeakPtr()));
  if (!advertisement_received_token) {
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), /*is_error=*/true,
                       UMABluetoothDiscoverySessionOutcome::UNKNOWN));
    return;
  }

  advertisement_received_token_ = *advertisement_received_token;

  hr = ble_advertisement_watcher_->Start();
  if (FAILED(hr)) {
    VLOG(2) << "Starting the Advertisement Watcher failed: "
            << logging::SystemErrorCodeToString(hr);
    RemoveAdvertisementReceivedHandler();
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), /*is_error=*/true,
                       UMABluetoothDiscoverySessionOutcome::UNKNOWN));
    return;
  }

  BluetoothLEAdvertisementWatcherStatus watcher_status;
  hr = ble_advertisement_watcher_->get_Status(&watcher_status);
  if (FAILED(hr)) {
    VLOG(2) << "Getting the Watcher Status failed: "
            << logging::SystemErrorCodeToString(hr);
  } else if (watcher_status == BluetoothLEAdvertisementWatcherStatus_Aborted) {
    VLOG(2) << "Starting Advertisement Watcher failed, it is in the Aborted "
               "state.";
    RemoveAdvertisementReceivedHandler();
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), /*is_error=*/true,
                       UMABluetoothDiscoverySessionOutcome::UNKNOWN));
    return;
  }

  ui_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), false,
                                UMABluetoothDiscoverySessionOutcome::SUCCESS));
}

void BluetoothAdapterWinrt::StopScan(DiscoverySessionResultCallback callback) {
  DCHECK_EQ(NumDiscoverySessions(), 0);

  RemoveAdvertisementReceivedHandler();
  HRESULT hr = ble_advertisement_watcher_->Stop();
  if (FAILED(hr)) {
    VLOG(2) << "Stopped the Advertisement Watcher failed: "
            << logging::SystemErrorCodeToString(hr);
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), /*is_error=*/true,
                       UMABluetoothDiscoverySessionOutcome::UNKNOWN));
    return;
  }

  for (auto& device : devices_)
    device.second->ClearAdvertisementData();
  ble_advertisement_watcher_.Reset();
  ui_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), /*is_error=*/false,
                                UMABluetoothDiscoverySessionOutcome::SUCCESS));
}

void BluetoothAdapterWinrt::RemovePairingDelegateInternal(
    BluetoothDevice::PairingDelegate* pairing_delegate) {
  NOTIMPLEMENTED();
}

HRESULT
BluetoothAdapterWinrt::ActivateBluetoothAdvertisementLEWatcherInstance(
    IBluetoothLEAdvertisementWatcher** instance) const {
  auto watcher_hstring = base::win::ScopedHString::Create(
      RuntimeClass_Windows_Devices_Bluetooth_Advertisement_BluetoothLEAdvertisementWatcher);
  if (!watcher_hstring.is_valid())
    return E_FAIL;

  ComPtr<IInspectable> inspectable;
  HRESULT hr =
      base::win::RoActivateInstance(watcher_hstring.get(), &inspectable);
  if (FAILED(hr)) {
    VLOG(2) << "RoActivateInstance failed: "
            << logging::SystemErrorCodeToString(hr);
    return hr;
  }

  ComPtr<IBluetoothLEAdvertisementWatcher> watcher;
  hr = inspectable.As(&watcher);
  if (FAILED(hr)) {
    VLOG(2) << "As IBluetoothLEAdvertisementWatcher failed: "
            << logging::SystemErrorCodeToString(hr);
    return hr;
  }

  return watcher.CopyTo(instance);
}

scoped_refptr<BluetoothAdvertisementWinrt>
BluetoothAdapterWinrt::CreateAdvertisement() const {
  return base::MakeRefCounted<BluetoothAdvertisementWinrt>();
}

std::unique_ptr<BluetoothDeviceWinrt> BluetoothAdapterWinrt::CreateDevice(
    uint64_t raw_address) {
  return std::make_unique<BluetoothDeviceWinrt>(this, raw_address);
}

void BluetoothAdapterWinrt::OnGetDefaultAdapter(
    base::ScopedClosureRunner on_init,
    ComPtr<IBluetoothAdapter> adapter) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!adapter) {
    VLOG(2) << "Getting Default Adapter failed.";
    return;
  }

  adapter_ = std::move(adapter);
  uint64_t raw_address;
  HRESULT hr = adapter_->get_BluetoothAddress(&raw_address);
  if (FAILED(hr)) {
    VLOG(2) << "Getting BluetoothAddress failed: "
            << logging::SystemErrorCodeToString(hr);
    return;
  }

  address_ = BluetoothDevice::CanonicalizeAddress(
      base::StringPrintf("%012llX", raw_address));
  DCHECK(!address_.empty());

  HSTRING device_id;
  hr = adapter_->get_DeviceId(&device_id);
  if (FAILED(hr)) {
    VLOG(2) << "Getting DeviceId failed: "
            << logging::SystemErrorCodeToString(hr);
    return;
  }

  ComPtr<IAsyncOperation<DeviceInformation*>> create_from_id_op;
  hr = device_information_statics_->CreateFromIdAsync(device_id,
                                                      &create_from_id_op);
  if (FAILED(hr)) {
    VLOG(2) << "CreateFromIdAsync failed: "
            << logging::SystemErrorCodeToString(hr);
    return;
  }

  hr = base::win::PostAsyncResults(
      std::move(create_from_id_op),
      base::BindOnce(&BluetoothAdapterWinrt::OnCreateFromIdAsync,
                     weak_ptr_factory_.GetWeakPtr(), std::move(on_init)));
  if (FAILED(hr)) {
    VLOG(2) << "PostAsyncResults failed: "
            << logging::SystemErrorCodeToString(hr);
  }
}

void BluetoothAdapterWinrt::OnCreateFromIdAsync(
    base::ScopedClosureRunner on_init,
    ComPtr<IDeviceInformation> device_information) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!device_information) {
    VLOG(2) << "Getting Device Information failed.";
    return;
  }

  HSTRING name;
  HRESULT hr = device_information->get_Name(&name);
  if (FAILED(hr)) {
    VLOG(2) << "Getting Name failed: " << logging::SystemErrorCodeToString(hr);
    return;
  }

  name_ = base::win::ScopedHString(name).GetAsUTF8();

  ComPtr<IAsyncOperation<RadioAccessStatus>> request_access_op;
  hr = radio_statics_->RequestAccessAsync(&request_access_op);
  if (FAILED(hr)) {
    VLOG(2) << "RequestAccessAsync failed: "
            << logging::SystemErrorCodeToString(hr);
    return;
  }

  hr = base::win::PostAsyncResults(
      std::move(request_access_op),
      base::BindOnce(&BluetoothAdapterWinrt::OnRequestRadioAccess,
                     weak_ptr_factory_.GetWeakPtr(), std::move(on_init)));

  if (FAILED(hr)) {
    VLOG(2) << "PostAsyncResults failed: "
            << logging::SystemErrorCodeToString(hr);
  }
}

void BluetoothAdapterWinrt::OnRequestRadioAccess(
    base::ScopedClosureRunner on_init,
    RadioAccessStatus access_status) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (access_status != RadioAccessStatus_Allowed) {
    VLOG(2) << "Got unexpected Radio Access Status: "
            << ToCString(access_status);
    return;
  }

  ComPtr<IAsyncOperation<Radio*>> get_radio_op;
  HRESULT hr = adapter_->GetRadioAsync(&get_radio_op);
  if (FAILED(hr)) {
    VLOG(2) << "GetRadioAsync failed: " << logging::SystemErrorCodeToString(hr);
    return;
  }

  hr = base::win::PostAsyncResults(
      std::move(get_radio_op),
      base::BindOnce(&BluetoothAdapterWinrt::OnGetRadio,
                     weak_ptr_factory_.GetWeakPtr(), std::move(on_init)));

  if (FAILED(hr)) {
    VLOG(2) << "PostAsyncResults failed: "
            << logging::SystemErrorCodeToString(hr);
  }
}

void BluetoothAdapterWinrt::OnGetRadio(base::ScopedClosureRunner on_init,
                                       ComPtr<IRadio> radio) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (radio) {
    radio_ = std::move(radio);
    radio_state_changed_token_ = AddTypedEventHandler(
        radio_.Get(), &IRadio::add_StateChanged,
        base::BindRepeating(&BluetoothAdapterWinrt::OnRadioStateChanged,
                            weak_ptr_factory_.GetWeakPtr()));

    if (!radio_state_changed_token_)
      VLOG(2) << "Adding Radio State Changed Handler failed.";
    return;
  }

  // This happens within WoW64, due to an issue with non-native APIs.
  VLOG(2) << "Getting Radio failed. Chrome will be unable to change the power "
             "state by itself.";

  // Attempt to create a DeviceWatcher for powered radios, so that querying
  // the power state is still possible.
  auto aqs_filter = base::win::ScopedHString::Create(kPoweredRadiosAqsFilter);
  HRESULT hr = device_information_statics_->CreateWatcherAqsFilter(
      aqs_filter.get(), &powered_radio_watcher_);
  if (FAILED(hr)) {
    VLOG(2) << "Creating Powered Radios Watcher failed: "
            << logging::SystemErrorCodeToString(hr);
    return;
  }

  powered_radio_added_token_ = AddTypedEventHandler(
      powered_radio_watcher_.Get(), &IDeviceWatcher::add_Added,
      base::BindRepeating(&BluetoothAdapterWinrt::OnPoweredRadioAdded,
                          weak_ptr_factory_.GetWeakPtr()));

  powered_radio_removed_token_ = AddTypedEventHandler(
      powered_radio_watcher_.Get(), &IDeviceWatcher::add_Removed,
      base::BindRepeating(&BluetoothAdapterWinrt::OnPoweredRadioRemoved,
                          weak_ptr_factory_.GetWeakPtr()));

  powered_radios_enumerated_token_ = AddTypedEventHandler(
      powered_radio_watcher_.Get(), &IDeviceWatcher::add_EnumerationCompleted,
      base::BindRepeating(&BluetoothAdapterWinrt::OnPoweredRadiosEnumerated,
                          weak_ptr_factory_.GetWeakPtr()));

  if (!powered_radio_added_token_ || !powered_radio_removed_token_ ||
      !powered_radios_enumerated_token_) {
    VLOG(2) << "Failed to Register Powered Radio Event Handlers.";
    TryRemovePoweredRadioEventHandlers();
    return;
  }

  hr = powered_radio_watcher_->Start();
  if (FAILED(hr)) {
    VLOG(2) << "Starting the Powered Radio Watcher failed: "
            << logging::SystemErrorCodeToString(hr);
    TryRemovePoweredRadioEventHandlers();
    return;
  }

  // Store the Closure Runner. It is expected that OnPoweredRadiosEnumerated()
  // is invoked soon after.
  on_init_ = std::make_unique<base::ScopedClosureRunner>(std::move(on_init));
}

void BluetoothAdapterWinrt::OnSetRadioState(RadioAccessStatus access_status) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (access_status != RadioAccessStatus_Allowed) {
    VLOG(2) << "Got unexpected Radio Access Status: "
            << ToCString(access_status);
    RunPendingPowerCallbacks();
  }
}

void BluetoothAdapterWinrt::OnRadioStateChanged(IRadio* radio,
                                                IInspectable* object) {
  RunPendingPowerCallbacks();
  NotifyAdapterPoweredChanged(IsPowered());
}

void BluetoothAdapterWinrt::OnPoweredRadioAdded(IDeviceWatcher* watcher,
                                                IDeviceInformation* info) {
  if (++num_powered_radios_ == 1)
    NotifyAdapterPoweredChanged(true);
  VLOG(2) << "OnPoweredRadioAdded(), Number of Powered Radios: "
          << num_powered_radios_;
}

void BluetoothAdapterWinrt::OnPoweredRadioRemoved(
    IDeviceWatcher* watcher,
    IDeviceInformationUpdate* update) {
  if (--num_powered_radios_ == 0)
    NotifyAdapterPoweredChanged(false);
  VLOG(2) << "OnPoweredRadioRemoved(), Number of Powered Radios: "
          << num_powered_radios_;
}

void BluetoothAdapterWinrt::OnPoweredRadiosEnumerated(IDeviceWatcher* watcher,
                                                      IInspectable* object) {
  // Destroy the ScopedClosureRunner, triggering the contained Closure to be
  // run.
  DCHECK(on_init_);
  on_init_.reset();
  VLOG(2) << "OnPoweredRadiosEnumerated(), Number of Powered Radios: "
          << num_powered_radios_;
}

void BluetoothAdapterWinrt::OnAdvertisementReceived(
    IBluetoothLEAdvertisementWatcher* watcher,
    IBluetoothLEAdvertisementReceivedEventArgs* received) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  uint64_t raw_bluetooth_address;
  HRESULT hr = received->get_BluetoothAddress(&raw_bluetooth_address);
  if (FAILED(hr)) {
    VLOG(2) << "get_BluetoothAddress() failed: "
            << logging::SystemErrorCodeToString(hr);
    return;
  }

  std::string bluetooth_address =
      BluetoothDeviceWinrt::CanonicalizeAddress(raw_bluetooth_address);
  auto it = devices_.find(bluetooth_address);
  const bool is_new_device = (it == devices_.end());
  if (is_new_device) {
    bool was_inserted = false;
    std::tie(it, was_inserted) = devices_.emplace(
        std::move(bluetooth_address), CreateDevice(raw_bluetooth_address));
    DCHECK(was_inserted);
  }

  BluetoothDevice* const device = it->second.get();
  ExtractAndUpdateAdvertisementData(received, device);

  for (auto& observer : observers_) {
    is_new_device ? observer.DeviceAdded(this, device)
                  : observer.DeviceChanged(this, device);
  }
}

void BluetoothAdapterWinrt::OnRegisterAdvertisement(
    BluetoothAdvertisement* advertisement,
    const CreateAdvertisementCallback& callback) {
  DCHECK(base::Contains(pending_advertisements_, advertisement));
  auto wrapped_advertisement = base::WrapRefCounted(advertisement);
  base::Erase(pending_advertisements_, advertisement);
  callback.Run(std::move(wrapped_advertisement));
}

void BluetoothAdapterWinrt::OnRegisterAdvertisementError(
    BluetoothAdvertisement* advertisement,
    const AdvertisementErrorCallback& error_callback,
    BluetoothAdvertisement::ErrorCode error_code) {
  // Note: We are not DCHECKing that |pending_advertisements_| contains
  // |advertisement|, as this method might be invoked during destruction.
  base::Erase(pending_advertisements_, advertisement);
  error_callback.Run(error_code);
}

void BluetoothAdapterWinrt::TryRemoveRadioStateChangedHandler() {
  DCHECK(radio_);
  if (!radio_state_changed_token_)
    return;

  HRESULT hr = radio_->remove_StateChanged(*radio_state_changed_token_);
  if (FAILED(hr)) {
    VLOG(2) << "Removing Radio State Changed Handler failed: "
            << logging::SystemErrorCodeToString(hr);
  }

  radio_state_changed_token_.reset();
}

void BluetoothAdapterWinrt::TryRemovePoweredRadioEventHandlers() {
  DCHECK(powered_radio_watcher_);
  if (powered_radio_added_token_) {
    HRESULT hr =
        powered_radio_watcher_->remove_Added(*powered_radio_removed_token_);
    if (FAILED(hr)) {
      VLOG(2) << "Removing the Powered Radio Added Handler failed: "
              << logging::SystemErrorCodeToString(hr);
    }

    powered_radio_added_token_.reset();
  }

  if (powered_radio_removed_token_) {
    HRESULT hr =
        powered_radio_watcher_->remove_Removed(*powered_radio_removed_token_);
    if (FAILED(hr)) {
      VLOG(2) << "Removing the Powered Radio Removed Handler failed: "
              << logging::SystemErrorCodeToString(hr);
    }

    powered_radio_removed_token_.reset();
  }

  if (powered_radios_enumerated_token_) {
    HRESULT hr = powered_radio_watcher_->remove_EnumerationCompleted(
        *powered_radios_enumerated_token_);
    if (FAILED(hr)) {
      VLOG(2) << "Removing the Powered Radios Enumerated Handler failed: "
              << logging::SystemErrorCodeToString(hr);
    }

    powered_radios_enumerated_token_.reset();
  }
}

void BluetoothAdapterWinrt::RemoveAdvertisementReceivedHandler() {
  DCHECK(ble_advertisement_watcher_);
  HRESULT hr = ble_advertisement_watcher_->remove_Received(
      advertisement_received_token_);
  if (FAILED(hr)) {
    VLOG(2) << "Removing the Received Handler failed: "
            << logging::SystemErrorCodeToString(hr);
  }
}

}  // namespace device
