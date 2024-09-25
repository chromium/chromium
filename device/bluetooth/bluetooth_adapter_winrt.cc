// Copyright 2018 The Chromium Authors
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

#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_native_library.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/win/com_init_util.h"
#include "base/win/core_winrt_util.h"
#include "base/win/post_async_results.h"
#include "components/device_event_log/device_event_log.h"
#include "device/bluetooth/bluetooth_advertisement_winrt.h"
#include "device/bluetooth/bluetooth_device_winrt.h"
#include "device/bluetooth/bluetooth_discovery_filter.h"
#include "device/bluetooth/bluetooth_discovery_session_outcome.h"
#include "device/bluetooth/event_utils_winrt.h"
#include "device/bluetooth/public/cpp/bluetooth_address.h"

namespace device {

namespace {

// In order to avoid a name clash with device::BluetoothAdapter we need this
// auxiliary namespace.
namespace uwp {
using ABI::Windows::Devices::Bluetooth::BluetoothAdapter;
}  // namespace uwp
using ABI::Windows::Devices::Bluetooth::BluetoothError;
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
using ABI::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEScanningMode_Active;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    IBluetoothLEAdvertisement;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    IBluetoothLEAdvertisementDataSection;
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
using ABI::Windows::Devices::Radios::RadioState_Unknown;
using ABI::Windows::Foundation::IAsyncOperation;
using ABI::Windows::Foundation::IReference;
using ABI::Windows::Foundation::Collections::IVector;
using ABI::Windows::Foundation::Collections::IVectorView;
using ABI::Windows::Storage::Streams::IBuffer;
using ABI::Windows::Storage::Streams::IDataReader;
using ABI::Windows::Storage::Streams::IDataReaderStatics;
using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

// Query string for powered Bluetooth radios. GUID Reference:
// https://docs.microsoft.com/en-us/windows-hardware/drivers/install/guid-bthport-device-interface
// TODO(crbug.com/40567018): Consider adding WindowsCreateStringReference
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
}

template <typename VectorView, typename T>
bool ToStdVector(VectorView* view, std::vector<T>* vector) {
  unsigned size;
  HRESULT hr = view->get_Size(&size);
  if (FAILED(hr)) {
    BLUETOOTH_LOG(ERROR) << "get_Size() failed: "
                         << logging::SystemErrorCodeToString(hr);
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

std::optional<std::vector<uint8_t>> ExtractVector(IBuffer* buffer) {
  ComPtr<IDataReaderStatics> data_reader_statics;
  HRESULT hr = base::win::GetActivationFactory<
      IDataReaderStatics, RuntimeClass_Windows_Storage_Streams_DataReader>(
      &data_reader_statics);
  if (FAILED(hr)) {
    BLUETOOTH_LOG(ERROR)
        << "Getting DataReaderStatics Activation Factory failed: "
        << logging::SystemErrorCodeToString(hr);
    return std::nullopt;
  }

  ComPtr<IDataReader> data_reader;
  hr = data_reader_statics->FromBuffer(buffer, &data_reader);
  if (FAILED(hr)) {
    BLUETOOTH_LOG(ERROR) << "FromBuffer() failed: "
                         << logging::SystemErrorCodeToString(hr);
    return std::nullopt;
  }

  uint32_t buffer_length;
  hr = buffer->get_Length(&buffer_length);
  if (FAILED(hr)) {
    BLUETOOTH_LOG(ERROR) << "get_Length() failed: "
                         << logging::SystemErrorCodeToString(hr);
    return std::nullopt;
  }

  std::vector<uint8_t> bytes(buffer_length);
  hr = data_reader->ReadBytes(buffer_length, bytes.data());
  if (FAILED(hr)) {
    BLUETOOTH_LOG(ERROR) << "ReadBytes() failed: "
                         << logging::SystemErrorCodeToString(hr);
    return std::nullopt;
  }

  return bytes;
}

std::optional<uint8_t> ExtractFlags(IBluetoothLEAdvertisement* advertisement) {
  if (!advertisement)
    return std::nullopt;

  ComPtr<IReference<BluetoothLEAdvertisementFlags>> flags_ref;
  HRESULT hr = advertisement->get_Flags(&flags_ref);
  if (FAILED(hr)) {
    BLUETOOTH_LOG(ERROR) << "get_Flags() failed: "
                         << logging::SystemErrorCodeToString(hr);
    return std::nullopt;
  }

  if (!flags_ref) {
    BLUETOOTH_LOG(DEBUG) << "No advertisement flags found.";
    return std::nullopt;
  }

  BluetoothLEAdvertisementFlags flags;
  hr = flags_ref->get_Value(&flags);
  if (FAILED(hr)) {
    BLUETOOTH_LOG(ERROR) << "get_Value() failed: "
                         << logging::SystemErrorCodeToString(hr);
    return std::nullopt;
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
    BLUETOOTH_LOG(ERROR) << "get_ServiceUuids() failed: "
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
      BLUETOOTH_LOG(ERROR) << "get_Data() failed: "
                           << logging::SystemErrorCodeToString(hr);
      continue;
    }

    auto bytes = ExtractVector(buffer.Get());
    if (!bytes)
      continue;

    auto bytes_span = base::make_span(*bytes);
    if (bytes_span.size() < num_bytes_uuid) {
      BLUETOOTH_LOG(ERROR) << "Buffer Length is too small: "
                           << bytes_span.size() << " vs. " << num_bytes_uuid;
      continue;
    }

    auto uuid_span = bytes_span.first(num_bytes_uuid);
    // The UUID is specified in little endian format, thus we reverse the bytes
    // here.
    std::vector<uint8_t> uuid_bytes(uuid_span.rbegin(), uuid_span.rend());

    // HexEncode the bytes and add dashes as required.
    std::string uuid_str;
    for (char c : base::HexEncode(uuid_bytes)) {
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
      BLUETOOTH_LOG(ERROR) << "GetSectionsByType() failed: "
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
    BLUETOOTH_LOG(ERROR) << "GetManufacturerData() failed: "
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
      BLUETOOTH_LOG(ERROR) << "get_CompanyId() failed: "
                           << logging::SystemErrorCodeToString(hr);
      continue;
    }

    ComPtr<IBuffer> buffer;
    hr = manufacturer_datum->get_Data(&buffer);
    if (FAILED(hr)) {
      BLUETOOTH_LOG(ERROR) << "get_Data() failed: "
                           << logging::SystemErrorCodeToString(hr);
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
std::optional<int8_t> ExtractTxPower(IBluetoothLEAdvertisement* advertisement) {
  if (!advertisement)
    return std::nullopt;

  ComPtr<IVectorView<BluetoothLEAdvertisementDataSection*>> data_sections;
  HRESULT hr = advertisement->GetSectionsByType(
      BluetoothDeviceWinrt::kTxPowerLevelDataSection, &data_sections);
  if (FAILED(hr)) {
    BLUETOOTH_LOG(ERROR) << "GetSectionsByType() failed: "
                         << logging::SystemErrorCodeToString(hr);
    return std::nullopt;
  }

  std::vector<ComPtr<IBluetoothLEAdvertisementDataSection>> vector;
  if (!ToStdVector(data_sections.Get(), &vector) || vector.empty())
    return std::nullopt;

  if (vector.size() != 1u) {
    BLUETOOTH_LOG(ERROR) << "Unexpected number of data sections: "
                         << vector.size();
    return std::nullopt;
  }

  ComPtr<IBuffer> buffer;
  hr = vector.front()->get_Data(&buffer);
  if (FAILED(hr)) {
    BLUETOOTH_LOG(ERROR) << "get_Data() failed: "
                         << logging::SystemErrorCodeToString(hr);
    return std::nullopt;
  }

  auto bytes = ExtractVector(buffer.Get());
  if (!bytes)
    return std::nullopt;

  if (bytes->size() != 1) {
    BLUETOOTH_LOG(ERROR) << "Unexpected number of bytes: " << bytes->size();
    return std::nullopt;
  }

  return bytes->front();
}

ComPtr<IBluetoothLEAdvertisement> GetAdvertisement(
    IBluetoothLEAdvertisementReceivedEventArgs* received) {
  ComPtr<IBluetoothLEAdvertisement> advertisement;
  HRESULT hr = received->get_Advertisement(&advertisement);
  if (FAILED(hr)) {
    BLUETOOTH_LOG(ERROR) << "get_Advertisement() failed: "
                         << logging::SystemErrorCodeToString(hr);
  }

  return advertisement;
}

std::optional<std::string> ExtractDeviceName(
    IBluetoothLEAdvertisement* advertisement) {
  if (!advertisement)
    return std::nullopt;

  HSTRING local_name;
  HRESULT hr = advertisement->get_LocalName(&local_name);
  if (FAILED(hr)) {
    BLUETOOTH_LOG(ERROR) << "Getting Local Name failed: "
                         << logging::SystemErrorCodeToString(hr);
    return std::nullopt;
  }

  // Return early otherwise ScopedHString will create an empty string.
  if (!local_name)
    return std::nullopt;

  return base::win::ScopedHString(local_name).GetAsUTF8();
}

RadioState GetState(IRadio* radio) {
  RadioState state;
  HRESULT hr = radio->get_State(&state);
  if (FAILED(hr)) {
    BLUETOOTH_LOG(ERROR) << "Getting Radio State failed: "
                         << logging::SystemErrorCodeToString(hr);
    return RadioState_Unknown;
  }
  return state;
}

}  // namespace

std::string BluetoothAdapterWinrt::GetAddress() const {
  return address_;
}

std::string BluetoothAdapterWinrt::GetName() const {
  return name_;
}

void BluetoothAdapterWinrt::SetName(const std::string& name,
                                    base::OnceClosure callback,
                                    ErrorCallback error_callback) {
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
  return radio_ != nullptr && radio_access_allowed_;
}

bool BluetoothAdapterWinrt::IsPowered() const {
  // Due to an issue on WoW64 we might fail to obtain the radio in OnGetRadio().
  // This is why it can be null here.
  if (!radio_)
    return num_powered_radios_ != 0;

  return GetState(radio_.Get()) == RadioState_On;
}

bool BluetoothAdapterWinrt::IsPeripheralRoleSupported() const {
  if (!adapter_) {
    return false;
  }
  boolean supported = false;
  HRESULT hr = adapter_->get_IsPeripheralRoleSupported(&supported);
  if (FAILED(hr)) {
    BLUETOOTH_LOG(ERROR) << "Getting IsPeripheralRoleSupported failed: "
                         << logging::SystemErrorCodeToString(hr);
  }
  return supported;
}

bool BluetoothAdapterWinrt::IsDiscoverable() const {
  NOTIMPLEMENTED();
  return false;
}

void BluetoothAdapterWinrt::SetDiscoverable(bool discoverable,
                                            base::OnceClosure callback,
                                            ErrorCallback error_callback) {
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
    CreateServiceCallback callback,
    CreateServiceErrorCallback error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothAdapterWinrt::CreateL2capService(
    const BluetoothUUID& uuid,
    const ServiceOptions& options,
    CreateServiceCallback callback,
    CreateServiceErrorCallback error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothAdapterWinrt::RegisterAdvertisement(
    std::unique_ptr<BluetoothAdvertisement::Data> advertisement_data,
    CreateAdvertisementCallback callback,
    AdvertisementErrorCallback error_callback) {
  auto advertisement = CreateAdvertisement();
  if (!advertisement->Initialize(std::move(advertisement_data))) {
    BLUETOOTH_LOG(ERROR) << "Failed to Initialize Advertisement.";
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(error_callback),
                       BluetoothAdvertisement::ERROR_STARTING_ADVERTISEMENT));
    return;
  }

  // In order to avoid |advertisement| holding a strong reference to itself, we
  // pass only a weak reference to the callbacks, and store a strong reference
  // in |pending_advertisements_|. When the callbacks are run, they will remove
  // the corresponding advertisement from the list of pending advertisements.
  advertisement->Register(
      base::BindOnce(&BluetoothAdapterWinrt::OnRegisterAdvertisement,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::Unretained(advertisement.get()),
                     std::move(callback)),
      base::BindOnce(&BluetoothAdapterWinrt::OnRegisterAdvertisementError,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::Unretained(advertisement.get()),
                     std::move(error_callback)));

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
  ui_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
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
      BLUETOOTH_LOG(ERROR) << "Stopping powered radio watcher failed: "
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

void BluetoothAdapterWinrt::Initialize(base::OnceClosure init_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Some of the initialization work requires loading libraries and should not
  // be run on the browser main thread.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::ThreadPolicy::MUST_USE_FOREGROUND},
      base::BindOnce(&BluetoothAdapterWinrt::PerformSlowInitTasks),
      base::BindOnce(&BluetoothAdapterWinrt::CompleteInitAgile,
                     weak_ptr_factory_.GetWeakPtr(), std::move(init_callback)));
}

void BluetoothAdapterWinrt::InitForTests(
    base::OnceClosure init_callback,
    ComPtr<IBluetoothAdapterStatics> bluetooth_adapter_statics,
    ComPtr<IDeviceInformationStatics> device_information_statics,
    ComPtr<IRadioStatics> radio_statics) {
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
  CompleteInitAgile(std::move(init_callback), std::move(agile_statics));
}

// static
BluetoothAdapterWinrt::StaticsInterfaces
BluetoothAdapterWinrt::PerformSlowInitTasks() {
  base::win::AssertComApartmentType(base::win::ComApartmentType::MTA);
  ComPtr<IBluetoothAdapterStatics> adapter_statics;
  HRESULT hr = base::win::GetActivationFactory<
      IBluetoothAdapterStatics,
      RuntimeClass_Windows_Devices_Bluetooth_BluetoothAdapter>(
      &adapter_statics);
  if (FAILED(hr)) {
    BLUETOOTH_LOG(ERROR)
        << "GetBluetoothAdapterStaticsActivationFactory failed: "
        << logging::SystemErrorCodeToString(hr);
    return BluetoothAdapterWinrt::StaticsInterfaces();
  }

  ComPtr<IDeviceInformationStatics> device_information_statics;
  hr = base::win::GetActivationFactory<
      IDeviceInformationStatics,
      RuntimeClass_Windows_Devices_Enumeration_DeviceInformation>(
      &device_information_statics);
  if (FAILED(hr)) {
    BLUETOOTH_LOG(ERROR)
        << "GetDeviceInformationStaticsActivationFactory failed: "
        << logging::SystemErrorCodeToString(hr);
    return BluetoothAdapterWinrt::StaticsInterfaces();
  }

  ComPtr<IRadioStatics> radio_statics;
  hr = base::win::GetActivationFactory<
      IRadioStatics, RuntimeClass_Windows_Devices_Radios_Radio>(&radio_statics);
  if (FAILED(hr)) {
    BLUETOOTH_LOG(ERROR) << "GetRadioStaticsActivationFactory failed: "
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

void BluetoothAdapterWinrt::CompleteInitAgile(base::OnceClosure init_callback,
                                              StaticsInterfaces agile_statics) {
  if (!agile_statics.adapter_statics ||
      !agile_statics.device_information_statics ||
      !agile_statics.radio_statics) {
    CompleteInit(std::move(init_callback), nullptr, nullptr, nullptr);
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

  CompleteInit(std::move(init_callback), std::move(bluetooth_adapter_statics),
               std::move(device_information_statics), std::move(radio_statics));
}

void BluetoothAdapterWinrt::CompleteInit(
    base::OnceClosure init_callback,
    ComPtr<IBluetoothAdapterStatics> bluetooth_adapter_statics,
    ComPtr<IDeviceInformationStatics> device_information_statics,
    ComPtr<IRadioStatics> radio_statics) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // We are wrapping |init_callback| in a ScopedClosureRunner to ensure it gets
  // run no matter how the function exits. Furthermore, we set |is_initialized_|
  // to true if adapter is still active when the callback gets run.
  base::ScopedClosureRunner on_init(base::BindOnce(
      [](base::WeakPtr<BluetoothAdapterWinrt> adapter,
         base::OnceClosure init_callback) {
        if (adapter)
          adapter->is_initialized_ = true;
        std::move(init_callback).Run();
      },
      weak_ptr_factory_.GetWeakPtr(), std::move(init_callback)));

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
    BLUETOOTH_LOG(ERROR) << "BluetoothAdapter::GetDefaultAsync failed: "
                         << logging::SystemErrorCodeToString(hr);
    return;
  }

  hr = base::win::PostAsyncResults(
      std::move(get_default_adapter_op),
      base::BindOnce(&BluetoothAdapterWinrt::OnGetDefaultAdapter,
                     weak_ptr_factory_.GetWeakPtr(), std::move(on_init)));

  if (FAILED(hr)) {
    BLUETOOTH_LOG(ERROR) << "PostAsyncResults failed: "
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
    BLUETOOTH_LOG(ERROR) << "Radio::SetStateAsync failed: "
                         << logging::SystemErrorCodeToString(hr);
    return false;
  }

  hr = base::win::PostAsyncResults(
      std::move(set_state_op),
      base::BindOnce(&BluetoothAdapterWinrt::OnSetRadioState,
                     weak_ptr_factory_.GetWeakPtr()));

  if (FAILED(hr)) {
    BLUETOOTH_LOG(ERROR) << "PostAsyncResults failed: "
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
    BLUETOOTH_LOG(ERROR)
        << "ActivateBluetoothAdvertisementLEWatcherInstance failed: "
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
    BLUETOOTH_LOG(ERROR) << "Setting ScanningMode to Active failed: "
                         << logging::SystemErrorCodeToString(hr);
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), /*is_error=*/true,
                       UMABluetoothDiscoverySessionOutcome::UNKNOWN));
    return;
  }

  advertisement_received_token_ = AddTypedEventHandler(
      ble_advertisement_watcher_.Get(),
      &IBluetoothLEAdvertisementWatcher::add_Received,
      base::BindRepeating(&BluetoothAdapterWinrt::OnAdvertisementReceived,
                          weak_ptr_factory_.GetWeakPtr()));
  if (!advertisement_received_token_) {
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), /*is_error=*/true,
                       UMABluetoothDiscoverySessionOutcome::UNKNOWN));
    return;
  }

  advertisement_watcher_stopped_token_ = AddTypedEventHandler(
      ble_advertisement_watcher_.Get(),
      &IBluetoothLEAdvertisementWatcher::add_Stopped,
      base::BindRepeating(&BluetoothAdapterWinrt::OnAdvertisementWatcherStopped,
                          weak_ptr_factory_.GetWeakPtr()));
  if (!advertisement_watcher_stopped_token_) {
    RemoveAdvertisementWatcherEventHandlers();
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), /*is_error=*/true,
                       UMABluetoothDiscoverySessionOutcome::UNKNOWN));
    return;
  }

  hr = ble_advertisement_watcher_->Start();
  if (FAILED(hr)) {
    BLUETOOTH_LOG(ERROR) << "Starting the Advertisement Watcher failed: "
                         << logging::SystemErrorCodeToString(hr);
    RemoveAdvertisementWatcherEventHandlers();
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), /*is_error=*/true,
                       UMABluetoothDiscoverySessionOutcome::UNKNOWN));
    return;
  }

  BluetoothLEAdvertisementWatcherStatus watcher_status;
  hr = ble_advertisement_watcher_->get_Status(&watcher_status);
  if (FAILED(hr)) {
    BLUETOOTH_LOG(ERROR) << "Getting the Watcher Status failed: "
                         << logging::SystemErrorCodeToString(hr);
  } else if (watcher_status == BluetoothLEAdvertisementWatcherStatus_Aborted) {
    BLUETOOTH_LOG(ERROR)
        << "Starting Advertisement Watcher failed, it is in the Aborted "
           "state.";
    RemoveAdvertisementWatcherEventHandlers();
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), /*is_error=*/true,
                       UMABluetoothDiscoverySessionOutcome::UNKNOWN));
    return;
  }

  for (auto& observer : observers_) {
    observer.AdapterDiscoveringChanged(this, /*discovering=*/true);
  }

  ui_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), false,
                                UMABluetoothDiscoverySessionOutcome::SUCCESS));
}

