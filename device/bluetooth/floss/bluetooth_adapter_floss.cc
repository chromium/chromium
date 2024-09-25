// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/bluetooth_adapter_floss.h"

#include "base/containers/contains.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "components/device_event_log/device_event_log.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_socket_thread.h"
#include "device/bluetooth/chromeos_platform_features.h"
#include "device/bluetooth/floss/bluetooth_advertisement_floss.h"
#include "device/bluetooth/floss/bluetooth_device_floss.h"
#include "device/bluetooth/floss/bluetooth_local_gatt_service_floss.h"
#include "device/bluetooth/floss/bluetooth_low_energy_scan_session_floss.h"
#include "device/bluetooth/floss/bluetooth_socket_floss.h"
#include "device/bluetooth/floss/floss_dbus_manager.h"
#include "device/bluetooth/floss/floss_lescan_client.h"
#include "device/bluetooth/floss/floss_socket_manager.h"
#include "device/bluetooth/public/cpp/bluetooth_address.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "device/bluetooth/chromeos/bluetooth_connection_logger.h"
#include "device/bluetooth/chromeos/bluetooth_utils.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/devicetype.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace floss {

namespace {
using device::UMABluetoothDiscoverySessionOutcome;

UMABluetoothDiscoverySessionOutcome TranslateDiscoveryErrorToUMA(
    const Error& error) {
  // TODO(b/192289534) - Deal with UMA later
  return UMABluetoothDiscoverySessionOutcome::NOT_IMPLEMENTED;
}

// Helper function to gate init behind a check for Object Manager support.
void InitWhenObjectManagerKnown(base::OnceClosure callback) {
  FlossDBusManager::Get()->CallWhenObjectManagerSupportIsKnown(
      std::move(callback));
}

bool DeviceNeedsToReadProperties(device::BluetoothDevice* device) {
  if (device) {
    BluetoothDeviceFloss* floss_device =
        static_cast<BluetoothDeviceFloss*>(device);
    return !(floss_device->HasReadProperties() ||
             floss_device->IsReadingProperties());
  }

  return true;
}

}  // namespace

// Empty delegate for BLE scanning used during discovery.
class BleDelegateForDiscovery
    : public device::BluetoothLowEnergyScanSession::Delegate {
 public:
  BleDelegateForDiscovery() = default;
  ~BleDelegateForDiscovery() override = default;

  base::WeakPtr<BleDelegateForDiscovery> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // Empty device::BluetoothLowEnergyScanSession::Delegate overrides
  void OnSessionStarted(
      device::BluetoothLowEnergyScanSession* scan_session,
      std::optional<device::BluetoothLowEnergyScanSession::ErrorCode>
          error_code) override {}
  void OnDeviceFound(device::BluetoothLowEnergyScanSession* scan_session,
                     device::BluetoothDevice* device) override {}
  void OnDeviceLost(device::BluetoothLowEnergyScanSession* scan_session,
                    device::BluetoothDevice* device) override {}
  void OnSessionInvalidated(
      device::BluetoothLowEnergyScanSession* scan_session) override {}

 private:
  base::WeakPtrFactory<BleDelegateForDiscovery> weak_ptr_factory_{this};
};

// According to the Bluetooth spec, these are the min and max values possible
// for advertising interval. Core 5.3 Spec, Vol 4, Part E, Section 7.8.5.
constexpr uint16_t kMinIntervalMs = 20;
constexpr uint16_t kMaxIntervalMs = 10240;

// static
scoped_refptr<BluetoothAdapterFloss> BluetoothAdapterFloss::CreateAdapter() {
  return base::WrapRefCounted(new BluetoothAdapterFloss());
}

BluetoothAdapterFloss::BluetoothAdapterFloss() {
  ui_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
  socket_thread_ = device::BluetoothSocketThread::Get();

  le_discovery_session_delegate_ = std::make_unique<BleDelegateForDiscovery>();
}

BluetoothAdapterFloss::~BluetoothAdapterFloss() {
  Shutdown();
}

void BluetoothAdapterFloss::Initialize(base::OnceClosure callback) {
  BLUETOOTH_LOG(EVENT) << "BluetoothAdapterFloss::Initialize";
  init_callback_ = std::move(callback);

  // Go ahead to Init() if object manager support is already known (e.g. when
  // using fake clients), otherwise find out object manager support first below.
  if (floss::FlossDBusManager::Get()->IsObjectManagerSupportKnown()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&BluetoothAdapterFloss::Init,
                                  weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // Queue a task to check for ObjectManager support and init once the support
  // is known.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&InitWhenObjectManagerKnown,
                     base::BindOnce(&BluetoothAdapterFloss::Init,
                                    weak_ptr_factory_.GetWeakPtr())));
}

void BluetoothAdapterFloss::Shutdown() {
  BLUETOOTH_LOG(EVENT) << "BluetoothAdapterFloss::Shutdown";

  if (dbus_is_shutdown_)
    return;

  if (!FlossDBusManager::Get()->IsObjectManagerSupported()) {
    dbus_is_shutdown_ = true;
    return;
  }

  if (IsPresent())
    RemoveAdapter();  // Cleans up devices and adapter observers.
  DCHECK(devices_.empty());

  // This may call unregister on advertisements that have already been
  // unregistered but that's fine. The advertisement object keeps a track of
  // the fact that it has been already unregistered and will call our empty
  // error callback with an "Already unregistered" error, which we'll ignore.
  for (const auto& adv : advertisements_) {
    adv->Stop(base::DoNothing(), base::DoNothing());
  }
  advertisements_.clear();

  FlossDBusManager::Get()->GetManagerClient()->RemoveObserver(this);
  dbus_is_shutdown_ = true;

  for (const auto& [key, scanner] : scanners_) {
    scanner->OnRelease();
  }
  scanners_.clear();
}

void BluetoothAdapterFloss::AddAdapterObservers() {
  DCHECK(FlossDBusManager::Get()->HasActiveAdapter());

  // Add any observers that depend on a specific observer.
  // FlossDBusManager::SwitchAdapter will control what the active adapter is
  // that we are controlling.
  FlossDBusManager::Get()->GetAdapterClient()->AddObserver(this);
  FlossDBusManager::Get()->GetLEScanClient()->AddObserver(this);
  FlossDBusManager::Get()->GetBatteryManagerClient()->AddObserver(this);
#if BUILDFLAG(IS_CHROMEOS)
  FlossDBusManager::Get()->GetAdminClient()->AddObserver(this);
#endif  // BUILDFLAG(IS_CHROMEOS)
}

void BluetoothAdapterFloss::RemoveAdapterObservers() {
  // Clean up observers
  FlossDBusManager::Get()->GetAdapterClient()->RemoveObserver(this);
  FlossDBusManager::Get()->GetLEScanClient()->RemoveObserver(this);
  FlossDBusManager::Get()->GetBatteryManagerClient()->RemoveObserver(this);
#if BUILDFLAG(IS_CHROMEOS)
  FlossDBusManager::Get()->GetAdminClient()->RemoveObserver(this);
#endif  // BUILDFLAG(IS_CHROMEOS)
}

void BluetoothAdapterFloss::RemoveAdapter() {
  if (!FlossDBusManager::Get()->HasActiveAdapter()) {
    return;
  }

  RemoveAdapterObservers();
  ClearAllDevices();

  // Remove adapter by switching to an invalid adapter (cleans up DBus clients)
  // and then emitting |AdapterPresentChanged| to observers.
  FlossDBusManager::Get()->SwitchAdapter(FlossDBusManager::kInvalidAdapter,
                                         base::DoNothing());
  PresentChanged(false);
}

void BluetoothAdapterFloss::PopulateInitialDevices() {
  FlossDBusManager::Get()->GetAdapterClient()->GetBondedDevices();
  FlossDBusManager::Get()->GetAdapterClient()->GetConnectedDevices();
}

void BluetoothAdapterFloss::Init() {
  // If dbus is shutdown or ObjectManager isn't supported, we just return
  // without initializing anything.
  if (dbus_is_shutdown_ ||
      !FlossDBusManager::Get()->IsObjectManagerSupported()) {
    BLUETOOTH_LOG(ERROR) << "Floss Adapter initialized without object manager";
    initialized_ = true;
    std::move(init_callback_).Run();
    return;
  }

  BLUETOOTH_LOG(EVENT) << "Floss Adapter Initialized";

  // Register for manager callbacks
  FlossDBusManager::Get()->GetManagerClient()->AddObserver(this);

  // Start with invalid DBus clients. This will return right away so don't need
  // to wait for callback.
  FlossDBusManager::Get()->SwitchAdapter(FlossDBusManager::kInvalidAdapter,
                                         base::DoNothing());

  // Switch to adapter if the default adapter is present and enabled. If it is
  // not enabled, wait for upper layers to power it on.
  if (IsPresent()) {
    FlossManagerClient* manager = FlossDBusManager::Get()->GetManagerClient();
    int default_adapter = manager->GetDefaultAdapter();

    if (manager->GetAdapterEnabled(default_adapter)) {
      AdapterEnabledChanged(default_adapter, /*enabled=*/true);
    }
  }

  VLOG(1) << "BluetoothAdapterFloss::Init completed. Calling init callback.";
  initialized_ = true;

  std::move(init_callback_).Run();
}

void BluetoothAdapterFloss::NotifyDeviceFound(uint8_t scanner_id,
                                              const std::string& address) {
  if (!base::Contains(devices_, address)) {
    return;
  }

  BluetoothDeviceFloss* device_ptr =
      static_cast<BluetoothDeviceFloss*>(devices_[address].get());

  for (const auto& [key, scanner] : scanners_) {
    if (scanner->GetScannerId() == scanner_id) {
      scanner->OnDeviceFound(device_ptr);
    }
  }
}

BluetoothDeviceFloss* BluetoothAdapterFloss::CreateOrGetDeviceForUpdate(
    const std::string& address,
    const std::string& name) {
  BluetoothDeviceFloss* device_ptr;
  std::string canonical_address = device::CanonicalizeBluetoothAddress(address);

  if (base::Contains(devices_, canonical_address)) {
    device_ptr =
        static_cast<BluetoothDeviceFloss*>(devices_[canonical_address].get());
    device_ptr->UpdateTimestamp();
  } else {
    auto device = CreateBluetoothDeviceFloss(
        FlossDeviceId({.address = address, .name = name}));
    device_ptr = device.get();
    devices_.emplace(canonical_address, std::move(device));
  }
  return device_ptr;
}

BluetoothAdapterFloss::UUIDList BluetoothAdapterFloss::GetUUIDs() const {
  return {};
}

std::string BluetoothAdapterFloss::GetAddress() const {
  if (IsPowered()) {
    return FlossDBusManager::Get()->GetAdapterClient()->GetAddress();
  }

  return std::string();
}

std::string BluetoothAdapterFloss::GetName() const {
  if (!IsPresent())
    return std::string();

  return FlossDBusManager::Get()->GetAdapterClient()->GetName();
}

std::string BluetoothAdapterFloss::GetSystemName() const {
  // TODO(b/238230098): Floss should expose system information, i.e. stack name
  // and version.
  return "Floss";
}

void BluetoothAdapterFloss::SetName(const std::string& name,
                                    base::OnceClosure callback,
                                    ErrorCallback error_callback) {
  if (!IsPresent()) {
    BLUETOOTH_LOG(ERROR) << "SetName: " << name << ". Not Present!";
    std::move(error_callback).Run();
    return;
  }

  FlossDBusManager::Get()->GetAdapterClient()->SetName(
      base::BindOnce(&BluetoothAdapterFloss::OnMethodResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(error_callback)),
      name);
}

bool BluetoothAdapterFloss::IsInitialized() const {
  return initialized_;
}

bool BluetoothAdapterFloss::IsPresent() const {
  // No clients will be working if object manager isn't supported or dbus is
  // shut down
  if (dbus_is_shutdown_ ||
      !FlossDBusManager::Get()->IsObjectManagerSupported()) {
    VLOG(1) << "BluetoothAdapterFloss::IsPresent = false (no object manager "
               "support or dbus is shut down)";
    return false;
  }

  FlossManagerClient* manager = FlossDBusManager::Get()->GetManagerClient();
  auto present = manager->GetAdapterPresent(manager->GetDefaultAdapter());

  return present;
}

bool BluetoothAdapterFloss::IsPowered() const {
  auto powered = FlossDBusManager::Get()->HasActiveAdapter();

  return powered;
}

void BluetoothAdapterFloss::SetPowered(bool powered,
                                       base::OnceClosure callback,
                                       ErrorCallback error_callback) {
  if (!IsPresent()) {
    BLUETOOTH_LOG(ERROR) << "SetPowered: " << powered << ". Not Present!";
    std::move(error_callback).Run();
    return;
  }

  BLUETOOTH_LOG(EVENT) << __func__ << ": " << powered;

  FlossDBusManager::Get()->GetManagerClient()->SetAdapterEnabled(
      FlossDBusManager::Get()->GetManagerClient()->GetDefaultAdapter(), powered,
      base::BindOnce(&BluetoothAdapterFloss::OnMethodResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(error_callback)));
}

bool BluetoothAdapterFloss::IsDiscoverable() const {
  if (!IsPresent())
    return false;

  return FlossDBusManager::Get()->GetAdapterClient()->GetDiscoverable();
}

void BluetoothAdapterFloss::SetDiscoverable(bool discoverable,
                                            base::OnceClosure callback,
                                            ErrorCallback error_callback) {
  if (!IsPresent()) {
    BLUETOOTH_LOG(ERROR) << "SetDiscoverable: " << discoverable
                         << ". Not Present!";
    std::move(error_callback).Run();
    return;
  }

  FlossDBusManager::Get()->GetAdapterClient()->SetDiscoverable(
      base::BindOnce(&BluetoothAdapterFloss::OnMethodResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(error_callback)),
      discoverable);
}

base::TimeDelta BluetoothAdapterFloss::GetDiscoverableTimeout() const {
  if (!IsPresent()) {
    return base::Seconds(0);
  }

  return base::Seconds(
      FlossDBusManager::Get()->GetAdapterClient()->GetDiscoverableTimeout());
}

bool BluetoothAdapterFloss::IsDiscovering() const {
  if (!IsPresent())
    return false;

  return NumScanningDiscoverySessions() > 0;
}

std::unique_ptr<BluetoothDeviceFloss>
BluetoothAdapterFloss::CreateBluetoothDeviceFloss(FlossDeviceId device) {
  return std::make_unique<BluetoothDeviceFloss>(this, device, ui_task_runner_,
                                                socket_thread_);
}

void BluetoothAdapterFloss::OnMethodResponse(base::OnceClosure callback,
                                             ErrorCallback error_callback,
                                             DBusResult<Void> ret) {
  if (!ret.has_value()) {
    std::move(error_callback).Run();
    return;
  }

  std::move(callback).Run();
}

void BluetoothAdapterFloss::OnRepeatedDiscoverySessionResult(
    bool start_discovery,
    bool is_error,
    UMABluetoothDiscoverySessionOutcome outcome) {
  BLUETOOTH_LOG(DEBUG) << __func__ << ": Discovery result - is_error( "
                       << is_error
                       << "), outcome = " << static_cast<int>(outcome);

  // If starting discovery failed and we have active discovery sessions, mark
  // them as inactive.
  if (start_discovery && is_error && NumScanningDiscoverySessions() > 0) {
    BLUETOOTH_LOG(DEBUG) << "Marking sessions as inactive.";
    MarkDiscoverySessionsAsInactive();

    // If we failed to re-start a repeated discovery, that means the discovering
    // state is false and needs to be sent to observers (we won't receive
    // another discovering changed callback).
    for (auto& observer : observers_) {
      observer.AdapterDiscoveringChanged(this, /*discovering=*/false);
    }
  }
}

void BluetoothAdapterFloss::OnStartDiscovery(
    DiscoverySessionResultCallback callback,
    DBusResult<Void> ret) {
  if (!ret.has_value()) {
    // Adapter path only exists if active adapter hasn't disappeared
    auto adapter_path = FlossDBusManager::Get()->HasActiveAdapter()
                            ? FlossDBusManager::Get()
                                  ->GetAdapterClient()
                                  ->GetObjectPath()
                                  ->value()
                            : std::string();
    BLUETOOTH_LOG(ERROR) << adapter_path
                         << ": Failed to start discovery: " << ret.error();
    std::move(callback).Run(true, TranslateDiscoveryErrorToUMA(ret.error()));
    // Clear any LE discovery session if starting discovery failed.
    le_discovery_session_.reset(nullptr);
    return;
  }

  BLUETOOTH_LOG(EVENT) << __func__;

  if (IsPresent()) {
    std::move(callback).Run(false,
                            UMABluetoothDiscoverySessionOutcome::SUCCESS);
  } else {
    std::move(callback).Run(
        true, UMABluetoothDiscoverySessionOutcome::ADAPTER_REMOVED);
  }
}

void BluetoothAdapterFloss::OnStopDiscovery(
    DiscoverySessionResultCallback callback,
    DBusResult<Void> ret) {
  if (!ret.has_value()) {
    // Adapter path only exists if active adapter hasn't disappeared
    auto adapter_path = FlossDBusManager::Get()->HasActiveAdapter()
                            ? FlossDBusManager::Get()
                                  ->GetAdapterClient()
                                  ->GetObjectPath()
                                  ->value()
                            : std::string();
    BLUETOOTH_LOG(ERROR) << adapter_path
                         << ": Failed to stop discovery: " << ret.error();
    std::move(callback).Run(true, TranslateDiscoveryErrorToUMA(ret.error()));

    return;
  }

  BLUETOOTH_LOG(EVENT) << __func__;

  DCHECK_GE(NumDiscoverySessions(), 0);
  std::move(callback).Run(false, UMABluetoothDiscoverySessionOutcome::SUCCESS);
}