void BluetoothAdapterWinrt::StopScan(DiscoverySessionResultCallback callback) {
  DCHECK_EQ(NumDiscoverySessions(), 0);

  RemoveAdvertisementWatcherEventHandlers();
  HRESULT hr = ble_advertisement_watcher_->Stop();
  if (FAILED(hr)) {
    BLUETOOTH_LOG(ERROR) << "Stopped the Advertisement Watcher failed: "
                         << logging::SystemErrorCodeToString(hr);
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), /*is_error=*/true,
                       UMABluetoothDiscoverySessionOutcome::UNKNOWN));
    return;
  }

  for (auto& device : devices_) {
    device.second->ClearAdvertisementData();
  }

  for (auto& observer : observers_) {
    observer.AdapterDiscoveringChanged(this, /*discovering=*/false);
  }

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
    BLUETOOTH_LOG(ERROR) << "RoActivateInstance failed: "
                         << logging::SystemErrorCodeToString(hr);
    return hr;
  }

  ComPtr<IBluetoothLEAdvertisementWatcher> watcher;
  hr = inspectable.As(&watcher);
  if (FAILED(hr)) {
    BLUETOOTH_LOG(ERROR) << "As IBluetoothLEAdvertisementWatcher failed: "
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
    BLUETOOTH_LOG(ERROR) << "Getting Default Adapter failed.";
    return;
  }

  adapter_ = std::move(adapter);
  uint64_t raw_address;
  HRESULT hr = adapter_->get_BluetoothAddress(&raw_address);
  if (FAILED(hr)) {
    BLUETOOTH_LOG(ERROR) << "Getting BluetoothAddress failed: "
                         << logging::SystemErrorCodeToString(hr);
    return;
  }

  address_ =
      CanonicalizeBluetoothAddress(base::StringPrintf("%012llX", raw_address));
  DCHECK(!address_.empty());

  HSTRING device_id;
  hr = adapter_->get_DeviceId(&device_id);
  if (FAILED(hr)) {
    BLUETOOTH_LOG(ERROR) << "Getting DeviceId failed: "
                         << logging::SystemErrorCodeToString(hr);
    return;
  }

  ComPtr<IAsyncOperation<DeviceInformation*>> create_from_id_op;
  hr = device_information_statics_->CreateFromIdAsync(device_id,
                                                      &create_from_id_op);
  if (FAILED(hr)) {
    BLUETOOTH_LOG(ERROR) << "CreateFromIdAsync failed: "
                         << logging::SystemErrorCodeToString(hr);
    return;
  }

  hr = base::win::PostAsyncResults(
      std::move(create_from_id_op),
      base::BindOnce(&BluetoothAdapterWinrt::OnCreateFromIdAsync,
                     weak_ptr_factory_.GetWeakPtr(), std::move(on_init)));
  if (FAILED(hr)) {
    BLUETOOTH_LOG(ERROR) << "PostAsyncResults failed: "
                         << logging::SystemErrorCodeToString(hr);
  }
}

void BluetoothAdapterWinrt::OnCreateFromIdAsync(
    base::ScopedClosureRunner on_init,
    ComPtr<IDeviceInformation> device_information) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!device_information) {
    BLUETOOTH_LOG(ERROR) << "Getting Device Information failed.";
    return;
  }

  HSTRING name;
  HRESULT hr = device_information->get_Name(&name);
  if (FAILED(hr)) {
    BLUETOOTH_LOG(ERROR) << "Getting Name failed: "
                         << logging::SystemErrorCodeToString(hr);
    return;
  }

  name_ = base::win::ScopedHString(name).GetAsUTF8();

  ComPtr<IAsyncOperation<RadioAccessStatus>> request_access_op;
  hr = radio_statics_->RequestAccessAsync(&request_access_op);
  if (FAILED(hr)) {
    BLUETOOTH_LOG(ERROR) << "RequestAccessAsync failed: "
                         << logging::SystemErrorCodeToString(hr);
    return;
  }

  hr = base::win::PostAsyncResults(
      std::move(request_access_op),
      base::BindOnce(&BluetoothAdapterWinrt::OnRequestRadioAccess,
                     weak_ptr_factory_.GetWeakPtr(), std::move(on_init)));

  if (FAILED(hr)) {
    BLUETOOTH_LOG(ERROR) << "PostAsyncResults failed: "
                         << logging::SystemErrorCodeToString(hr);
  }
}