void BluetoothAdapterFloss::OnInitializeDeviceProperties(
    BluetoothDeviceFloss* device_ptr) {
  for (auto& observer : observers_) {
    observer.DeviceAdded(this, device_ptr);
  }
}

void BluetoothAdapterFloss::OnDeviceUuidsChanged(
    BluetoothDeviceFloss* device_ptr) {
  // SDP done. Calling |SetGattServicesDiscoveryComplete| because it actually
  // refers to all services including SDP, not just GATT.
  device_ptr->SetGattServicesDiscoveryComplete(true);
  NotifyDeviceChanged(device_ptr);
}

void BluetoothAdapterFloss::OnGetConnectionState(const FlossDeviceId& device_id,
                                                 DBusResult<uint32_t> ret) {
  BluetoothDeviceFloss* device =
      static_cast<BluetoothDeviceFloss*>(GetDevice(device_id.address));

  if (!device) {
    LOG(WARNING) << "GetConnectionState returned for a non-existing device "
                 << device_id;
    return;
  }

  if (!ret.has_value()) {
    LOG(WARNING) << "GetConnectionState returned error: " << ret.error()
                 << " on device: " << device_id;
    return;
  }

  // Connected if connection state >= 1:
  // https://android.googlesource.com/platform/packages/modules/Bluetooth/+/84eff3217e552cbb3399e6deecdfce6748ae34ef/system/btif/src/btif_dm.cc#693
  device->SetConnectionState(*ret);

  // If the state is different than what is currently stored, update it.
  if ((*ret >= 1) != device->IsConnected()) {
    device->SetIsConnected(*ret >= 1);
    if (device->HasReadProperties()) {
      NotifyDeviceChanged(device);
      NotifyDeviceConnectedStateChanged(device, device->IsConnected());
    }
  }
}

void BluetoothAdapterFloss::OnGetBondState(const FlossDeviceId& device_id,
                                           DBusResult<uint32_t> ret) {
  BluetoothDeviceFloss* device =
      static_cast<BluetoothDeviceFloss*>(GetDevice(device_id.address));

  if (!device) {
    LOG(WARNING) << "GetBondState returned for a non-existing device "
                 << device_id;
    return;
  }

  if (!ret.has_value()) {
    LOG(WARNING) << "GetBondState returned error: " << ret.error()
                 << " on device: " << device_id;
    return;
  }

  device->SetBondState(static_cast<FlossAdapterClient::BondState>(*ret),
                       std::nullopt);
  if (device->HasReadProperties()) {
    NotifyDevicePairedChanged(device, device->IsPaired());
  }
}

void BluetoothAdapterFloss::OnGetBatteryInformation(
    DBusResult<std::optional<BatterySet>> battery_set) {
  if (!battery_set.has_value() || !battery_set.value().has_value()) {
    return;
  }

  auto set = battery_set.value().value();
  BatteryInfoUpdated(set.address, set);
}

// Announce to observers a change in the adapter state.
void BluetoothAdapterFloss::DiscoverableChanged(bool discoverable) {
  for (auto& observer : observers_) {
    observer.AdapterDiscoverableChanged(this, discoverable);
  }
}

void BluetoothAdapterFloss::DiscoveringChanged(bool discovering) {
  // If the adapter stopped discovery due to a reason other than a request by
  // us, reset the count to 0.
  BLUETOOTH_LOG(EVENT) << "Discovering changed: " << discovering;

  // While there are discovery sessions open, keep restarting discovery.
  if (!discovering && NumScanningDiscoverySessions() > 0) {
    FlossDBusManager::Get()->GetAdapterClient()->StartDiscovery(base::BindOnce(
        &BluetoothAdapterFloss::OnStartDiscovery,
        weak_ptr_factory_.GetWeakPtr(),
        base::BindOnce(&BluetoothAdapterFloss::OnRepeatedDiscoverySessionResult,
                       weak_ptr_factory_.GetWeakPtr(),
                       /*start_discovery=*/true)));

  } else {
    for (auto& observer : observers_) {
      observer.AdapterDiscoveringChanged(this, discovering);
    }

    // No active discovery sessions and we stopped discovering. Make sure the LE
    // session is also stopped.
    if (!discovering) {
      le_discovery_session_.reset(nullptr);
    }
  }
}

void BluetoothAdapterFloss::PresentChanged(bool present) {
  for (auto& observer : observers_) {
    observer.AdapterPresentChanged(this, present);
  }
}

void BluetoothAdapterFloss::NotifyAdapterPoweredChanged(bool powered) {
  for (auto& observer : observers_) {
    observer.AdapterPoweredChanged(this, powered);
  }
}

void BluetoothAdapterFloss::NotifyDeviceConnectedStateChanged(
    BluetoothDeviceFloss* device,
    bool is_now_connected) {
  DCHECK_EQ(device->IsConnected(), is_now_connected);

#if BUILDFLAG(IS_CHROMEOS)
  if (is_now_connected) {
    device::BluetoothConnectionLogger::RecordDeviceConnected(
        device->GetIdentifier(), device->GetDeviceType());
  } else {
    device::RecordDeviceDisconnect(device->GetDeviceType());
  }

  // Also log the total number of connected devices. This uses a sampled
  // histogram rather than a enumeration.
  int count = 0;
  for (auto& [unused_address, current_device] : devices_) {
    if (current_device->IsPaired() && current_device->IsConnected()) {
      count++;
    }
  }

  UMA_HISTOGRAM_COUNTS_100("Bluetooth.ConnectedDeviceCount", count);
#endif

  BluetoothAdapter::NotifyDeviceConnectedStateChanged(device, is_now_connected);
}

// Observers

void BluetoothAdapterFloss::AdapterPresent(int adapter, bool present) {
  VLOG(1) << "BluetoothAdapterFloss: Adapter " << adapter
          << ", present: " << present;
  // TODO(b/191906229) - Support non-default adapters
  if (adapter !=
      FlossDBusManager::Get()->GetManagerClient()->GetDefaultAdapter()) {
    return;
  }

  // If default adapter isn't present, we need to clean up the dbus manager
  if (!present) {
    RemoveAdapter();
    return;
  }

  if (FlossDBusManager::Get()->GetManagerClient()->GetAdapterEnabled(adapter) &&
      adapter != FlossDBusManager::Get()->GetActiveAdapter()) {
    // If the adapter is already enabled in platform layer, defer the present
    // changed until the clients are ready, so the observers could get the
    // correct power state right after present.
    FlossDBusManager::Get()->SwitchAdapter(
        adapter,
        base::BindOnce(&BluetoothAdapterFloss::OnAdapterClientsReady,
                       weak_ptr_factory_.GetWeakPtr(), /* enabled = */ true,
                       /* is_newly_present = */ true));
  } else {
    // Notify observers
    PresentChanged(present);
  }
}

void BluetoothAdapterFloss::AdapterEnabledChanged(int adapter, bool enabled) {
  // TODO(b/191906229) - Support non-default adapters
  if (adapter !=
      FlossDBusManager::Get()->GetManagerClient()->GetDefaultAdapter()) {
    VLOG(0) << __func__ << ": Adapter not default: "
            << FlossDBusManager::Get()->GetManagerClient()->GetDefaultAdapter();
    return;
  }

  if (enabled && adapter != FlossDBusManager::Get()->GetActiveAdapter()) {
    FlossDBusManager::Get()->SwitchAdapter(
        adapter, base::BindOnce(&BluetoothAdapterFloss::OnAdapterClientsReady,
                                weak_ptr_factory_.GetWeakPtr(), enabled,
                                /* is_newly_present = */ false));
  } else if (!enabled && FlossDBusManager::Get()->HasActiveAdapter()) {
    FlossDBusManager::Get()->SwitchAdapter(
        FlossDBusManager::kInvalidAdapter,
        base::BindOnce(&BluetoothAdapterFloss::OnAdapterClientsReady,
                       weak_ptr_factory_.GetWeakPtr(), enabled,
                       /* is_newly_present = */ false));
  }
}

void BluetoothAdapterFloss::OnAdapterClientsReady(bool enabled,
                                                  bool is_newly_present) {
  if (enabled) {
    AddAdapterObservers();
    PopulateInitialDevices();
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // No need to do this in Lacros because Ash would be around, and would have
    // done this already.
    SetStandardChromeOSAdapterName();
    if (base::FeatureList::IsEnabled(
            chromeos::bluetooth::features::kBluetoothFlossTelephony)) {
      ConfigureBluetoothTelephony(true);
    }

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  } else {
    ClearAllDevices();
    RemoveAdapterObservers();
  }

  if (is_newly_present) {
    PresentChanged(true);
  }

  NotifyAdapterPoweredChanged(enabled);
}

void BluetoothAdapterFloss::AdapterDiscoveringChanged(bool state) {
  DCHECK(IsPresent());

  DiscoveringChanged(state);
}