void BluetoothAdapterWinrt::OnRequestRadioAccess(
    base::ScopedClosureRunner on_init,
    RadioAccessStatus access_status) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  radio_access_allowed_ = access_status == RadioAccessStatus_Allowed;
  if (!radio_access_allowed_) {
    // This happens if "Allow apps to control device radios" is off in Privacy
    // settings.
    BLUETOOTH_LOG(ERROR) << "RequestRadioAccessAsync failed: "
                         << ToCString(access_status)
                         << "Will not be able to change radio power.";
  }

  ComPtr<IAsyncOperation<Radio*>> get_radio_op;
  HRESULT hr = adapter_->GetRadioAsync(&get_radio_op);
  if (FAILED(hr)) {
    BLUETOOTH_LOG(ERROR) << "GetRadioAsync failed: "
                         << logging::SystemErrorCodeToString(hr);
    return;
  }

  hr = base::win::PostAsyncResults(
      std::move(get_radio_op),
      base::BindOnce(&BluetoothAdapterWinrt::OnGetRadio,
                     weak_ptr_factory_.GetWeakPtr(), std::move(on_init)));

  if (FAILED(hr)) {
    BLUETOOTH_LOG(ERROR) << "PostAsyncResults failed: "
                         << logging::SystemErrorCodeToString(hr);
  }
}