void BluetoothAdapterFloss::AdapterFoundDevice(
    const FlossDeviceId& device_found) {
  DCHECK(FlossDBusManager::Get());
  DCHECK(IsPresent());

  BLUETOOTH_LOG(EVENT) << __func__ << ": " << device_found;

  UpdateDeviceProperties(true, device_found);
}

void BluetoothAdapterFloss::UpdateDeviceProperties(
    bool is_triggered_by_inquiry,
    const FlossDeviceId& device_found) {
  DCHECK(FlossDBusManager::Get());
  DCHECK(IsPresent());

  auto device_floss = CreateBluetoothDeviceFloss(device_found);
  BluetoothDeviceFloss* new_device_ptr = nullptr;

  std::string canonical_address =
      device::CanonicalizeBluetoothAddress(device_floss->GetAddress());

  // Devices are newly found if they aren't in the devices_ map or they were
  // added via ScanResult (which doesn't trigger property reads).
  if (!base::Contains(devices_, canonical_address)) {
    new_device_ptr = device_floss.get();
    devices_.emplace(canonical_address, std::move(device_floss));
  } else if (DeviceNeedsToReadProperties(devices_[canonical_address].get())) {
    new_device_ptr =
        static_cast<BluetoothDeviceFloss*>(devices_[canonical_address].get());
  }

  BluetoothDeviceFloss::PropertiesState state =
      BluetoothDeviceFloss::PropertiesState::kTriggeredByScan;
  if (is_triggered_by_inquiry) {
    state = BluetoothDeviceFloss::PropertiesState::kTriggeredByInquiry;
  }

  // Trigger property reads for new devices.
  if (new_device_ptr) {
    new_device_ptr->InitializeDeviceProperties(
        state,
        base::BindOnce(&BluetoothAdapterFloss::OnInitializeDeviceProperties,
                       weak_ptr_factory_.GetWeakPtr(), new_device_ptr));

    // TODO(b/204708206): Convert "Paired" and "Connected" property into a
    // property framework.
    FlossDBusManager::Get()->GetAdapterClient()->GetBondState(
        base::BindOnce(&BluetoothAdapterFloss::OnGetBondState,
                       weak_ptr_factory_.GetWeakPtr(), device_found),
        device_found);
    FlossDBusManager::Get()->GetAdapterClient()->GetConnectionState(
        base::BindOnce(&BluetoothAdapterFloss::OnGetConnectionState,
                       weak_ptr_factory_.GetWeakPtr(), device_found),
        device_found);

    FlossDBusManager::Get()->GetBatteryManagerClient()->GetBatteryInformation(
        base::BindOnce(&BluetoothAdapterFloss::OnGetBatteryInformation,
                       weak_ptr_factory_.GetWeakPtr()),
        new_device_ptr->AsFlossDeviceId());

    return;
  }

  BluetoothDeviceFloss* device =
      static_cast<BluetoothDeviceFloss*>(devices_[canonical_address].get());

  // If the name has changed, we should also reinitialize the device properties.
  // For dual mode devices, if this is the first inquiry result received or
  // first scan result received, reinitialize the device properties.
  // NotifyDeviceChanged will get called after properties are re-init.
  if ((!device_found.name.empty() && device->GetName() != device_found.name) ||
      !(device->GetPropertiesState() & state)) {
    device->SetName(device_found.name);
    device->InitializeDeviceProperties(
        state, base::BindOnce(&BluetoothAdapterFloss::NotifyDeviceChanged,
                              weak_ptr_factory_.GetWeakPtr(), device));
  }
}

void BluetoothAdapterFloss::AdapterClearedDevice(
    const FlossDeviceId& device_cleared) {
  DCHECK(FlossDBusManager::Get());
  DCHECK(IsPresent());

  auto device_floss = CreateBluetoothDeviceFloss(device_cleared);
  std::string canonical_address =
      device::CanonicalizeBluetoothAddress(device_floss->GetAddress());
  if (base::Contains(devices_, canonical_address)) {
    BluetoothDeviceFloss* device_ptr = device_floss.get();
    BluetoothDeviceFloss* found_ptr = static_cast<BluetoothDeviceFloss*>(
        GetDevice(device_floss->GetAddress()));

    // Only remove devices from devices_ that are not paired or connected.
    if (!found_ptr || (!found_ptr->IsPaired() && !found_ptr->IsConnected())) {
      devices_.erase(canonical_address);

      for (auto& observer : observers_) {
        observer.DeviceRemoved(this, device_ptr);
      }
    }
  }

  BLUETOOTH_LOG(EVENT) << __func__ << ": " << device_cleared;
}

void BluetoothAdapterFloss::AdapterDevicePropertyChanged(
    FlossAdapterClient::BtPropertyType prop_type,
    const FlossDeviceId& device) {
  DCHECK(FlossDBusManager::Get());
  DCHECK(IsPresent());

  BLUETOOTH_LOG(EVENT) << __func__ << ": " << device
                       << ": prop_type = " << static_cast<uint32_t>(prop_type);

  BluetoothDeviceFloss* device_ptr =
      static_cast<BluetoothDeviceFloss*>(GetDevice(device.address));

  if (!device_ptr) {
    return;
  }

  switch (prop_type) {
    case FlossAdapterClient::BtPropertyType::kBdName:
      if (device.name.size() != 0 &&
          device.name != device_ptr->GetName().value_or("")) {
        device_ptr->SetName(device.name);
        device_ptr->InitializeDeviceProperties(
            BluetoothDeviceFloss::PropertiesState::kTriggeredByScan,
            base::BindOnce(&BluetoothAdapterFloss::NotifyDeviceChanged,
                           weak_ptr_factory_.GetWeakPtr(), device_ptr));
      }
      break;
    case FlossAdapterClient::BtPropertyType::kTypeOfDevice:
      device_ptr->FetchRemoteType(
          base::BindOnce(&BluetoothAdapterFloss::NotifyDeviceChanged,
                         weak_ptr_factory_.GetWeakPtr(), device_ptr));
      break;
    case FlossAdapterClient::BtPropertyType::kUuids:
      device_ptr->FetchRemoteUuids(
          base::BindOnce(&BluetoothAdapterFloss::OnDeviceUuidsChanged,
                         weak_ptr_factory_.GetWeakPtr(), device_ptr));
      break;
    case FlossAdapterClient::BtPropertyType::kAppearance:
      device_ptr->FetchRemoteAppearance(
          base::BindOnce(&BluetoothAdapterFloss::NotifyDeviceChanged,
                         weak_ptr_factory_.GetWeakPtr(), device_ptr));
      break;
    case FlossAdapterClient::BtPropertyType::kVendorProductInfo:
      device_ptr->FetchRemoteVendorProductInfo(
          base::BindOnce(&BluetoothAdapterFloss::NotifyDeviceChanged,
                         weak_ptr_factory_.GetWeakPtr(), device_ptr));
      break;
    case FlossAdapterClient::BtPropertyType::kRemoteAddrType:
      device_ptr->FetchRemoteAddressType(
          base::BindOnce(&BluetoothAdapterFloss::NotifyDeviceChanged,
                         weak_ptr_factory_.GetWeakPtr(), device_ptr));
      break;
    default:;  // Do nothing for other property types for now
  }
}

void BluetoothAdapterFloss::AdapterSspRequest(
    const FlossDeviceId& remote_device,
    uint32_t cod,
    FlossAdapterClient::BluetoothSspVariant variant,
    uint32_t passkey) {
  BluetoothDeviceFloss* device =
      static_cast<BluetoothDeviceFloss*>(GetDevice(remote_device.address));

  if (!device) {
    LOG(WARNING) << "SSP request for an unknown device";
    return;
  }

  BluetoothPairingFloss* pairing = device->pairing();

  // For incoming bonding which is not "just works", let the user decide whether
  // to accept or reject the request. Don't process "just works" requests as
  // it will be auto-accepted but it might be originated from a malicious peer.
  if (!pairing &&
      variant != FlossAdapterClient::BluetoothSspVariant::kConsent) {
    device::BluetoothDevice::PairingDelegate* pairing_delegate =
        DefaultPairingDelegate();
    if (pairing_delegate) {
      pairing = device->BeginPairing(pairing_delegate);
    }
  }

  if (!pairing) {
    // Reject the request right away to avoid users try to pair with it while
    // the remote is waiting reply.
    FlossDBusManager::Get()->GetAdapterClient()->SetPairingConfirmation(
        base::DoNothing(), remote_device, /*accept=*/false);
    return;
  }

  if (!pairing->active()) {
    LOG(WARNING) << "SSP request for an inactive pairing";
    return;
  }

  device::BluetoothDevice::PairingDelegate* pairing_delegate =
      pairing->pairing_delegate();

  if (!pairing_delegate) {
    LOG(WARNING) << "SSP request for an unknown delegate";
    return;
  }

  switch (variant) {
    case FlossAdapterClient::BluetoothSspVariant::kPasskeyConfirmation:
      pairing->SetPairingExpectation(
          BluetoothPairingFloss::PairingExpectation::kConfirmation);
      pairing_delegate->ConfirmPasskey(device, passkey);
      break;
    case FlossAdapterClient::BluetoothSspVariant::kPasskeyEntry:
      // TODO(b/202334519): Test with LEGO Mindstorms EV3.
      pairing->SetPairingExpectation(
          BluetoothPairingFloss::PairingExpectation::kPinCode);
      pairing_delegate->RequestPinCode(device);
      break;
    case FlossAdapterClient::BluetoothSspVariant::kConsent:
      // We don't need to ask pairing delegate for consent, because having a
      // pairing delegate means that a user is the initiator of this pairing.
      FlossDBusManager::Get()->GetAdapterClient()->SetPairingConfirmation(
          base::DoNothing(), remote_device, /*accept=*/true);
      device->ResetPairing();
      break;
    case FlossAdapterClient::BluetoothSspVariant::kPasskeyNotification:
      pairing_delegate->DisplayPasskey(device, passkey);
      break;
    default:
      LOG(ERROR) << "Unimplemented pairing method "
                 << static_cast<int>(variant);
  }
}

void BluetoothAdapterFloss::AdapterPinDisplay(
    const FlossDeviceId& remote_device,
    std::string pincode) {
  BluetoothDeviceFloss* device =
      static_cast<BluetoothDeviceFloss*>(GetDevice(remote_device.address));

  if (!device) {
    LOG(WARNING) << "PIN display for an unknown device";
    return;
  }

  if (pincode.length() != 6) {
    LOG(WARNING) << "PIN display for length=" << pincode.length()
                 << " is not supported";
    return;
  }

  BluetoothPairingFloss* pairing = device->pairing();

  // Initiate pairing data for the incoming bonding.
  if (!pairing) {
    device::BluetoothDevice::PairingDelegate* pairing_delegate =
        DefaultPairingDelegate();
    if (pairing_delegate) {
      pairing = device->BeginPairing(pairing_delegate);
    }
  }

  if (!pairing->active()) {
    LOG(WARNING) << "PIN display for an inactive pairing";
    return;
  }

  device::BluetoothDevice::PairingDelegate* pairing_delegate =
      pairing->pairing_delegate();

  if (!pairing_delegate) {
    LOG(WARNING) << "PIN display for an unknown delegate";
    return;
  }

  pairing_delegate->DisplayPinCode(device, pincode);
}

void BluetoothAdapterFloss::AdapterPinRequest(
    const FlossDeviceId& remote_device,
    uint32_t cod,
    bool min_16_digit) {
  BluetoothDeviceFloss* device =
      static_cast<BluetoothDeviceFloss*>(GetDevice(remote_device.address));

  if (!device) {
    LOG(WARNING) << "PIN request for an unknown device";
    return;
  }

  if (min_16_digit) {
    LOG(WARNING) << "16-digit pin is not supported";
    return;
  }

  BluetoothPairingFloss* pairing = device->pairing();

  // Initiate pairing data for the incoming bonding.
  if (!pairing) {
    device::BluetoothDevice::PairingDelegate* pairing_delegate =
        DefaultPairingDelegate();
    if (pairing_delegate) {
      pairing = device->BeginPairing(pairing_delegate);
    }
  }

  if (!pairing->active()) {
    LOG(WARNING) << "PIN request for an inactive pairing";
    return;
  }

  device::BluetoothDevice::PairingDelegate* pairing_delegate =
      pairing->pairing_delegate();

  if (!pairing_delegate) {
    LOG(WARNING) << "PIN request for an unknown delegate";
    return;
  }

  pairing_delegate->RequestPinCode(device);
}

void BluetoothAdapterFloss::DeviceBondStateChanged(
    const FlossDeviceId& remote_device,
    uint32_t status,
    FlossAdapterClient::BondState bond_state) {
  std::string canonical_address =
      device::CanonicalizeBluetoothAddress(remote_device.address);

  if (!base::Contains(devices_, canonical_address)) {
    LOG(WARNING) << "Received BondStateChanged for a non-existent device";
    return;
  }

  BLUETOOTH_LOG(EVENT) << "BondStateChanged " << remote_device.address
                       << " state = " << static_cast<uint32_t>(bond_state)
                       << " status = " << status;

  BluetoothDeviceFloss* device =
      static_cast<BluetoothDeviceFloss*>(devices_[canonical_address].get());

  if (status != 0) {
    if (device->pairing()) {
      // Mark that no actions should be triggered for pairing delegate.
      device->pairing()->SetActive(false);
    }
    LOG(ERROR) << "Received BondStateChanged with error status = " << status
               << " for " << remote_device.address;
    device->SetBondState(bond_state,
                         FlossDBusClient::BtifStatusToConnectErrorCode(
                             static_cast<FlossDBusClient::BtifStatus>(status)));
    if (bond_state == FlossAdapterClient::BondState::kNotBonded) {
      // Since we're no longer bonded, update connection state so that
      // ConnectCallback can process the error correctly.
      device->SetIsConnected(false);
    }
    NotifyDeviceChanged(device);
    NotifyDevicePairedChanged(device, device->IsPaired());

    // TODO(b/192289534): Record status in UMA.
    return;
  }

  if (device->GetBondState() == bond_state) {
    return;
  }

  device->SetBondState(bond_state, std::nullopt);
  NotifyDeviceChanged(device);
  NotifyDevicePairedChanged(device, device->IsPaired());

  if (bond_state == FlossAdapterClient::BondState::kNotBonded) {
    // If we're no longer bonded (or paired/connected), we should clear the
    // device so it doesn't show up in found devices list.
    AdapterClearedDevice(remote_device);
  }
}

void BluetoothAdapterFloss::AdapterDeviceConnected(
    const FlossDeviceId& device_id) {
  DCHECK(FlossDBusManager::Get());
  DCHECK(IsPresent());

  BLUETOOTH_LOG(EVENT) << __func__ << ": " << device_id;

  BluetoothDeviceFloss* device =
      static_cast<BluetoothDeviceFloss*>(GetDevice(device_id.address));
  if (!device) {
    BLUETOOTH_LOG(EVENT) << "Adding newly connected device to devices_ map: "
                         << device_id.address;
    UpdateDeviceProperties(false, device_id);
    return;
  }

  // TODO(b/220387308): Querying connection state after connection can be racy
  // with pairing state. We may need a separate pairing callback from Floss.
  FlossDBusManager::Get()->GetAdapterClient()->GetConnectionState(
      base::BindOnce(&BluetoothAdapterFloss::OnGetConnectionState,
                     weak_ptr_factory_.GetWeakPtr(), device_id),
      device_id);

  device->SetIsConnected(true);
  if (device->HasReadProperties()) {
    NotifyDeviceChanged(device);
    NotifyDeviceConnectedStateChanged(device, true);
  }
}

std::optional<device::BluetoothDevice::BatteryType> variant_to_battery_type(
    const std::string& variant) {
  std::unordered_map<std::string, device::BluetoothDevice::BatteryType>
      battery_type_lookup = {
          {"", device::BluetoothDevice::BatteryType::kDefault},
          {"left", device::BluetoothDevice::BatteryType::kLeftBudTrueWireless},
          {"right",
           device::BluetoothDevice::BatteryType::kRightBudTrueWireless},
          {"case", device::BluetoothDevice::BatteryType::kCaseTrueWireless},
      };
  if (!base::Contains(battery_type_lookup, variant)) {
    return std::nullopt;
  }
  return battery_type_lookup[variant];
}

void BluetoothAdapterFloss::BatteryInfoUpdated(std::string remote_address,
                                               BatterySet battery_set) {
  BluetoothDeviceFloss* device =
      static_cast<BluetoothDeviceFloss*>(GetDevice(remote_address));
  if (!device) {
    LOG(WARNING) << "BatterySet received for unknown device" << remote_address;
    return;
  }

  for (const auto& battery : battery_set.batteries) {
    std::optional<device::BluetoothDevice::BatteryType> battery_type =
        variant_to_battery_type(battery.variant);
    if (!battery_type) {
      LOG(WARNING) << "Unable to convert to battery_type from "
                   << battery.variant;
      continue;
    }
    device->SetBatteryInfo(device::BluetoothDevice::BatteryInfo(
        battery_type.value(), battery.percentage));
  }
}

void BluetoothAdapterFloss::AdapterDeviceDisconnected(
    const FlossDeviceId& device_id) {
  DCHECK(FlossDBusManager::Get());
  DCHECK(IsPresent());

  BLUETOOTH_LOG(EVENT) << __func__ << ": " << device_id;

  BluetoothDeviceFloss* device =
      static_cast<BluetoothDeviceFloss*>(GetDevice(device_id.address));
  if (!device) {
    LOG(WARNING) << "Device disconnected for an unknown device "
                 << device_id.address;
    return;
  }

  device->SetIsConnected(false);
  if (device->HasReadProperties()) {
    NotifyDeviceChanged(device);
    NotifyDeviceConnectedStateChanged(device, false);
  }
}