void BluetoothAdapterWinrt::OnGetRadio(base::ScopedClosureRunner on_init,
                                       ComPtr<IRadio> radio) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (radio) {
    radio_ = std::move(radio);
    radio_was_powered_ = GetState(radio_.Get()) == RadioState_On;
    radio_state_changed_token_ = AddTypedEventHandler(
        radio_.Get(), &IRadio::add_StateChanged,
        base::BindRepeating(&BluetoothAdapterWinrt::OnRadioStateChanged,
                            weak_ptr_factory_.GetWeakPtr()));

    if (!radio_state_changed_token_)
      BLUETOOTH_LOG(ERROR) << "Adding Radio State Changed Handler failed.";
    return;
  }

  // This happens within WoW64, due to an issue with non-native APIs.
  BLUETOOTH_LOG(ERROR)
      << "Getting Radio failed. Chrome will be unable to change the power "
         "state by itself.";

  // Attempt to create a DeviceWatcher for powered radios, so that querying
  // the power state is still possible.
  auto aqs_filter = base::win::ScopedHString::Create(kPoweredRadiosAqsFilter);
  HRESULT hr = device_information_statics_->CreateWatcherAqsFilter(
      aqs_filter.get(), &powered_radio_watcher_);
  if (FAILED(hr)) {
    BLUETOOTH_LOG(ERROR) << "Creating Powered Radios Watcher failed: "
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
    BLUETOOTH_LOG(ERROR) << "Failed to Register Powered Radio Event Handlers.";
    TryRemovePoweredRadioEventHandlers();
    return;
  }

  hr = powered_radio_watcher_->Start();
  if (FAILED(hr)) {
    BLUETOOTH_LOG(ERROR) << "Starting the Powered Radio Watcher failed: "
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
    BLUETOOTH_LOG(ERROR) << "Got unexpected Radio Access Status: "
                         << ToCString(access_status);
    RunPendingPowerCallbacks();
  }
}