std::unordered_map<device::BluetoothDevice*, device::BluetoothDevice::UUIDSet>
BluetoothAdapterFloss::RetrieveGattConnectedDevicesWithDiscoveryFilter(
    const device::BluetoothDiscoveryFilter& discovery_filter) {
  NOTIMPLEMENTED();
  return {};
}

#if BUILDFLAG(IS_CHROMEOS)
void BluetoothAdapterFloss::DevicePolicyEffectChanged(
    const FlossDeviceId& device_id,
    const std::optional<PolicyEffect>& effect) {
  BLUETOOTH_LOG(EVENT) << __func__ << ": " << device_id;

  BluetoothDeviceFloss* device =
      static_cast<BluetoothDeviceFloss*>(GetDevice(device_id.address));
  if (!device) {
    LOG(WARNING) << "Device disconnected for an unknown device "
                 << device_id.address;
    return;
  }

  device->SetIsBlockedByPolicy(effect.has_value() ? effect.value().affected
                                                  : false);
}

void BluetoothAdapterFloss::ServiceAllowlistChanged(
    const std::vector<device::BluetoothUUID>& allowlist) {
  std::vector<std::string> uuid_str(allowlist.size());

  base::ranges::transform(
      allowlist, uuid_str.begin(),
      [](device::BluetoothUUID dev) { return dev.canonical_value(); });

  BLUETOOTH_LOG(EVENT) << __func__ << ": " << base::JoinString(uuid_str, ",");
  // TODO(b/257877673): Notify observers
}
#endif  // BUILDFLAG(IS_CHROMEOS)

void BluetoothAdapterFloss::CreateRfcommService(
    const device::BluetoothUUID& uuid,
    const ServiceOptions& options,
    CreateServiceCallback callback,
    CreateServiceErrorCallback error_callback) {
  DCHECK(!dbus_is_shutdown_);
  BLUETOOTH_LOG(DEBUG) << "Creating RFCOMM service: " << uuid.canonical_value();
  scoped_refptr<BluetoothSocketFloss> socket =
      BluetoothSocketFloss::CreateBluetoothSocket(ui_task_runner_,
                                                  socket_thread_);

  socket->Listen(this, FlossSocketManager::SocketType::kRfcomm, uuid, options,
                 base::BindOnce(std::move(callback), socket),
                 base::BindOnce(&BluetoothAdapterFloss::OnCreateServiceError,
                                weak_ptr_factory_.GetWeakPtr(), socket,
                                std::move(error_callback)));
}

void BluetoothAdapterFloss::CreateL2capService(
    const device::BluetoothUUID& uuid,
    const ServiceOptions& options,
    CreateServiceCallback callback,
    CreateServiceErrorCallback error_callback) {
  DCHECK(!dbus_is_shutdown_);
  BLUETOOTH_LOG(DEBUG) << "Creating L2CAP service: " << uuid.canonical_value();
  scoped_refptr<BluetoothSocketFloss> socket =
      BluetoothSocketFloss::CreateBluetoothSocket(ui_task_runner_,
                                                  socket_thread_);

  socket->Listen(this, FlossSocketManager::SocketType::kL2cap, uuid, options,
                 base::BindOnce(std::move(callback), socket),
                 base::BindOnce(&BluetoothAdapterFloss::OnCreateServiceError,
                                weak_ptr_factory_.GetWeakPtr(), socket,
                                std::move(error_callback)));
}

void BluetoothAdapterFloss::OnCreateServiceError(
    scoped_refptr<BluetoothSocketFloss> socket,
    CreateServiceErrorCallback error_callback,
    const std::string& error_message) {
  std::move(error_callback).Run(error_message);
}

void BluetoothAdapterFloss::RegisterAdvertisement(
    std::unique_ptr<device::BluetoothAdvertisement::Data> advertisement_data,
    CreateAdvertisementCallback callback,
    AdvertisementErrorCallback error_callback) {
  scoped_refptr<BluetoothAdvertisementFloss> advertisement(
      new BluetoothAdvertisementFloss(std::move(advertisement_data),
                                      interval_ms_, this));
  advertisement->Start(base::BindOnce(std::move(callback), advertisement),
                       std::move(error_callback));
  advertisements_.emplace_back(advertisement);
}

#if BUILDFLAG(IS_CHROMEOS)
bool BluetoothAdapterFloss::IsExtendedAdvertisementsAvailable() const {
  if (!IsPresent()) {
    return false;
  }

  return FlossDBusManager::Get()->GetAdapterClient()->IsExtAdvSupported();
}
#endif  // BUILDFLAG(IS_CHROMEOS)

void BluetoothAdapterFloss::SetAdvertisingInterval(
    const base::TimeDelta& min,
    const base::TimeDelta& max,
    base::OnceClosure callback,
    AdvertisementErrorCallback error_callback) {
  uint16_t min_ms = static_cast<uint16_t>(
      std::min(static_cast<int64_t>(std::numeric_limits<uint16_t>::max()),
               min.InMilliseconds()));
  uint16_t max_ms = static_cast<uint16_t>(
      std::min(static_cast<int64_t>(std::numeric_limits<uint16_t>::max()),
               max.InMilliseconds()));

  // TODO(b/253718595): Support a 'no preference' option so Floss can choose a
  // default value for the advertising interval. We are temporarily performing
  // parameter checking to fulfill existing callers' expectations.
  if (min_ms < kMinIntervalMs || max_ms > kMaxIntervalMs || min_ms > max_ms) {
    std::move(error_callback)
        .Run(device::BluetoothAdvertisement::
                 ERROR_INVALID_ADVERTISEMENT_INTERVAL);
    return;
  }
  interval_ms_ = min_ms;

  for (const auto& adv : advertisements_) {
    adv->SetAdvertisingInterval(interval_ms_, base::DoNothing(),
                                base::DoNothing());
  }
  std::move(callback).Run();
}

void BluetoothAdapterFloss::ResetAdvertising(
    base::OnceClosure callback,
    AdvertisementErrorCallback error_callback) {
  for (const auto& adv : advertisements_) {
    adv->Stop(base::DoNothing(), base::DoNothing());
  }
  std::move(callback).Run();
}

void BluetoothAdapterFloss::ConnectDevice(
    const std::string& address,
    const std::optional<device::BluetoothDevice::AddressType>& address_type,
    ConnectDeviceCallback callback,
    ConnectDeviceErrorCallback error_callback) {
  // On Floss, ACL and RFCOMM connection are done with
  // createRfcommSocketToServiceRecord(UUID). This should be called after
  // ConnectDevice. Since all that is required on Floss for insecure connection
  // is an address, this function currently just creates a device pointer.
  // TODO(b/269500327): This behavior is actually a better design. We should
  // rename this function to CreateDevice which does only device creation and
  // let the caller decide what to do with it (Connect, Pair, etc).
  BluetoothDeviceFloss* device_ptr;
  std::string canonical_address = device::CanonicalizeBluetoothAddress(address);

  if (base::Contains(devices_, canonical_address)) {
    device_ptr =
        static_cast<BluetoothDeviceFloss*>(devices_[canonical_address].get());
  } else {
    auto device = CreateBluetoothDeviceFloss(
        FlossDeviceId({.address = address, .name = ""}));
    device_ptr = device.get();
    devices_.emplace(canonical_address, std::move(device));
  }

  std::move(callback).Run(device_ptr);
}

void BluetoothAdapterFloss::AddLocalGattService(
    std::unique_ptr<BluetoothLocalGattServiceFloss> service) {
  DCHECK(!base::Contains(owned_gatt_services_, service->GetIdentifier()));
  owned_gatt_services_[service->GetIdentifier()] = std::move(service);
}

void BluetoothAdapterFloss::RemoveLocalGattService(
    BluetoothLocalGattServiceFloss* service) {
  auto service_iter = owned_gatt_services_.find(service->GetIdentifier());
  if (service_iter == owned_gatt_services_.end()) {
    BLUETOOTH_LOG(ERROR)
        << "Trying to remove service: " << service->GetIdentifier()
        << " from adapter: "
        << FlossDBusManager::Get()->GetAdapterClient()->GetObjectPath()->value()
        << " that doesn't own it.";
    return;
  }

  owned_gatt_services_.erase(service_iter);
}

device::BluetoothLocalGattService* BluetoothAdapterFloss::GetGattService(
    const std::string& identifier) const {
  const auto& service = owned_gatt_services_.find(identifier);
  return service == owned_gatt_services_.end() ? nullptr
                                               : service->second.get();
}

base::WeakPtr<device::BluetoothLocalGattService>
BluetoothAdapterFloss::CreateLocalGattService(
    const device::BluetoothUUID& uuid,
    bool is_primary,
    device::BluetoothLocalGattService::Delegate* delegate) {
  return floss::BluetoothLocalGattServiceFloss::Create(this, uuid, is_primary,
                                                       delegate);
}

void BluetoothAdapterFloss::RegisterGattService(
    BluetoothLocalGattServiceFloss* service) {
  FlossDBusManager::Get()->GetGattManagerClient()->AddService(
      base::BindOnce(&BluetoothAdapterFloss::OnGattServiceAdded,
                     weak_ptr_factory_.GetWeakPtr(), service),
      service->ToGattService());
}

void BluetoothAdapterFloss::OnGattServiceAdded(
    BluetoothLocalGattServiceFloss* service,
    DBusResult<Void> ret) {
  if (!ret.has_value()) {
    service->GattServerServiceAdded(GattStatus::kError,
                                    service->ToGattService());
  }
}

void BluetoothAdapterFloss::UnregisterGattService(
    BluetoothLocalGattServiceFloss* service) {
  FlossDBusManager::Get()->GetGattManagerClient()->RemoveService(
      base::BindOnce(&BluetoothAdapterFloss::OnGattServiceRemoved,
                     weak_ptr_factory_.GetWeakPtr(), service),
      service->InstanceId());
}

void BluetoothAdapterFloss::OnGattServiceRemoved(
    BluetoothLocalGattServiceFloss* service,
    DBusResult<Void> ret) {
  if (!ret.has_value()) {
    service->GattServerServiceRemoved(GattStatus::kError,
                                      service->InstanceId());
  }
}

bool BluetoothAdapterFloss::SendValueChanged(
    BluetoothLocalGattCharacteristicFloss* characteristic,
    const std::vector<uint8_t>& value) {
  if (!characteristic->GetService()->IsRegistered()) {
    return false;
  }

  bool confirm =
      characteristic->CccdNotificationType() ==
      device::BluetoothLocalGattCharacteristic::NotificationType::kIndication;
  std::string service_name =
      FlossDBusManager::Get()->GetGattManagerClient()->ServiceName();
  FlossDBusManager::Get()->GetGattManagerClient()->ServerSendNotification(
      base::DoNothing(), service_name, characteristic->InstanceId(), confirm,
      value);
  return true;
}

#if BUILDFLAG(IS_CHROMEOS)
void BluetoothAdapterFloss::SetServiceAllowList(const UUIDList& uuids,
                                                base::OnceClosure callback,
                                                ErrorCallback error_callback) {
  FlossDBusManager::Get()->GetAdminClient()->SetAllowedServices(
      base::BindOnce(&BluetoothAdapterFloss::OnMethodResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(error_callback)),
      uuids);
}

std::unique_ptr<device::BluetoothLowEnergyScanSession>
BluetoothAdapterFloss::StartLowEnergyScanSession(
    std::unique_ptr<device::BluetoothLowEnergyScanFilter> filter,
    base::WeakPtr<device::BluetoothLowEnergyScanSession::Delegate> delegate) {
  auto scan_session = std::make_unique<BluetoothLowEnergyScanSessionFloss>(
      std::move(filter), delegate,
      base::BindOnce(&BluetoothAdapterFloss::OnLowEnergyScanSessionDestroyed,
                     weak_ptr_factory_.GetWeakPtr()));
  FlossDBusManager::Get()->GetLEScanClient()->RegisterScanner(base::BindOnce(
      &BluetoothAdapterFloss::OnRegisterScanner, weak_ptr_factory_.GetWeakPtr(),
      scan_session->GetWeakPtr()));
  return scan_session;
}

device::BluetoothAdapter::LowEnergyScanSessionHardwareOffloadingStatus
BluetoothAdapterFloss::GetLowEnergyScanSessionHardwareOffloadingStatus() {
  if (!IsPowered()) {
    BLUETOOTH_LOG(ERROR)
        << "GetLowEnergyScanSessionHardwareOffloadingStatus called when "
        << "adapter is not powered.";
    return device::BluetoothAdapter::
        LowEnergyScanSessionHardwareOffloadingStatus::kUndetermined;
  }
  return FlossDBusManager::Get()->GetGattManagerClient()->GetMsftSupported()
             ? device::BluetoothAdapter::
                   LowEnergyScanSessionHardwareOffloadingStatus::kSupported
             : device::BluetoothAdapter::
                   LowEnergyScanSessionHardwareOffloadingStatus::kNotSupported;
}

std::vector<device::BluetoothAdapter::BluetoothRole>
BluetoothAdapterFloss::GetSupportedRoles() {
  std::vector<BluetoothAdapter::BluetoothRole> roles;

  if (!IsPresent()) {
    return roles;
  }

  for (auto role :
       FlossDBusManager::Get()->GetAdapterClient()->GetSupportedRoles()) {
    switch (role) {
      case FlossAdapterClient::BtAdapterRole::kCentral:
        roles.push_back(BluetoothAdapter::BluetoothRole::kCentral);
        break;
      case FlossAdapterClient::BtAdapterRole::kPeripheral:
        roles.push_back(BluetoothAdapter::BluetoothRole::kPeripheral);
        break;
      case FlossAdapterClient::BtAdapterRole::kCentralPeripheral:
        roles.push_back(BluetoothAdapter::BluetoothRole::kCentralPeripheral);
        break;
      default:
        BLUETOOTH_LOG(EVENT)
            << __func__ << ": Unknown role: " << static_cast<uint32_t>(role);
    }
  }

  return roles;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
void BluetoothAdapterFloss::SetStandardChromeOSAdapterName() {
  if (!IsPresent()) {
    BLUETOOTH_LOG(ERROR)
        << "SetStandardChromeOSAdapterName called when adapter is not present.";
    return;
  }

  std::string alias = ash::GetDeviceBluetoothName(GetAddress());
  FlossDBusManager::Get()->GetAdapterClient()->SetName(base::DoNothing(),
                                                       alias);
}

void BluetoothAdapterFloss::ConfigureBluetoothTelephony(bool enabled) {
  FlossDBusManager::Get()->GetBluetoothTelephonyClient()->SetPhoneOpsEnabled(
      base::DoNothing(), enabled);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

void BluetoothAdapterFloss::ScannerRegistered(device::BluetoothUUID uuid,
                                              uint8_t scanner_id,
                                              GattStatus status) {
  BLUETOOTH_LOG(EVENT) << "Scanner registered with UUID = " << uuid
                       << ", scanner id = " << static_cast<int>(scanner_id)
                       << ", status = " << static_cast<int>(status);

  if (!base::Contains(scanners_, uuid)) {
    VLOG(1) << "ScannerRegistered but no longer exists " << uuid;
    return;
  }

  if (status != GattStatus::kSuccess) {
    BLUETOOTH_LOG(ERROR) << "Error registering scanner " << uuid
                         << ", status: " << static_cast<int>(status);
    scanners_[uuid]->OnActivate(scanner_id, /*success=*/false);
    return;
  }

  FlossDBusManager::Get()->GetLEScanClient()->StartScan(
      base::BindOnce(&BluetoothAdapterFloss::OnStartScan,
                     weak_ptr_factory_.GetWeakPtr(), uuid, scanner_id),
      scanner_id, std::nullopt, scanners_[uuid]->GetFlossScanFilter());
}

void BluetoothAdapterFloss::ScanResultReceived(ScanResult scan_result) {
  BLUETOOTH_LOG(DEBUG) << __func__ << ": " << scan_result.address;

  bool already_found = base::Contains(
      devices_, device::CanonicalizeBluetoothAddress(scan_result.address));

  BluetoothDeviceFloss* device_ptr =
      CreateOrGetDeviceForUpdate(scan_result.address, scan_result.name);

  std::vector<device::BluetoothUUID> service_uuids = scan_result.service_uuids;
  device::BluetoothDevice::ServiceDataMap service_data_map;
  for (const auto& [uuid_str, bytes] : scan_result.service_data) {
    auto uuid = device::BluetoothUUID(uuid_str);
    service_uuids.push_back(uuid);
    service_data_map[uuid] = bytes;
  }

  device::BluetoothDevice::ManufacturerDataMap manufacturer_data_map(
      scan_result.manufacturer_data.begin(),
      scan_result.manufacturer_data.end());
  device_ptr->UpdateAdvertisementData(scan_result.rssi, scan_result.flags,
                                      service_uuids, scan_result.tx_power,
                                      service_data_map, manufacturer_data_map);

  for (auto& observer : observers_) {
    observer.DeviceAdvertisementReceived(this, device_ptr, scan_result.rssi,
                                         scan_result.adv_data);
    observer.DeviceAdvertisementReceived(
        scan_result.address, /*device_name=*/device_ptr->GetName(),
        /*advertisement_name=*/scan_result.name, scan_result.rssi,
        scan_result.tx_power, device_ptr->GetAppearance(), service_uuids,
        service_data_map, manufacturer_data_map);
  }

  // Update properties and emit a |DeviceFound| if newly found.
  // Also explicitly call |DeviceChanged| if already found since
  // |UpdateDeviceProperties| doesn't always emit |DeviceChanged|.
  UpdateDeviceProperties(false, device_ptr->AsFlossDeviceId());
  if (already_found) {
    NotifyDeviceChanged(device_ptr);
  }
}

void BluetoothAdapterFloss::AdvertisementFound(uint8_t scanner_id,
                                               ScanResult scan_result) {
  BLUETOOTH_LOG(DEBUG) << __func__ << ": " << scan_result.address;

  CreateOrGetDeviceForUpdate(scan_result.address, scan_result.name);

  // MSFT event does not arrive together with the advertisement data, but they
  // always arrive very close to each other.
  // Ideally Floss daemon should consolidate the advertisement data into the
  // AdvertisementFound callback, but for now the workaround is to delay
  // notifying client for a little bit to practically be sure that the client
  // will have updated the advertisement data by the time they hear
  // OnDeviceFound.
  // TODO(b/271165074): Fix this by combining AdvertisementFound with the first
  // advertisement data.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&BluetoothAdapterFloss::NotifyDeviceFound,
                     weak_ptr_factory_.GetWeakPtr(), scanner_id,
                     device::CanonicalizeBluetoothAddress(scan_result.address)),
      base::Seconds(1));
}