void BluetoothAdapterWinrt::OnRadioStateChanged(IRadio* radio,
                                                IInspectable* object) {
  DCHECK(radio_.Get() == radio);
  RunPendingPowerCallbacks();

  // Deduplicate StateChanged events, which can occur twice for a single
  // power-on change.
  const bool is_powered = GetState(radio) == RadioState_On;
  if (radio_was_powered_ == is_powered) {
    return;
  }
  radio_was_powered_ = is_powered;
  NotifyAdapterPoweredChanged(is_powered);
}

void BluetoothAdapterWinrt::OnPoweredRadioAdded(IDeviceWatcher* watcher,
                                                IDeviceInformation* info) {
  if (++num_powered_radios_ == 1)
    NotifyAdapterPoweredChanged(true);
  BLUETOOTH_LOG(ERROR) << "OnPoweredRadioAdded(), Number of Powered Radios: "
                       << num_powered_radios_;
}

void BluetoothAdapterWinrt::OnPoweredRadioRemoved(
    IDeviceWatcher* watcher,
    IDeviceInformationUpdate* update) {
  if (--num_powered_radios_ == 0)
    NotifyAdapterPoweredChanged(false);
  BLUETOOTH_LOG(ERROR) << "OnPoweredRadioRemoved(), Number of Powered Radios: "
                       << num_powered_radios_;
}