void BluetoothAdapterFloss::AdvertisementLost(uint8_t scanner_id,
                                              ScanResult scan_result) {
  BLUETOOTH_LOG(DEBUG) << __func__ << ": " << scan_result.address;

  auto device = CreateBluetoothDeviceFloss(FlossDeviceId(
      {.address = scan_result.address, .name = scan_result.name}));
  std::string canonical_address =
      device::CanonicalizeBluetoothAddress(device->GetAddress());
  if (!base::Contains(devices_, canonical_address)) {
    BLUETOOTH_LOG(EVENT) << __func__
                         << ": Device lost but never previously found: "
                         << scan_result.address;
    return;
  }

  BluetoothDeviceFloss* device_ptr = device.get();
  for (const auto& [key, scanner] : scanners_) {
    if (scanner->GetScannerId() == scanner_id) {
      scanner->OnDeviceLost(device_ptr);
    }
  }
}

void BluetoothAdapterFloss::RemovePairingDelegateInternal(
    device::BluetoothDevice::PairingDelegate* pairing_delegate) {
  // Check if any device is using the pairing delegate.
  // If so, clear the pairing context which will make any responses no-ops.
  for (auto& [_, device] : devices_) {
    BluetoothDeviceFloss* device_floss =
        static_cast<BluetoothDeviceFloss*>(device.get());

    BluetoothPairingFloss* pairing = device_floss->pairing();
    if (pairing && pairing->pairing_delegate() == pairing_delegate) {
      BLUETOOTH_LOG(DEBUG) << __func__ << ": " << device_floss->GetAddress();
      device_floss->ResetPairing();
    }
  }
}

base::WeakPtr<device::BluetoothAdapter> BluetoothAdapterFloss::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool BluetoothAdapterFloss::SetPoweredImpl(bool powered) {
  NOTREACHED();
}

void BluetoothAdapterFloss::StartScanWithFilter(
    std::unique_ptr<device::BluetoothDiscoveryFilter> discovery_filter,
    DiscoverySessionResultCallback callback) {
  // Also return ADAPTER_NOT_PRESENT if not powered.
  // TODO(b/193839304) - !IsPowered should return ADAPTER_NOT_POWERED
  if (!IsPresent() || !IsPowered()) {
    std::move(callback).Run(
        true, UMABluetoothDiscoverySessionOutcome::ADAPTER_NOT_PRESENT);
    return;
  }

  BLUETOOTH_LOG(EVENT) << __func__;

#if BUILDFLAG(IS_CHROMEOS)
  // First start LE scan before discovery
  if (!le_discovery_session_) {
    le_discovery_session_ = StartLowEnergyScanSession(
        nullptr, static_cast<BleDelegateForDiscovery*>(
                     le_discovery_session_delegate_.get())
                     ->GetWeakPtr());
  }
#endif

  // TODO(b/192251662) - Support scan filtering. For now, start scanning with no
  // filters in place.
  FlossDBusManager::Get()->GetAdapterClient()->StartDiscovery(
      base::BindOnce(&BluetoothAdapterFloss::OnStartDiscovery,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void BluetoothAdapterFloss::UpdateFilter(
    std::unique_ptr<device::BluetoothDiscoveryFilter> discovery_filter,
    DiscoverySessionResultCallback callback) {
  // Also return ADAPTER_NOT_PRESENT if not powered.
  // TODO(b/193839304) - !IsPowered should return ADAPTER_NOT_POWERED
  if (!IsPresent() || !IsPowered()) {
    std::move(callback).Run(
        true, UMABluetoothDiscoverySessionOutcome::ADAPTER_NOT_PRESENT);
    return;
  }

  BLUETOOTH_LOG(EVENT) << __func__;

  // TODO(b/192251662) - Support scan filtering. For now, always return success
  std::move(callback).Run(false, UMABluetoothDiscoverySessionOutcome::SUCCESS);
}

void BluetoothAdapterFloss::StopScan(DiscoverySessionResultCallback callback) {
  // Also return ADAPTER_NOT_PRESENT if not powered.
  // TODO(b/193839304) - !IsPowered should return ADAPTER_NOT_POWERED
  if (!IsPresent() || !IsPowered()) {
    std::move(callback).Run(
        /*is_error= */ false,
        UMABluetoothDiscoverySessionOutcome::ADAPTER_NOT_PRESENT);
    return;
  }

  BLUETOOTH_LOG(EVENT) << __func__;

  DCHECK_EQ(NumDiscoverySessions(), 0);
  FlossDBusManager::Get()->GetAdapterClient()->CancelDiscovery(
      base::BindOnce(&BluetoothAdapterFloss::OnStopDiscovery,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));

  // Deleting the scan session will stop any active scanning.
  le_discovery_session_.reset(nullptr);
}

void BluetoothAdapterFloss::OnRegisterScanner(
    base::WeakPtr<BluetoothLowEnergyScanSessionFloss> scan_session,
    DBusResult<device::BluetoothUUID> ret) {
  if (!scan_session) {
    BLUETOOTH_LOG(ERROR)
        << "Scan session removed before registration completed.";
    return;
  }
  if (!ret.has_value() || ret.value().canonical_value() == kEmptyUuidStr) {
    BLUETOOTH_LOG(ERROR) << "Failed RegisterScanner.";
    scan_session->OnRelease();
    return;
  }
  scan_session->OnRegistered(ret.value());
  scanners_[ret.value()] = scan_session;
  BLUETOOTH_LOG(EVENT) << "Registering scanner " << ret.value();
}

void BluetoothAdapterFloss::OnStartScan(
    device::BluetoothUUID uuid,
    uint8_t scanner_id,
    DBusResult<FlossDBusClient::BtifStatus> ret) {
  if (!base::Contains(scanners_, uuid)) {
    VLOG(1) << "Started scanning but scanner no longer exists " << uuid;
    return;
  }

  if (!ret.has_value() ||
      ret.value() != FlossDBusClient::BtifStatus::kSuccess) {
    if (ret.has_value()) {
      BLUETOOTH_LOG(ERROR) << "Failed StartScan, status: "
                           << static_cast<uint32_t>(ret.value());
    } else {
      BLUETOOTH_LOG(ERROR) << "Failed StartScan, D-Bus error: " << ret.error();
    }
    scanners_[uuid]->OnActivate(scanner_id, /*success=*/false);
    return;
  }

  BLUETOOTH_LOG(EVENT) << "OnStartScan succeeded";
  scanners_[uuid]->OnActivate(scanner_id, /*success=*/true);
}

void BluetoothAdapterFloss::OnLowEnergyScanSessionDestroyed(
    const std::string& uuid_str) {
  BLUETOOTH_LOG(EVENT) << __func__ << ": UUID = " << uuid_str;

  device::BluetoothUUID uuid = device::BluetoothUUID(uuid_str);
  if (!base::Contains(scanners_, uuid)) {
    return;
  }

  uint8_t scanner_id = scanners_[uuid]->GetScannerId();
  scanners_.erase(uuid);

  if (IsPowered()) {
    FlossDBusManager::Get()->GetLEScanClient()->UnregisterScanner(
        base::BindOnce(&BluetoothAdapterFloss::OnUnregisterScanner,
                       weak_ptr_factory_.GetWeakPtr(), scanner_id),
        scanner_id);
  }
}

void BluetoothAdapterFloss::OnUnregisterScanner(uint8_t scanner_id,
                                                DBusResult<bool> ret) {
  BLUETOOTH_LOG(EVENT) << __func__
                       << ": scanner_id = " << static_cast<int>(scanner_id);

  if (!ret.has_value()) {
    BLUETOOTH_LOG(ERROR) << "Failed UnregisterScanner: " << ret.error();
  }
}

}  // namespace floss