void BluetoothAdapterWinrt::OnPoweredRadiosEnumerated(IDeviceWatcher* watcher,
                                                      IInspectable* object) {
  BLUETOOTH_LOG(ERROR)
      << "OnPoweredRadiosEnumerated(), Number of Powered Radios: "
      << num_powered_radios_;
  // Destroy the ScopedClosureRunner, triggering the contained Closure to be
  // run. Note this may destroy |this|.
  DCHECK(on_init_);
  on_init_.reset();
}

void BluetoothAdapterWinrt::OnAdvertisementReceived(
    IBluetoothLEAdvertisementWatcher* watcher,
    IBluetoothLEAdvertisementReceivedEventArgs* received) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  uint64_t raw_bluetooth_address;
  HRESULT hr = received->get_BluetoothAddress(&raw_bluetooth_address);
  if (FAILED(hr)) {
    BLUETOOTH_LOG(ERROR) << "get_BluetoothAddress() failed: "
                         << logging::SystemErrorCodeToString(hr);
    return;
  }

  const std::string bluetooth_address =
      BluetoothDeviceWinrt::CanonicalizeAddress(raw_bluetooth_address);
  auto it = devices_.find(bluetooth_address);
  const bool is_new_device = (it == devices_.end());
  if (is_new_device) {
    bool was_inserted = false;
    std::tie(it, was_inserted) = devices_.emplace(
        bluetooth_address, CreateDevice(raw_bluetooth_address));
    DCHECK(was_inserted);
  }

  BluetoothDevice* const device = it->second.get();

  int16_t rssi = 0;
  hr = received->get_RawSignalStrengthInDBm(&rssi);
  if (FAILED(hr)) {
    BLUETOOTH_LOG(ERROR) << "get_RawSignalStrengthInDBm() failed: "
                         << logging::SystemErrorCodeToString(hr);
  }

  // Extract the remaining advertisement data.
  ComPtr<IBluetoothLEAdvertisement> advertisement = GetAdvertisement(received);
  std::optional<std::string> device_name =
      ExtractDeviceName(advertisement.Get());
  std::optional<int8_t> tx_power = ExtractTxPower(advertisement.Get());
  BluetoothDevice::UUIDList advertised_uuids =
      ExtractAdvertisedUUIDs(advertisement.Get());
  BluetoothDevice::ServiceDataMap service_data_map =
      ExtractServiceData(advertisement.Get());
  BluetoothDevice::ManufacturerDataMap manufacturer_data_map =
      ExtractManufacturerData(advertisement.Get());

  static_cast<BluetoothDeviceWinrt*>(device)->UpdateLocalName(device_name);
  device->UpdateAdvertisementData(rssi, ExtractFlags(advertisement.Get()),
                                  advertised_uuids, tx_power, service_data_map,
                                  manufacturer_data_map);

  for (auto& observer : observers_) {
    observer.DeviceAdvertisementReceived(
        bluetooth_address, device->GetName(),
        /*advertisement_name=*/device_name, rssi, tx_power,
        device->GetAppearance(), advertised_uuids, service_data_map,
        manufacturer_data_map);
    is_new_device ? observer.DeviceAdded(this, device)
                  : observer.DeviceChanged(this, device);
  }
}

void BluetoothAdapterWinrt::OnAdvertisementWatcherStopped(
    ABI::Windows::Devices::Bluetooth::Advertisement::
        IBluetoothLEAdvertisementWatcher* watcher,
    ABI::Windows::Devices::Bluetooth::Advertisement::
        IBluetoothLEAdvertisementWatcherStoppedEventArgs* args) {
  BluetoothError error;
  HRESULT hr = args->get_Error(&error);
  if (FAILED(hr)) {
    BLUETOOTH_LOG(ERROR) << "get_Error() failed: " << hr;
    return;
  }
  BLUETOOTH_LOG(DEBUG) << "OnAdvertisementWatcherStopped() error=" << error;

  MarkDiscoverySessionsAsInactive();
}

void BluetoothAdapterWinrt::OnRegisterAdvertisement(
    BluetoothAdvertisement* advertisement,
    CreateAdvertisementCallback callback) {
  DCHECK(base::Contains(pending_advertisements_, advertisement));
  auto wrapped_advertisement = base::WrapRefCounted(advertisement);
  std::erase(pending_advertisements_, advertisement);
  std::move(callback).Run(std::move(wrapped_advertisement));
}

void BluetoothAdapterWinrt::OnRegisterAdvertisementError(
    BluetoothAdvertisement* advertisement,
    AdvertisementErrorCallback error_callback,
    BluetoothAdvertisement::ErrorCode error_code) {
  // Note: We are not DCHECKing that |pending_advertisements_| contains
  // |advertisement|, as this method might be invoked during destruction.
  std::erase(pending_advertisements_, advertisement);
  std::move(error_callback).Run(error_code);
}

void BluetoothAdapterWinrt::TryRemoveRadioStateChangedHandler() {
  DCHECK(radio_);
  if (!radio_state_changed_token_)
    return;

  HRESULT hr = radio_->remove_StateChanged(*radio_state_changed_token_);
  if (FAILED(hr)) {
    BLUETOOTH_LOG(ERROR) << "Removing Radio State Changed Handler failed: "
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
      BLUETOOTH_LOG(ERROR)
          << "Removing the Powered Radio Added Handler failed: "
          << logging::SystemErrorCodeToString(hr);
    }

    powered_radio_added_token_.reset();
  }

  if (powered_radio_removed_token_) {
    HRESULT hr =
        powered_radio_watcher_->remove_Removed(*powered_radio_removed_token_);
    if (FAILED(hr)) {
      BLUETOOTH_LOG(ERROR)
          << "Removing the Powered Radio Removed Handler failed: "
          << logging::SystemErrorCodeToString(hr);
    }

    powered_radio_removed_token_.reset();
  }

  if (powered_radios_enumerated_token_) {
    HRESULT hr = powered_radio_watcher_->remove_EnumerationCompleted(
        *powered_radios_enumerated_token_);
    if (FAILED(hr)) {
      BLUETOOTH_LOG(ERROR)
          << "Removing the Powered Radios Enumerated Handler failed: "
          << logging::SystemErrorCodeToString(hr);
    }

    powered_radios_enumerated_token_.reset();
  }
}

void BluetoothAdapterWinrt::RemoveAdvertisementWatcherEventHandlers() {
  DCHECK(ble_advertisement_watcher_);
  if (advertisement_received_token_) {
    HRESULT hr = ble_advertisement_watcher_->remove_Received(
        *advertisement_received_token_);
    if (FAILED(hr)) {
      BLUETOOTH_LOG(ERROR) << "Removing the Received Handler failed: "
                           << logging::SystemErrorCodeToString(hr);
    }
  }
  if (advertisement_watcher_stopped_token_) {
    HRESULT hr = ble_advertisement_watcher_->remove_Stopped(
        *advertisement_watcher_stopped_token_);
    if (FAILED(hr)) {
      BLUETOOTH_LOG(ERROR) << "Removing the Stopped Handler failed: "
                           << logging::SystemErrorCodeToString(hr);
    }
  }
}

}  // namespace device
